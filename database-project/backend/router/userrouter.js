const express = require("express");
const router = express.Router();
const { registerUser, loginUser,logoutUser,getUserDeviceLogs,verifyMFA,forgotPassword,resetPassword } = require("../controller/authController");
const { verifyToken } = require("../middleware/authMiddleware");
const { checkRole } = require("../middleware/roleMiddleware");



router.post("/register", registerUser);
router.post("/login", loginUser);
console.log("visited")
router.post("/logout/:userId",logoutUser)
router.post("/verify-mfa/:userId",verifyMFA);






router.get("/user", verifyToken, (req, res) => {
  res.json({ message: `Hello ${req.user.username}` });
});

router.get("/admin", verifyToken, checkRole("admin"), (req, res) => {
  res.json({ message: `Welcome Admin ${req.user.username}` });
});

module.exports = router;
