const crypto = require("crypto");
const dotenv = require("dotenv");
const { sendCommand } = require("../engineClient");

dotenv.config();

const DB_HOST = process.env.DB_HOST || "localhost";
const DB_PORT = process.env.DB_PORT || "9999";

function buildConnectionURL({ username, password, dbName, userId }) {
  return `rdb://${username}:${password}@${DB_HOST}:${DB_PORT}/${dbName}?auth=${userId}`;
}

function normalizeEngineDatabases(engineRes) {
  if (!engineRes) return [];

  if (Array.isArray(engineRes)) {
    return engineRes.map(db => (typeof db === "string" ? { name: db } : db));
  }

  if (Array.isArray(engineRes.databases)) {
    return engineRes.databases.map(db => (typeof db === "string" ? { name: db } : db));
  }

  return [];
}

/* ---------------------------------------------------------
   PERMISSION CHECK (ENGINE METADATA)
--------------------------------------------------------- */
function hasPermission(engineMeta, userId, required) {
  const user = engineMeta.users?.find(u => u.userId === userId);
  if (!user) return false;

  if (user.role === "admin") return true;
  if (user.role === "dataManager" && required !== "deleteOnly") return true;
  if (user.role === "member" && required === "readOnly") return true;

  return false;
}

/* ---------------------------------------------------------
   SET DATABASE CREDENTIALS + CONNECTION URL
--------------------------------------------------------- */
async function setupDatabaseCredentials(req, res) {
  const { userId, dbName } = req.params;
  const { dbUsername, dbPassword } = req.body;

  if (!dbUsername || !dbPassword) {
    return res.status(400).json({ error: "dbUsername and dbPassword are required" });
  }

  const found = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { id: userId }
  });

  const user = found[0];
  if (!user) return res.status(404).json({ error: "User not found" });

  const databases = Array.isArray(user.databases) ? user.databases : [];
  let db = databases.find(d => d.name === dbName);

  if (!db) {
    db = { name: dbName };
    databases.push(db);
  }

  const token = crypto.randomBytes(16).toString("hex");

  const connectionURL = buildConnectionURL({
    username: dbUsername,
    password: dbPassword,
    dbName,
    userId
  });

  db.credentials = {
    username: dbUsername,
    password: dbPassword,
    token
  };
  db.connectionURL = connectionURL;

  await sendCommand({
    action: "updateOne",
    dbName: "system",
    collection: "users",
    filter: { id: userId },
    update: { $set: { databases } }
  });

  return res.json({
    message: "Database credentials saved successfully",
    connectionURL: db.connectionURL
  });
}

/* ---------------------------------------------------------
   CONNECT DATABASE (SDK)
--------------------------------------------------------- */
async function connectDatabase(req, res) {
  const { userId, dbName, schemaName } = req.params;

  const found = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { id: userId }
  });

  const user = found[0];
  if (!user) return res.status(404).json({ error: "User not found" });

  const db = (user.databases || []).find(d => d.name === dbName);
  if (!db) return res.status(404).json({ error: "Database not found" });

  if (!db.credentials)
    return res.status(400).json({ error: "Database credentials not set" });

  const connectionURL = db.connectionURL || buildConnectionURL({
    username: db.credentials.username,
    password: db.credentials.password,
    dbName,
    userId
  });

  const ydp = require("your-data-patner");
  const conn = ydp.connect(connectionURL);

  new ydp.model(schemaName, new ydp.Schema({}), conn);

  return res.json({
    message: "Connected to DB Successfully",
    connectionURL,
    usage: {
      insert: `conn.insert("${schemaName}", {name:'User'})`,
      read:   `conn.read("${schemaName}")`,
      update: `conn.update("${schemaName}", id, updateData)`,
      delete: `conn.delete("${schemaName}", id)`
    }
  });
}

/* ---------------------------------------------------------
   CREATE DATABASE (ENGINE)
--------------------------------------------------------- */
async function askDatabasePermission(req, res) {
  const { userId } = req.params;
  const { dbName, sharedUsers } = req.body;

  if (!userId || !dbName)
    return res.status(400).json({ error: "User ID and database name required" });
  
  // ensure user workspace exists, then create the database
  await sendCommand({ action: "initUserSpace", userId });

  const engineRes = await sendCommand({
    action: "createDatabase",
    userId,
    dbName,
    sharedUsers: sharedUsers || []
  });

  return res.json(engineRes);
}

