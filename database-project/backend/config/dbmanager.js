const fs = require("fs");
const path = require("path");
const { writeUsers, readUsers } = require("../config/db");

const BASE_DB_PATH = path.join(__dirname, "../data/databases");

/* CREATE FOLDERS IF NOT EXISTS */
function ensureDir(dir) {
  if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true });
}

/* READ SCHEMA DATA */
function readSchema(dbPath, schemaName) {
  const file = path.join(dbPath, `${schemaName}.json`);
  if (!fs.existsSync(file)) return [];
  return JSON.parse(fs.readFileSync(file, "utf-8"));
}

/* WRITE SCHEMA DATA */
function writeSchema(dbPath, schemaName, data) {
  const file = path.join(dbPath, `${schemaName}.json`);
  fs.writeFileSync(file, JSON.stringify(data, null, 2));
}

/* PERMISSION VALIDATION */
function validatePermission(dbMeta, userId, accessType) {
  if (dbMeta.permissions.owner === userId) return true; // 🟢 full access

  const shared = dbMeta.permissions.sharedWith.find(u => u.userId === userId);
  if (!shared) return false;

  if (accessType === "view") return true;
  if (accessType === "edit" && ["admin", "dataManager"].includes(shared.role)) return true;
  if (accessType === "delete" && shared.role === "admin") return true;

  return false;
}


/* ================================================================
   📌 CREATE DATABASE
================================================================ */
async function createDatabase(userId, dbName, usersWithAccess = []) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User not found");

  const userDbPath = path.join(BASE_DB_PATH, userId);
  const dbPath = path.join(userDbPath, dbName);

  ensureDir(userDbPath);
  ensureDir(dbPath);

  const dbMeta = {
    name: dbName,
    path: dbPath,
    createdAt: new Date().toISOString(),
    permissions: {
      owner: userId,
      sharedWith: usersWithAccess // ➤ [{userId, role:"member/dataManager/admin"}]
    },
    schemas: []
  };

  if (!user.databases) user.databases = [];
  user.databases.push(dbMeta);

  await writeUsers(users);
  return dbMeta;
}


/* ================================================================
   🧾 CREATE SCHEMA / COLLECTION
================================================================ */
async function createSchema(userId, dbName, schemaName) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User not found");

  const db = user.databases.find(d => d.name === dbName);
  if (!db) throw new Error("Database not found");

  const schemaPath = path.join(db.path, `${schemaName}.json`);
  if (!fs.existsSync(schemaPath)) fs.writeFileSync(schemaPath, JSON.stringify([], null, 2));

  db.schemas.push({
    name: schemaName,
    file: `${schemaName}.json`,
    createdAt: new Date().toISOString()
  });

  await writeUsers(users);
  return { message: "Schema created successfully", schemaName };
}


/* ================================================================
   ➕ INSERT (CREATE DATA)
================================================================ */
async function insertData(userId, dbName, schemaName, data) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User does not exist");

  const db = user.databases.find(d => d.name === dbName);
  if (!db) throw new Error("DB not found");

  if (!validatePermission(db, userId, "edit"))
    throw new Error("Permission denied: Write access required");

  let rows = readSchema(db.path, schemaName);
  data.id = Date.now(); // Auto–ID
  rows.push(data);

  writeSchema(db.path, schemaName, rows);
  return { message: "Data inserted", inserted: data };
}


/* ================================================================
  ✏ UPDATE DATA
================================================================ */
async function updateData(userId, dbName, schemaName, recordId, values) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User not found");

  const db = user.databases.find(d => d.name === dbName);
  if (!db) throw new Error("Database not found");

  if (!validatePermission(db, userId, "edit"))
    throw new Error("Permission denied: Edit access required");

  let rows = readSchema(db.path, schemaName);
  let row = rows.find(r => r.id == recordId);
  if (!row) throw new Error("Record not found");

  Object.assign(row, values);
  writeSchema(db.path, schemaName, rows);

  return { message: "Record updated", updated: row };
}


/* ================================================================
   ❌ DELETE DATA
================================================================ */
async function deleteData(userId, dbName, schemaName, recordId) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User not found");

  const db = user.databases.find(d => d.name === dbName);
  if (!db) throw new Error("Database not found");

  if (!validatePermission(db, userId, "delete"))
    throw new Error("Permission denied: Admin required to delete");

  let rows = readSchema(db.path, schemaName);
  rows = rows.filter(r => r.id != recordId);

  writeSchema(db.path, schemaName, rows);
  return { message: "Record deleted", id: recordId };
}


/* ================================================================
   🔍 READ (VIEW DATA)
================================================================ */
async function readData(userId, dbName, schemaName) {
  const users = await readUsers();
  const user = users.find(u => u.id === userId);
  if (!user) throw new Error("User not found");

  const db = user.databases.find(d => d.name === dbName);
  if (!db) throw new Error("Database not found");

  if (!validatePermission(db, userId, "view"))
    throw new Error("Permission denied: Read only");

  return readSchema(db.path, schemaName);
}


module.exports = {
  createDatabase,
  createSchema,
  insertData,
  updateData,
  deleteData,
  readData
};
