const net = require("net");

// Engine connection pool
// - Keeps a small number of persistent sockets
// - Ensures only one outstanding request per socket to avoid framing issues
// - If all pooled sockets are busy, falls back to a temporary socket

const POOL_SIZE = parseInt(process.env.ENGINE_POOL_SIZE || "4", 10);
const ENGINE_HOST = process.env.ENGINE_HOST || "127.0.0.1";
const ENGINE_PORT = parseInt(process.env.ENGINE_PORT || "9000", 10);

class PooledSocket {
  constructor() {
    this.reset();
    this.connect();
  }

  reset() {
    this.socket = null;
    this.busy = false; // whether a request is outstanding on this socket
    this.buffer = "";
    this.currentResolve = null;
    this.currentReject = null;
  }

  connect() {
    this.socket = new net.Socket();

    this.socket.on("data", chunk => {
      this.buffer += chunk.toString();
    });

    this.socket.on("end", () => {
      // response should be in buffer
      const buf = this.buffer;
      this.buffer = "";
      const resolve = this.currentResolve;
      const reject = this.currentReject;
      this.currentResolve = null;
      this.currentReject = null;
      this.busy = false;

      if (!resolve) return; // nothing waiting

      try {
        const response = JSON.parse(buf || "{}");
        if (Array.isArray(response)) return resolve(response);
        if (response?.status === "ok" && Array.isArray(response.data)) return resolve(response.data);
        if (response && typeof response === "object") return resolve(response);
        return resolve([]);
      } catch (err) {
        return reject("Invalid engine response: " + err.message);
      }
    });

    this.socket.on("error", err => {
      const reject = this.currentReject;
      this.currentResolve = null;
      this.currentReject = null;
      this.busy = false;
      if (reject) reject(err.message || String(err));
      // recreate socket after a short delay
      setTimeout(() => this.recreate(), 100);
    });

    this.socket.on("close", () => {
      // ensure current request is rejected
      const reject = this.currentReject;
      this.currentResolve = null;
      this.currentReject = null;
      this.busy = false;
      if (reject) reject("engine connection closed");
      setTimeout(() => this.recreate(), 100);
    });

    // initiate connect
    try {
      this.socket.connect(ENGINE_PORT, ENGINE_HOST);
    } catch (err) {
      setTimeout(() => this.recreate(), 200);
    }
  }

  recreate() {
    try {
      if (this.socket) {
        this.socket.destroy();
      }
    } catch (e) {}
    this.reset();
    this.connect();
  }

  send(payload) {
    return new Promise((resolve, reject) => {
      if (this.busy) return reject(new Error("socket busy"));
      this.busy = true;
      this.currentResolve = resolve;
      this.currentReject = reject;
      this.buffer = "";

      try {
        this.socket.write(JSON.stringify(payload));
      } catch (err) {
        this.busy = false;
        this.currentResolve = null;
        this.currentReject = null;
        return reject(err.message || String(err));
      }
    });
  }
}

// initialize pool
const pool = [];
for (let i = 0; i < POOL_SIZE; i++) pool.push(new PooledSocket());

function getAvailableSocket() {
  for (let i = 0; i < pool.length; i++) {
    if (!pool[i].busy) return pool[i];
  }
  return null;
}

function sendCommand(payload) {
  // try to use pooled socket
  const sock = getAvailableSocket();
  if (sock) {
    return sock.send(payload);
  }

  // fallback: use a short-lived socket if pool is exhausted
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let buffer = "";

    client.connect(ENGINE_PORT, ENGINE_HOST, () => {
      client.write(JSON.stringify(payload));
    });

    client.on("data", chunk => {
      buffer += chunk.toString();
    });

    client.on("end", () => {
      try {
        const response = JSON.parse(buffer || "{}");
        if (Array.isArray(response)) return resolve(response);
        if (response?.status === "ok" && Array.isArray(response.data)) return resolve(response.data);
        if (response && typeof response === "object") return resolve(response);
        return resolve([]);
      } catch (err) {
        return reject("Invalid engine response: " + err.message);
      }
    });

    client.on("error", err => reject(err.message || String(err)));
    client.on("close", () => {
      try { client.destroy(); } catch (e) {}
    });
  });
}

module.exports = { sendCommand };
