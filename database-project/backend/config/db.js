const fs = require("fs-extra");
const path = require("path");

const usersFile = path.join(__dirname, "../data/users.json");

console.log("the file had been connected",usersFile)

async function readUsers() {
  try {
    const data = await fs.readFile(usersFile, "utf8");
    console.log("this is the data",data)
    return JSON.parse(data || "[]");
  } catch (err) {
    console.error("Error reading users file:", err);
    return [];
  }
}

async function writeUsers(users) {
  await fs.writeFile(usersFile, JSON.stringify(users, null, 2));
}

module.exports = { readUsers, writeUsers };
