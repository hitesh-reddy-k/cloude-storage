const fs = require("fs-extra");
const path = require("path");

const logsFile = path.join(__dirname, "../data/logs.json");

async function readLogs() {
  try {
    if (!(await fs.pathExists(logsFile))) {
      await fs.outputJson(logsFile, []);
    }
    const data = await fs.readFile(logsFile, "utf8");
    console.log(data) 
    return JSON.parse(data || "[]");
  } catch (err) {
    console.error("Error reading logs:", err);
    return [];
  }
}

async function writeLogs(logs) {
  try {
    await fs.outputJson(logsFile, logs, { spaces: 2 });
  } catch (err) {
    console.error("Error writing logs:", err);
  }
}

module.exports = { readLogs, writeLogs };
