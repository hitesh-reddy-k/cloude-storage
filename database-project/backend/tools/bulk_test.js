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
  const engine = require("../engineClient");

  // Bulk load test that uses the engine 'bulk' action to send batches of ops.
  // Usage: node backend/tools/bulk_test.js [total] [concurrency] [batchSize]

  const total = parseInt(process.argv[2] || "500", 10);
  const concurrency = parseInt(process.argv[3] || "20", 10);
  const batchSize = parseInt(process.argv[4] || "25", 10);

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

  // Create an array of batches (each batch is an array of ops)
  function createBatches(total, batchSize) {
    const ops = [];
    for (let i = 0; i < total; i++) ops.push(makeInsert(i));
    const batches = [];
    for (let i = 0; i < ops.length; i += batchSize) batches.push(ops.slice(i, i + batchSize));
    return batches;
  }

  async function worker(queue, id, timeoutMs) {
    let sent = 0, errors = 0;
    while (true) {
      const batch = queue.shift();
      if (!batch) break;
      try {
        // send as one bulk op
        await withTimeout(engine.sendCommand({ action: 'bulk', ops: batch }), timeoutMs);
        sent += batch.length;
      } catch (err) {
        errors += batch.length;
        if (errors <= 5) console.error(`worker ${id} bulk send error:`, err && err.message ? err.message : err);
      }
      if ((sent + errors) % (batchSize * 5) === 0) {
        console.log(`worker ${id} progress: processed=${sent + errors}`);
      }
    }
    return { sent, errors };
  }

  const engine = require("../engineClient");

  // Bulk load test that uses the engine 'bulk' action to send batches of ops.
  // Usage: node backend/tools/bulk_test.js [total] [concurrency] [batchSize]

  const total = parseInt(process.argv[2] || "500", 10);
  const concurrency = parseInt(process.argv[3] || "20", 10);
  const batchSize = parseInt(process.argv[4] || "25", 10);

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

  // Create an array of batches (each batch is an array of ops)
  function createBatches(total, batchSize) {
    const ops = [];
    for (let i = 0; i < total; i++) ops.push(makeInsert(i));
    const batches = [];
    for (let i = 0; i < ops.length; i += batchSize) batches.push(ops.slice(i, i + batchSize));
    return batches;
  }

  async function worker(queue, id, timeoutMs) {
    let sent = 0, errors = 0;
    while (true) {
      const batch = queue.shift();
      if (!batch) break;
      try {
        // send as one bulk op
        await withTimeout(engine.sendCommand({ action: 'bulk', ops: batch }), timeoutMs);
        sent += batch.length;
      } catch (err) {
        errors += batch.length;
        if (errors <= 5) console.error(`worker ${id} bulk send error:`, err && err.message ? err.message : err);
      }
      if ((sent + errors) % (batchSize * 5) === 0) {
        console.log(`worker ${id} progress: processed=${sent + errors}`);
      }
    }
    return { sent, errors };
  }

  (async () => {
    const batches = createBatches(total, batchSize);
    const queue = batches.slice(); // shallow copy

    const start = Date.now();
    const timeoutMs = parseInt(process.env.REQUEST_TIMEOUT || "5000", 10);
    const workers = [];
    for (let i = 0; i < Math.min(concurrency, batches.length); i++) workers.push(worker(queue, i, timeoutMs));

    const results = await Promise.all(workers);
    const durationMs = Date.now() - start;
    const sent = results.reduce((s, r) => s + r.sent, 0);
    const errors = results.reduce((s, r) => s + r.errors, 0);
    console.log(`Done. total=${total} sent=${sent} errors=${errors} time=${durationMs}ms ops/s=${((sent*1000)/durationMs).toFixed(2)}`);
  })();
