const net = require("net");

function sendCommand(payload) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();

    client.connect(9000, "127.0.0.1", () => {
      client.write(JSON.stringify(payload));
    });

    client.on("data", data => {
      try {
        resolve(JSON.parse(data.toString()));
      } catch {
        reject("Invalid engine response");
      }
      client.destroy();
    });

    client.on("error", err => reject(err.message));
  });
}

module.exports = { sendCommand };
