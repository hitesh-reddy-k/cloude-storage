const dotenv = require("dotenv");
const path = require("path");

dotenv.config({
  path: path.join(__dirname, "../env/.env")
});

const jwtSecret = process.env.JWT_SECRET;
if (!jwtSecret) {
  throw new Error("JWT_SECRET is missing. Add it to backend/env/.env");
}

// allow either JWT_EXPIRE or legacy jwtExpire, default to 7d
const jwtExpire =
  process.env.JWT_EXPIRE ||
  process.env.jwtExpire ||
  "7d";

module.exports = { jwtSecret, jwtExpire };
