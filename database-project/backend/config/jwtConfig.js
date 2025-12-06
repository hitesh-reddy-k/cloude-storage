const dotenv = require("dotenv")
const path = require("path")

dotenv.config({
  path: path.join(__dirname, "../env/.env")
});
console.log(process.env.JWT_SECRET)

module.exports = {
  jwtSecret: process.env.JWT_SECRET, 
  jwtExpire: process.env.jwtExpire, 
};