/* ---------------------------------------------------------
   ADD SCHEMA / COLLECTION
--------------------------------------------------------- */
async function addSchema(req, res) {
  const { userId, dbName } = req.params;
  const { schemaName } = req.body;

  if (!schemaName)
    return res.status(400).json({ error: "Schema name required" });

  const engineRes = await sendCommand({
    action: "createCollection",
    userId,
    dbName,
    collection: schemaName
  });

  res.json(engineRes);
}

/* ---------------------------------------------------------
   LIST USER DATABASES (ENGINE)
--------------------------------------------------------- */
async function listUserDatabases(req, res) {
  const { userId } = req.params;
  try {
    const [engineRes, userRes] = await Promise.all([
      sendCommand({ action: "listDatabases", userId }),
      sendCommand({
        action: "find",
        dbName: "system",
        collection: "users",
        filter: { id: userId }
      })
    ]);

    const engineDbs = normalizeEngineDatabases(engineRes);
    const user = Array.isArray(userRes) ? userRes[0] : null;
    const userDbMap = new Map(
      (user?.databases || []).map(db => [db.name, db])
    );

    const merged = engineDbs.map(db => {
      const dbName = db.name;
      const meta = userDbMap.get(dbName) || {};
      const creds = meta.credentials;
      const connectionURL = creds
        ? buildConnectionURL({
            username: creds.username,
            password: creds.password,
            dbName,
            userId
          })
        : meta.connectionURL || null;

      return {
        ...db,
        name: dbName,
        connectionURL,
        credentialsSet: Boolean(creds),
        token: creds?.token || meta.token || null
      };
    });

    const orphanedUserDbs = [...userDbMap.values()]
      .filter(db => !engineDbs.find(edb => edb.name === db.name))
      .map(db => ({
        name: db.name,
        connectionURL: db.connectionURL || null,
        credentialsSet: Boolean(db.credentials),
        token: db.credentials?.token || null,
        status: "not-found-in-engine"
      }));

    return res.json({
      databases: [...merged, ...orphanedUserDbs],
      total: merged.length + orphanedUserDbs.length
    });
  } catch (err) {
    console.error("[listUserDatabases] error", err);
    return res.status(500).json({ error: "Failed to list databases" });
  }
}

/* ---------------------------------------------------------
   INSERT DATA
--------------------------------------------------------- */
async function addData(req, res) {
  const { userId, dbName, schemaName } = req.params;

  const payload = Object.assign({}, req.body);
  if (!payload.id) payload.id = String(Date.now());

  const engineRes = await sendCommand({
    action: "insert",
    userId,
    dbName,
    collection: schemaName,
    data: payload
  });
  res.json({ ...engineRes, insertedId: payload.id });
}

/* ---------------------------------------------------------
   UPDATE DATA
--------------------------------------------------------- */
async function updateData(req, res) {
  const { userId, dbName, schemaName, dataId } = req.params;

  const engineRes = await sendCommand({
    action: "updateOne",
    userId,
    dbName,
    collection: schemaName,
    filter: { id: String(dataId) },
    update: req.body
  });

  res.json(engineRes);
}

/* ---------------------------------------------------------
   DELETE DATA
--------------------------------------------------------- */
async function deleteData(req, res) {
  const { userId, dbName, schemaName, dataId } = req.params;

  const engineRes = await sendCommand({
    action: "deleteOne",
    userId,
    dbName,
    collection: schemaName,
    filter: { id: String(dataId) }
  });

  res.json(engineRes);
}

/* ---------------------------------------------------------
   GET DATA
--------------------------------------------------------- */
async function getData(req, res) {
  const { userId, dbName, schemaName } = req.params;

  const engineRes = await sendCommand({
    action: "find",
    userId,
    dbName,
    collection: schemaName,
    filter: req.query || {}
  });

  res.json(engineRes);
}

module.exports = {
  askDatabasePermission,
  addSchema,
  listUserDatabases,
  setupDatabaseCredentials,
  addData,
  updateData,
  deleteData,
  getData,
  connectDatabase
};
