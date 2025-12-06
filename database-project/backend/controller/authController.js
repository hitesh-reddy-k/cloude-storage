const bcrypt = require("bcrypt");
const jwt = require("jsonwebtoken");
const dotenv = require("dotenv");
const { readUsers, writeUsers } = require("../config/db");
const { generateUniqueId } = require("../utilites/generatId");
const { jwtSecret, jwtExpire } = require("../config/jwtConfig");
const useragent = require("useragent");
const geoip = require("geoip-lite");
const nodemailer = require("nodemailer");
const path = require("path");


dotenv.config({
  path: path.join(__dirname, "../env/.env")
});



const transporter = nodemailer.createTransport({
  service: "gmail",
  auth: {
    user: process.env.EMAIL_USER,
    pass: process.env.EMAIL_PASS,
  },
});

function findUser(users, { email, username, phoneNumber }) {
  return users.find(
    (u) =>
      u.email === email ||
      u.username === username ||
      u.phoneNumber === phoneNumber
  );
}

async function registerUser(req, res) {
  const { username, email, password, role, phoneNumber } = req.body;

  if (!username || !email || !password)
    return res
      .status(400)
      .json({ error: "Username, email, and password required" });

  const users = await readUsers();
  const existing = findUser(users, { email, username, phoneNumber });
  if (existing)
    return res.status(400).json({ error: "User already exists" });

  const hashed = await bcrypt.hash(password, 10);

  const newUser = {
    id: generateUniqueId(),
    username,
    email,
    phoneNumber,
    password: hashed,
    role: role || "user",
    createdAt: new Date().toISOString(),
    logs: { logins: [], logouts: [] },
    activeDevices: [],
  };

  users.push(newUser);
  await writeUsers(users);

  res.json({
    message: "User registered successfully",
    user: {
      id: newUser.id,
      username: newUser.username,
      email: newUser.email,
      role: newUser.role,
      createdAt: newUser.createdAt,
    },
  });
}

async function loginUser(req, res) {
  console.log("➡️  Login route hit"); // STEP 1
  const { email, username, phoneNumber, password } = req.body;

  if ((!email && !username && !phoneNumber) || !password) {
    console.log("❌ Missing credentials");
    return res.status(400).json({ error: "Missing credentials" });
  }

  const users = await readUsers();
  console.log("✅ Users read successfully");

  const user = findUser(users, { email, username, phoneNumber });
  console.log("👤 User found:", user ? user.username : "none");

  if (!user) return res.status(404).json({ error: "User not found" });

  const valid = await bcrypt.compare(password, user.password);
  console.log("🔑 Password valid?", valid);

  if (!valid) return res.status(401).json({ error: "Invalid password" });

  const mfaCode = Math.floor(100000 + Math.random() * 900000).toString();
  console.log("📩 MFA code generated:", mfaCode);

  user.mfaCode = mfaCode;
  user.mfaExpires = Date.now() + 5 * 60 * 1000;

  await writeUsers(users);
  console.log("💾 Users updated with MFA code");

  try {
    console.log("📧 Sending email...");
    await transporter.sendMail({
      from: process.env.EMAIL_USER,
      to: user.email,
      subject: "Your Login Verification Code (MFA)",
      text: `Your MFA verification code is: ${mfaCode}`,
    });
    console.log("✅ Email sent");
  } catch (err) {
    console.error("❌ Email send failed:", err);
  }

  res.json({
    message: "MFA code sent to your email. Verify to complete login.",
    userId: user.id,
  });
}


