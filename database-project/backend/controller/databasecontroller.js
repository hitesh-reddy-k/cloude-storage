const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const { createDatabase, createSchema } = require("../config/dbmanager");
const { readUsers } = require("../config/db");
const { readDatabase } = require("../config/dbmanager");

const USERS_DB = path.join(__dirname, "../data/users.json");
const DB_PATH = path.join(__dirname, "../data/datdabases");

/* SAVE USERS JSON */
function saveUsers(users) {
  fs.writeFileSync(USERS_DB, JSON.stringify(users, null, 2));
  console.log("\n🔹 USERS UPDATED & SAVED TO FILE");
}


function saveDB(dbName, data) {
  fs.writeFileSync(path.join(DB_PATH, `${dbName}.json`), JSON.stringify(data, null, 2));
}


//function to check if user has required permission


function hasPermission(dbData, userId, required) {
  const user = dbData.users.find(u => u.userId === userId);
  if (!user) return false;

  if (user.role === "admin") return true; // full access
  if (user.role === "dataManager" && required !== "deleteOnly") return true;
  if (user.role === "member" && required === "readOnly") return true;

  return false;
}


/* ---------------------------------------------------------
   SET DATABASE CREDENTIALS + GENERATE CONNECTION URL
--------------------------------------------------------- */
async function setupDatabaseCredentials(req, res) {
  console.log("\n==== SETUP DATABASE CREDENTIALS HIT ====");
  console.log("Params:", req.params);
  console.log("Body:", req.body);

  const { userId, dbName } = req.params;
  const { dbUsername, dbPassword } = req.body;

  if (!dbUsername || !dbPassword) {
    return res.status(400).json({ error: "dbUsername and dbPassword are required" });
  }

  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) return res.status(404).json({ error: "User not found" });

  const db = user.databases?.find(d => d.name === dbName);
  if (!db) return res.status(404).json({ error: "Database not found for this user" });

  console.log("\n🔹 Storing DB Credentials...");

  // Generate 16-char token
  const token = crypto.randomBytes(16).toString("hex");

  // Save credentials
  db.credentials = {
    username: dbUsername,
    password: dbPassword,
    token // Save only the token
  };

  // Combine userId + token for full 40-char authKey
  db.connectionURL = `rdb://${dbUsername}:${dbPassword}@localhost:9999/${dbName}?auth=${userId}`;

  // Save back to users JSON
  saveUsers(users);

  console.log("\n🟢 CONNECTION URL GENERATED:");
  console.log(db.connectionURL);

  return res.json({
    message: "Database credentials saved successfully",
    connectionURL: db.connectionURL
  });
}


async function connectDatabase(req, res) {
  const { userId, dbName, schemaName } = req.params;
  // LOAD USERS
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) return res.status(404).json({ error: "User not found" });

  // FIND DB
  const db = user.databases.find(d => d.name === dbName);
  if (!db) return res.status(404).json({ error: "Database not found" });

  if (!db.credentials || !db.credentials.username)
    return res.status(400).json({ error: "Database credentials not set" });

  const connectionURL = db.connectionURL;
  console.log("\n🔗 FETCHED CONNECTION URL:", connectionURL);

  // Dynamic client SDK load
  const ydp = require("your-data-patner");
  const conn = ydp.connect(connectionURL); // <-- automatic connect 🔥

  // Model ready for CRUD instantly
  const Model = new ydp.model(schemaName, new ydp.Schema({}), conn);

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
   CREATE DATABASE & PERMISSION SYSTEM
