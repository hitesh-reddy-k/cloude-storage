const net = require("net");

function sendCommand(payload) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let buffer = "";

    client.connect(9000, "127.0.0.1", () => {
      client.write(JSON.stringify(payload));
    });

    client.on("data", chunk => {
      buffer += chunk.toString();
    });

    client.on("end", () => {
      try {
        const response = JSON.parse(buffer);

        // ✅ normalize response
        if (Array.isArray(response)) {
          resolve(response);
        }

        if (response?.status === "ok" && Array.isArray(response.data)) {
          resolve(response.data);
        }

        resolve([]);
      } catch (err) {
        reject("Invalid engine response: " + err.message);
      }
    });

    client.on("error", err => reject(err.message));
  });
}

module.exports = { sendCommand };
