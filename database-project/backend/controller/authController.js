const bcrypt = require("bcrypt");
const jwt = require("jsonwebtoken");
const dotenv = require("dotenv");
const { sendCommand } = require("../engineClient");
const { jwtSecret, jwtExpire } = require("../config/jwtConfig");
const nodemailer = require("nodemailer");
const useragent = require("useragent");
const geoip = require("geoip-lite");

dotenv.config();

const transporter = nodemailer.createTransport({
  service: "gmail",
  auth: {
    user: process.env.EMAIL_USER,
    pass: process.env.EMAIL_PASS
  }
});

/* ----------------------------------
   REGISTER
---------------------------------- */
async function registerUser(req, res) {
  const { username, email, password, role, phoneNumber } = req.body;

  if (!username || !email || !password)
    return res.status(400).json({ error: "Username, email, and password required" });

  const exists = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { $or: [{ email }, { username }, { phoneNumber }] }
  });

  if (exists.length)
    return res.status(400).json({ error: "User already exists" });

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
    sessions: []
  };

  await sendCommand({
    action: "insert",
    dbName: "system",
    collection: "users",
    data: user
  });

  res.json({
    message: "User registered successfully",
    user: {
      id: user.id,
      username,
      email,
      role: user.role
    }
  });
}

/* ----------------------------------
   LOGIN (MFA)
---------------------------------- */
async function loginUser(req, res) {
  const { email, username, phoneNumber, password } = req.body;

  const users = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { $or: [{ email }, { username }, { phoneNumber }] }
  });

  const user = users[0];
  if (!user) return res.status(404).json({ error: "User not found" });

  const valid = await bcrypt.compare(password, user.password);
  if (!valid) return res.status(401).json({ error: "Invalid password" });

  const mfaCode = Math.floor(100000 + Math.random() * 900000).toString();

  await sendCommand({
    action: "updateOne",
    dbName: "system",
    collection: "users",
    filter: { id: user.id },
    update: {
      mfaCode,
      mfaExpires: Date.now() + 5 * 60 * 1000
    }
  });

  await transporter.sendMail({
    to: user.email,
    subject: "Your MFA Code",
    text: `Your MFA code is ${mfaCode}`
  });

  res.json({ message: "MFA code sent", userId: user.id });
}

/* ----------------------------------
   VERIFY MFA
---------------------------------- */
async function verifyMFA(req, res) {
  const { userId } = req.params;
  const { code } = req.body;

  const users = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { id: userId }
  });

  const user = users[0];
  if (!user) return res.status(404).json({ error: "User not found" });

  if (Date.now() > user.mfaExpires)
    return res.status(400).json({ error: "MFA expired" });

  if (user.mfaCode !== code)
    return res.status(400).json({ error: "Invalid MFA code" });

  const agent = useragent.parse(req.headers["user-agent"]);
  const ip = req.socket.remoteAddress;
  const geo = geoip.lookup(ip) || {};

  const sessionId = "sess_" + Date.now();

  user.sessions.push({ sessionId, createdAt: new Date().toISOString() });
  user.activeDevices.push(agent.toString());
  user.logs.logins.push({ time: new Date().toISOString(), ip, geo });

  await sendCommand({
    action: "updateOne",
    dbName: "system",
    collection: "users",
    filter: { id: userId },
    update: user
  });

  const token = jwt.sign(
    { id: user.id, role: user.role, sessionId },
    jwtSecret,
    { expiresIn: jwtExpire }
  );

  res.json({
    message: "Login successful",
    token,
    sessionId
  });
}

/* ----------------------------------
   LOGOUT
---------------------------------- */
async function logoutUser(req, res) {
  const { userId } = req.params;
  const { deviceName } = req.body;

  const users = await sendCommand({
    action: "find",
    dbName: "system",
    collection: "users",
    filter: { id: userId }
  });

  const user = users[0];
  if (!user) return res.status(404).json({ error: "User not found" });

  user.activeDevices = user.activeDevices.filter(d => d !== deviceName);
  user.logs.logouts.push({ time: new Date().toISOString(), device: deviceName });

  await sendCommand({
    action: "updateOne",
    dbName: "system",
    collection: "users",
    filter: { id: userId },
    update: user
  });

  res.json({ message: "Logout successful" });
}

module.exports = {
  registerUser,
  loginUser,
  verifyMFA,
  logoutUser
};
