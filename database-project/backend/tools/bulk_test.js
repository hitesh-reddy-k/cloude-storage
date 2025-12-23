const engine = require("../engineClient");

// Bulk load test using engineClient.sendCommand (uses pooled persistent sockets)
// Usage: node backend/tools/bulk_test.js [total] [concurrency]

const total = parseInt(process.argv[2] || "500", 10);
const concurrency = parseInt(process.argv[3] || "50", 10);

function makeInsert(i) {
  return {
    action: "insert",
    userId: "loadtest",
    dbName: "load-db",
    collection: "user-db",
    data: {
      id: String(Date.now()) + "_" + i,
      name: "BulkUser_" + i,
      email: `bulk${i}@example.com`,
      createdAt: new Date().toISOString()
    }
  };
}

function withTimeout(promise, ms = 5000) {
  return Promise.race([
    promise,
    new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), ms))
  ]);
}

async function worker(queue, id, timeoutMs) {
  let sent = 0, errors = 0;
  while (true) {
    const item = queue.shift();
    if (!item) break;
    try {
      await withTimeout(engine.sendCommand(item), timeoutMs);
      sent++;
    } catch (err) {
      errors++;
      if (errors <= 10) console.error(`worker ${id} send error (${errors}):`, err && err.message ? err.message : err);
    }
    if ((sent + errors) % 50 === 0) {
      console.log(`worker ${id} progress: processed=${sent + errors}`);
    }
  }
  return { sent, errors };
}

(async () => {
  const queue = [];
  for (let i = 0; i < total; i++) queue.push(makeInsert(i));

  const start = Date.now();
  const timeoutMs = parseInt(process.argv[4] || process.env.REQUEST_TIMEOUT || "5000", 10);
  const workers = [];
  for (let i = 0; i < Math.min(concurrency, total); i++) workers.push(worker(queue, i, timeoutMs));

  const results = await Promise.all(workers);
  const durationMs = Date.now() - start;
  const sent = results.reduce((s, r) => s + r.sent, 0);
  const errors = results.reduce((s, r) => s + r.errors, 0);
  console.log(`Done. total=${total} sent=${sent} errors=${errors} time=${durationMs}ms ops/s=${((sent*1000)/durationMs).toFixed(2)}`);
})();
