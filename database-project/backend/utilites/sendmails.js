const nodemailer = require("nodemailer");
const dotenv=require("dotenv")

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

async function sendMail(to, subject, text) {
  try {
    await transporter.sendMail({
      from: process.env.EMAIL_USER,
      to,
      subject,
      text,
    });
    console.log(`Email sent to ${to}`);
  } catch (err) {
    console.error("Email send error:", err);
  }
}

module.exports = { sendMail };
