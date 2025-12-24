const nodemailer = require("nodemailer");
const dotenv = require("dotenv");
const path = require("path");

// load backend env file (backend/env/.env)
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

// verify transporter at startup to surface auth/config issues early
transporter.verify().then(() => {
  console.log("📨 Mail transporter ready");
}).catch(err => {
  console.error("✉️ Mail transporter verify failed:", err && err.message ? err.message : err);
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
    console.error("Email send error:", err && err.message ? err.message : err);
  }
}

function buildOtpHtml({ appName = "MyDB", username = "", code, minutes = 5 }) {
  return `
  <div style="font-family: Arial, Helvetica, sans-serif; color: #111;">
    <div style="max-width:600px;margin:0 auto;padding:20px;border:1px solid #f0f0f0;border-radius:8px;">
      <div style="text-align:center;padding-bottom:12px;">
        <h2 style="margin:0;color:#2b6cb0;">${appName} — One Time Password</h2>
      </div>
      <p>Hi ${username || "there"},</p>
      <p>Use the code below to complete your sign in. This code will expire in <strong>${minutes} minutes</strong>.</p>

      <div style="text-align:center;margin:24px 0;">
        <div style="display:inline-block;background:#f7fafc;padding:18px 24px;border-radius:8px;border:1px solid #e2e8f0">
          <span style="font-size:28px;letter-spacing:4px;font-weight:700;color:#111">${code}</span>
        </div>
      </div>

      <p>If you did not request this code, you can safely ignore this email.</p>

      <hr style="border:none;border-top:1px solid #edf2f7;margin:20px 0;"/>
      <p style="font-size:12px;color:#718096;margin:0;">${appName} team</p>
    </div>
  </div>
  `;
}

async function sendOTP(to, code, minutes = 5, opts = {}) {
  const subject = opts.subject || "Your OTP code";
  const text = `Your OTP is ${code}. It expires in ${minutes} minutes.`;
  const html = buildOtpHtml({ appName: opts.appName || "MyDB", username: opts.username || "", code, minutes });

  try {
    await transporter.sendMail({
      from: process.env.EMAIL_USER,
      to,
      subject,
      text,
      html,
    });
    console.log(`OTP email sent to ${to}`);
  } catch (err) {
    console.error("OTP send error:", err && err.message ? err.message : err);
  }
}

module.exports = { sendMail, sendOTP };
