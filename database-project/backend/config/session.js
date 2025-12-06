const { v4: uuidv4 } = require("uuid");

function createSession() {
  return {
    sessionId: uuidv4(),
    createdAt: new Date().toISOString(),
    expiresAt: Date.now() + 1000 * 60 * 60 * 24, // 24 hours
  };
}

module.exports = { createSession };