--------------------------------------------------------- */
async function askDatabasePermission(req, res) {
  console.log("\n==== CREATE / REQUEST DATABASE PERMISSION ====");
  console.log("Request Body:", req.body);

  const { userId } = req.params;
  const { dbName, sharedUsers } = req.body;

  if (!userId || !dbName) {
    console.log("❌ Missing Required Fields");
    return res.status(400).json({ error: "User ID and database name required" });
  }

  const users = await readUsers();
  console.log("📁 Total Users Found:", users.length);

  const invalidUsers = (sharedUsers || []).filter(
    u => !users.find(usr => usr.id === u.userId)
  );

  if (invalidUsers.length > 0) {
    console.log("❌ INVALID ACCESS USERS:", invalidUsers);
    return res.status(400).json({
      error: "Some shared users do not exist",
      invalidUsers
    });
  }

  try {
    console.log("\n⚙ Creating Database...");
    const dbInfo = await createDatabase(userId, dbName, sharedUsers || []);
    console.log("🟢 New Database Created:", dbInfo);

    return res.json({
      message: "Database created successfully",
      database: dbInfo
    });

  } catch (err) {
    console.log("❌ ERROR CREATING DB:", err.message);
    return res.status(500).json({ error: err.message });
  }
}



/* ---------------------------------------------------------
   ADD NEW SCHEMA
--------------------------------------------------------- */
async function addSchema(req, res) {
  console.log("\n==== ADD SCHEMA API CALLED ====");
  console.log("Params:", req.params);
  console.log("Body:", req.body);

  const { userId, dbName } = req.params;
  const { schemaName } = req.body;

  if (!schemaName) {
    console.log("❌ Missing Schema Name");
    return res.status(400).json({ error: "Schema name required" });
  }

  try {
    console.log("\n⚙ Creating Schema...");
    const result = await createSchema(userId, dbName, schemaName);

    console.log("🟢 New Schema Added:", result);
    return res.json(result);

  } catch (err) {
    console.log("❌ ERROR ADDING SCHEMA:", err.message);
    return res.status(500).json({ error: err.message });
  }
}



/* ---------------------------------------------------------
   LIST USER DATABASES
--------------------------------------------------------- */
async function listUserDatabases(req, res) {
  console.log("\n==== LIST USER DATABASES ====");
  console.log("Params:", req.params);

  const { userId } = req.params;
  const users = await readUsers();

  const user = users.find(u => u.id === userId);
  if (!user) {
    console.log("❌ USER NOT FOUND");
    return res.status(404).json({ error: "User not found" });
  }

  console.log("📄 DATABASE LIST:", user.databases);
  return res.json({ databases: user.databases || [] });
}


async function addData(req, res) {
  const { userId, dbName, schemaName } = req.params;
  const newData = req.body;

  const dbData = await readDatabase(dbName);
  if (!dbData) return res.status(404).json({ error: "Database not found" });

  if (!hasPermission(dbData, userId, "write"))
    return res.status(403).json({ error: "Access denied — Add not allowed" });

  dbData.schemas[schemaName].push({ id: Date.now(), ...newData });
  saveDB(dbName, dbData);

  return res.json({ message: "Data added successfully", data: newData });
}


async function updateData(req, res) {
  const { userId, dbName, schemaName, dataId } = req.params;
  const updates = req.body;

  const dbData = await readDatabase(dbName);
  if (!dbData) return res.status(404).json({ error: "Database not found" });

  if (!hasPermission(dbData, userId, "write"))
    return res.status(403).json({ error: "Access denied — Update not allowed" });

  let record = dbData.schemas[schemaName].find(d => d.id == dataId);
  if (!record) return res.status(404).json({ error: "Record not found" });

  Object.assign(record, updates);
  saveDB(dbName, dbData);

  return res.json({ message: "Data updated", record });
}


async function deleteData(req, res) {
  const { userId, dbName, schemaName, dataId } = req.params;

  const dbData = await readDatabase(dbName);
  if (!dbData) return res.status(404).json({ error: "Database not found" });

  if (!hasPermission(dbData, userId, "deleteOnly"))
    return res.status(403).json({ error: "Delete access denied" });

  dbData.schemas[schemaName] = dbData.schemas[schemaName].filter(d => d.id != dataId);
  saveDB(dbName, dbData);

  return res.json({ message: "Record deleted successfully" });
}


async function getData(req, res) {
  const { userId, dbName, schemaName } = req.params;

  const dbData = await readDatabase(dbName);
  if (!dbData) return res.status(404).json({ error: "Database not found" });

  if (!hasPermission(dbData, userId, "readOnly"))
    return res.status(403).json({ error: "Read access denied" });

  return res.json(dbData.schemas[schemaName]);
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
