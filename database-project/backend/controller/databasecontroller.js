const crypto = require("crypto");
const { sendCommand } = require("../engineClient");

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

  db.credentials = {
    username: dbUsername,
    password: dbPassword,
    token
  };

  db.connectionURL = `rdb://${dbUsername}:${dbPassword}@localhost:9999/${dbName}?auth=${userId}`;

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

  const ydp = require("your-data-patner");
  const conn = ydp.connect(db.connectionURL);

  new ydp.model(schemaName, new ydp.Schema({}), conn);

  return res.json({
    message: "Connected to DB Successfully",
    connectionURL: db.connectionURL,
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

  const engineRes = await sendCommand({
    action: "listDatabases",
    userId
  });

  res.json(engineRes);
}

/* ---------------------------------------------------------
   INSERT DATA
--------------------------------------------------------- */
async function addData(req, res) {
  const { userId, dbName, schemaName } = req.params;

  const engineRes = await sendCommand({
    action: "insert",
    userId,
    dbName,
    collection: schemaName,
    data: req.body
  });

  res.json(engineRes);
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
    filter: { id: Number(dataId) },
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
    filter: { id: Number(dataId) }
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
