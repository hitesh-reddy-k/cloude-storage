const bcrypt = require("bcrypt");
const jwt = require("jsonwebtoken");
const dotenv = require("dotenv");
const { sendCommand } = require("../engineClient");
const { jwtSecret, jwtExpire } = require("../config/jwtConfig");
const useragent = require("useragent");
const geoip = require("geoip-lite");

dotenv.config();

/* =====================================================
   REGISTER
===================================================== */
async function registerUser(req, res) {
  try {
    console.log("🟡 [REGISTER] Body:", req.body);

    const { username, email, password, role, phoneNumber } = req.body;
    if (!username || !email || !password) {
      return res.status(400).json({ error: "Username, email, password required" });
    }

    const exists = await sendCommand({
      action: "find",
      dbName: "system",
      collection: "users",
      filter: { $or: [{ email }, { username }, { phoneNumber }] }
    });

    if (exists.length) {
      console.log("🔴 [REGISTER] User already exists");
      return res.status(400).json({ error: "User already exists" });
    }

    const hashed = await bcrypt.hash(password, 10);

    const user = {
      id: "usr_" + Date.now(),
      username,
      email,
      phoneNumber,
      password: hashed,
      role: role || "user",
      createdAt: new Date().toISOString(),

      logs: { logins: [], logouts: [] },
      activeDevices: [],
      sessions: [],

      // MFA defaults
      mfaCode: null,
      mfaExpires: null
    };

    await sendCommand({
      action: "insert",
      dbName: "system",
      collection: "users",
      data: user
    });

    // initialize per-user workspace in engine
    await sendCommand({
      action: "initUserSpace",
      userId: user.id
    });

    console.log("✅ [REGISTER] User created:", user.id);
    res.json({ message: "User registered successfully", userId: user.id });

  } catch (err) {
    console.error("🔥 [REGISTER] ERROR:", err);
    res.status(500).json({ error: "Internal server error" });
  }
}

/* =====================================================
   LOGIN → INIT MFA
===================================================== */
async function loginUser(req, res) {
  try {
    console.log("🟡 [LOGIN] Body:", req.body);

    const { email, username, phoneNumber, password } = req.body;

    const orConditions = [];
    if (email) orConditions.push({ email });
    if (username) orConditions.push({ username });
    if (phoneNumber) orConditions.push({ phoneNumber });

    const users = await sendCommand({
      action: "find",
      dbName: "system",
      collection: "users",
      filter: { $or: orConditions }
    });

    console.log("🟡 [LOGIN] DB response:", users);

    const user = users[0];
    if (!user) {
      console.log("🔴 [LOGIN] User not found");
      console.log("find the ")
      return res.status(404).json({ error: "User not found" });
    }

    const valid = await bcrypt.compare(password, user.password);
    if (!valid) {
      console.log("🔴 [LOGIN] Invalid password");
      return res.status(401).json({ error: "Invalid password" });
    }

    const mfaCode = Math.floor(100000 + Math.random() * 900000).toString();
    const mfaExpires = Date.now() + 5 * 60 * 1000;

    // ⚠️ FULL DOCUMENT UPDATE (ENGINE REQUIREMENT)
    const updatedUser = {
      ...user,
      mfaCode,
      mfaExpires
    };

    await sendCommand({
      action: "updateOne",
      dbName: "system",
      collection: "users",
      filter: { id: user.id },
      update: {
  $set: {
    mfaCode,
    mfaExpires
  }
}

    });

    console.log("✅ [LOGIN] MFA stored in DB", {
      userId: user.id,
      mfaCode,
      mfaExpires
    });

    // DEV MODE MFA
    console.log("🔐 [DEV MFA CODE]:", mfaCode);

    res.json({
      message: "MFA code generated",
      userId: user.id
    });

  } catch (err) {
    console.error("🔥 [LOGIN] ERROR:", err);
    res.status(500).json({ error: "Internal server error" });
  }
}