async function verifyMFA(req, res) {
  const { userId } = req.params;
  const { code } = req.body;

  if (!userId || !code)
    return res.status(400).json({ error: "User ID and MFA code required" });

  const users = await readUsers();
  const user = users.find((u) => u.id === userId);

  if (!user) return res.status(404).json({ error: "User not found" });
  if (Date.now() > user.mfaExpires)
    return res.status(400).json({ error: "MFA code expired" });
  if (user.mfaCode !== code)
    return res.status(400).json({ error: "Invalid MFA code" });

  delete user.mfaCode;
  delete user.mfaExpires;

  // --- DEVICE INFO ---
  const agent = useragent.parse(req.headers["user-agent"]);
  const browser = `${agent.family} ${agent.major}`;
  const os = agent.os.toString();
  const ip = req.headers["x-forwarded-for"] || req.socket.remoteAddress;
  const geo = geoip.lookup(ip) || {};
  const location = geo.city
    ? `${geo.city}, ${geo.country}`
    : geo.country || "Unknown";
  const device = `${browser} (${os})`;

  if (!user.logs) user.logs = { logins: [], logouts: [] };
  if (!user.activeDevices) user.activeDevices = [];

  user.logs.logins.push({ time: new Date().toISOString(), device, ip, location });
  if (!user.activeDevices.includes(device)) user.activeDevices.push(device);

  // --- ⭐ CREATE AND STORE SESSION ⭐ ---
  const { createSession } = require("../config/session");
  const session = createSession();

  if (!user.sessions) user.sessions = [];
  user.sessions.push(session);

  // --- SAVE USER ---
  await writeUsers(users);

  // --- ⭐ TOKEN WITH sessionId ⭐ ---
  const token = jwt.sign(
    {
      id: user.id,
      username: user.username,
      role: user.role,
      sessionId: session.sessionId,  // IMPORTANT
    },
    jwtSecret,
    { expiresIn: jwtExpire }
  );

  res.json({
    message: "MFA verified successfully. Login complete.",
    token,
    sessionId: session.sessionId,
    user: {
      id: user.id,
      username: user.username,
      email: user.email,
      activeDevices: user.activeDevices,
    },
  });
}

async function logoutUser(req, res) {
  const { userId } = req.params;
  const { deviceName } = req.body;

  if (!userId || !deviceName)
    return res.status(400).json({ error: "User ID and device name required" });

  const users = await readUsers();
  const user = users.find((u) => u.id === userId);
  if (!user) return res.status(404).json({ error: "User not found" });

  const logoutTime = new Date().toISOString();
  user.logs.logouts.push({ time: logoutTime, device: deviceName });
  user.activeDevices = user.activeDevices.filter((d) => d !== deviceName);

  await writeUsers(users);

  res.json({
    message: "Logout successful",
    user: {
      id: user.id,
      username: user.username,
      activeDevices: user.activeDevices,
      lastLogout: logoutTime,
    },
  });
}


async function getUserDeviceLogs(req, res) {
  try {
    const { userId } = req.params;

    if (!userId)
      return res.status(400).json({ error: "User ID is required in URL" });

    const users = await readUsers();
    const user = users.find((u) => u.id === userId);

    if (!user)
      return res.status(404).json({ error: "User not found" });

    res.json({
      message: "User device logs fetched successfully",
      user: {
        id: user.id,
        username: user.username,
        email: user.email,
      },
      totalLogins: user.logs.logins.length,
      totalLogouts: user.logs.logouts.length,
      activeDeviceCount: user.activeDevices.length,
      logins: user.logs.logins,
      logouts: user.logs.logouts,
      activeDevices: user.activeDevices,
    });
  } catch (error) {
    console.error("Error fetching device logs:", error);
    res.status(500).json({ error: "Internal server error" });
  }
}

async function forgotPassword(req, res) {
  const { email } = req.body;

  if (!email) return res.status(400).json({ error: "Email required" });

  const users = await readUsers();
  const user = users.find((u) => u.email === email);
  if (!user) return res.status(404).json({ error: "User not found" });

  const resetCode = Math.floor(100000 + Math.random() * 900000).toString();
  user.resetCode = resetCode;
  user.resetExpires = Date.now() + 10 * 60 * 1000;

  await writeUsers(users);

  try {
    await transporter.sendMail({
      from: process.env.EMAIL_USER,
      to: email,
      subject: "Password Reset Code",
      text: `Your password reset code is: ${resetCode}`,
    });
  } catch (err) {
    console.error("Reset email error:", err);
  }

  res.json({ message: "Password reset code sent to your email" });
}

async function resetPassword(req, res) {
  const { email, code, newPassword } = req.body;

  if (!email || !code || !newPassword)
    return res
      .status(400)
      .json({ error: "Email, reset code, and new password required" });

  const users = await readUsers();
  const user = users.find((u) => u.email === email);
  if (!user) return res.status(404).json({ error: "User not found" });

  if (user.resetCode !== code)
    return res.status(400).json({ error: "Invalid reset code" });
  if (Date.now() > user.resetExpires)
    return res.status(400).json({ error: "Reset code expired" });

  const hashed = await bcrypt.hash(newPassword, 10);
  user.password = hashed;

  delete user.resetCode;
  delete user.resetExpires;

  await writeUsers(users);

  res.json({ message: "Password reset successfully" });
}

module.exports = {
  registerUser,
  loginUser,
  verifyMFA,
  logoutUser,
  getUserDeviceLogs,
  forgotPassword,
  resetPassword,
};
