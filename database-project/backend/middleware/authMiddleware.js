const jwt = require("jsonwebtoken");
const { jwtSecret } = require("../config/jwtConfig");

// Detect ALL supported database token formats
function isDatabaseToken(token) {
  return (
    /^[a-f0-9]{40}$/i.test(token) ||    // 40-char old DB token
    /^[a-f0-9]{32}$/i.test(token) ||    // 32-char new DB token
    /^[a-f0-9]{24}$/i.test(token) ||    // 24-char userId (buggy URL)
    /^[a-f0-9]{24}-[a-f0-9]{16}$/i.test(token) // userId-sessionId format
  );
}

module.exports.verifyToken = (req, res, next) => {
  console.log("\n🔍 ===== VERIFY TOKEN DEBUG START =====");

  const authHeader = req.headers.authorization;
  console.log("🟦 Auth Header:", authHeader);

  if (!authHeader) {
    console.log("❌ No authorization header found");
    return res.status(401).json({ error: "Missing Authorization header" });
  }

  const token = authHeader.split(" ")[1];
  console.log("🔑 Extracted Token:", token);

  console.log("🔎 Checking if token is Database Token…");

  // CASE 1 — Database Token
  if (isDatabaseToken(token)) {
    console.log("✅ Detected DATABASE TOKEN");

    req.authType = "database";

    // parse only if format is userId-sessionId
    if (/^[a-f0-9]{24}-[a-f0-9]{16}$/i.test(token)) {
      const [userId, sessionId] = token.split("-");
      req.userId = userId;
      req.sessionId = sessionId;
    } else {
      // pure hex tokens only contain userId prefix
      req.userId = token.substring(0, 24);
      req.sessionId = null;
    }

    console.log("🆔 UserID:", req.userId);
    console.log("🟪 SessionID:", req.sessionId);

    console.log("👉 Passing to next middleware…");
    console.log("===== VERIFY TOKEN DEBUG END =====\n");
    return next();
  }

  // CASE 2 — JWT Authentication
  console.log("🔎 Checking if token is JWT…");

  try {
    const decoded = jwt.verify(token, jwtSecret);
    console.log("✅ JWT Verified:", decoded);

    req.authType = "jwt";
    req.userId = decoded.id;
    req.sessionId = decoded.sessionId;

    console.log("🆔 JWT UserID:", decoded.id);
    console.log("🟪 JWT SessionID:", decoded.sessionId);

    console.log("👉 Passing request to next middleware…");
    console.log("===== VERIFY TOKEN DEBUG END =====\n");
    next();
  } catch (err) {
    console.log("❌ JWT Verification Failed:", err.message);
    console.log("===== VERIFY TOKEN DEBUG END =====\n");
    return res.status(401).json({ error: "Invalid or expired token" });
  }
};