/* =====================================================
   VERIFY MFA
===================================================== */
async function verifyMFA(req, res) {
  try {
    const { userId } = req.params;
    const { code } = req.body;

    console.log("🟡 [VERIFY MFA] userId (param):", userId);
    console.log("🟡 [VERIFY MFA] code:", code);

    const users = await sendCommand({
      action: "find",
      dbName: "system",
      collection: "users",
      filter: { id: userId }
    });

    console.log("🟡 [VERIFY MFA] DB response:", users);

    const user = users[0];
    if (!user) {
      console.log("🔴 [VERIFY MFA] User not found");
      return res.status(404).json({ error: "User not found" });
    }

    console.log("🟡 [VERIFY MFA] Stored MFA:", {
      mfaCode: user.mfaCode,
      mfaExpires: user.mfaExpires,
      now: Date.now()
    });

    // 🔥 ROOT CAUSE CHECK
    if (!user.mfaCode || !user.mfaExpires || user.mfaExpires === 0) {
      console.error("🔴 [VERIFY MFA] MFA missing in DB");
      return res.status(400).json({ error: "MFA not initialized" });
    }

    if (Date.now() > user.mfaExpires) {
      console.log("🔴 [VERIFY MFA] MFA expired");
      return res.status(400).json({ error: "MFA expired" });
    }

    if (String(user.mfaCode) !== String(code)) {
      console.log("🔴 [VERIFY MFA] Invalid MFA code");
      return res.status(400).json({ error: "Invalid MFA code" });
    }

    const agent = useragent.parse(req.headers["user-agent"] || "unknown");
    const ip = req.socket?.remoteAddress || "unknown";
    const geo = geoip.lookup(ip) || {};

    // normalize optional fields to avoid undefined errors with older users
    const safeLogs = user.logs && typeof user.logs === "object" ? user.logs : { logins: [], logouts: [] };
    safeLogs.logins = Array.isArray(safeLogs.logins) ? safeLogs.logins : [];
    safeLogs.logouts = Array.isArray(safeLogs.logouts) ? safeLogs.logouts : [];

    const safeSessions = Array.isArray(user.sessions) ? user.sessions : [];
    const safeDevices = Array.isArray(user.activeDevices) ? user.activeDevices : [];

    const sessionId = "sess_" + Date.now();

    // ⚠️ FULL DOCUMENT UPDATE AGAIN
    const updatedUser = {
      ...user,
      mfaCode: null,
      mfaExpires: null,
      sessions: [...safeSessions, { sessionId, createdAt: new Date().toISOString() }],
      activeDevices: [...safeDevices, agent.toString()],
      logs: {
        ...safeLogs,
        logins: [...safeLogs.logins, { time: new Date().toISOString(), ip, geo }]
      }
    };

    await sendCommand({
      action: "updateOne",
      dbName: "system",
      collection: "users",
      filter: { id: userId },
      update: updatedUser
    });

    const token = jwt.sign(
      { id: user.id, role: user.role, sessionId },
      jwtSecret,
      { expiresIn: jwtExpire }
    );

    console.log("✅ [VERIFY MFA] Login successful:", sessionId);

    res.json({
      message: "Login successful",
      token,
      sessionId
    });

  } catch (err) {
    console.error("🔥 [VERIFY MFA] ERROR:", err);
    res.status(500).json({ error: "Internal server error" });
  }
}

/* =====================================================
   LOGOUT (FULL DOCUMENT UPDATE)
===================================================== */
async function logoutUser(req, res) {
  try {
    const { userId } = req.params;
    const { deviceName } = req.body;

    console.log("🟡 [LOGOUT] userId:", userId, "device:", deviceName);

    const users = await sendCommand({
      action: "find",
      dbName: "system",
      collection: "users",
      filter: { id: userId }
    });

    const user = users[0];
    if (!user) {
      console.log("🔴 [LOGOUT] User not found");
      return res.status(404).json({ error: "User not found" });
    }

    const updatedUser = {
      ...user,
      activeDevices: (user.activeDevices || []).filter(d => d !== deviceName),
      logs: {
        ...user.logs,
        logouts: [...(user.logs.logouts || []), {
          time: new Date().toISOString(),
          device: deviceName
        }]
      }
    };

    await sendCommand({
      action: "updateOne",
      dbName: "system",
      collection: "users",
      filter: { id: userId },
      update: updatedUser
    });

    console.log("✅ [LOGOUT] Successful");

    res.json({ message: "Logout successful" });

  } catch (err) {
    console.error("🔥 [LOGOUT] ERROR:", err);
    res.status(500).json({ error: "Internal server error" });
  }
}

module.exports = {
  registerUser,
  loginUser,
  verifyMFA,
  logoutUser
};
