const express = require("express");
const dotenv = require("dotenv");
const net = require("net");

const userRoutes = require("./router/userrouter");
const databaseRoutes = require("./router/databaseRoutes");
const publicRoutes = require("./router/publicRoutes");

dotenv.config();

const app = express();
const PORT = 9999;

// ---------------------------
// MIDDLEWARE
// ---------------------------
app.use(express.json());

// ---------------------------
// ENGINE HEALTH CHECK
// ---------------------------
function checkEngineConnection() {
  return new Promise((resolve, reject) => {
    const socket = new net.Socket();

    socket.connect(9000, "127.0.0.1", () => {
      socket.write(JSON.stringify({ action: "ping" }));
    });

    socket.on("data", data => {
      const res = data.toString();
      socket.destroy();
      resolve(res);
    });

    socket.on("error", err => {
      reject(err);
    });
  });
}

// ---------------------------
// ROUTES (UNCHANGED)
// ---------------------------
app.get("/", (req, res) => {
  res.send("🚀 API Gateway Running");
});

app.use("/user", userRoutes);
app.use("/database", databaseRoutes);
app.use("/public", publicRoutes);

// ---------------------------
// SERVER START
// ---------------------------
async function startServer() {
  console.log("🔄 Connecting to Database Engine...");

  try {
    await checkEngineConnection();
    console.log("✅ Database Engine Connected");

    app.listen(PORT, () => {
      console.log(`🌐 API Server running at http://localhost:${PORT}`);
    });

  } catch (err) {

    console.error("❌ Database Engine not running");
    console.error("➡️ Start C++ engine first on port 9000");
    process.exit(1);
  }
}

startServer();
