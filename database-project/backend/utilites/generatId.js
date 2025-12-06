const crypto = require("crypto");
const fs = require("fs");
const path = require("path");


const usersFile = path.join("data", "users.json");

console.log("this is the user files",usersFile)


function generateObjectId() {
  return crypto.randomBytes(12).toString("hex"); 
}



// Ensure ID doesn’t already exist in users.json
function generateUniqueId() {
  let id;
  let users = [];

  try {
    const data = fs.readFileSync(usersFile, "utf8");
    console.log("created user data",data)
    users = JSON.parse(data || "[]");
  } catch {
    users = [];
  }

  do {
    id = generateObjectId();
  } while (users.some((u) => u.id === id));

  return id;
}

module.exports = { generateUniqueId };
