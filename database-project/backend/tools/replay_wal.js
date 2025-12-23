const fs = require('fs');
const path = require('path');
const net = require('net');

const DATA_ROOT = path.resolve(__dirname, '..', '..', '..', 'data');
const ENGINE_HOST = '127.0.0.1';
const ENGINE_PORT = 9000;

function findWalFiles(dir) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const e of entries) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) out.push(...findWalFiles(p));
    else if (e.isFile() && e.name.endsWith('.wal')) out.push(p);
  }
  return out;
}

function parseWalFile(file) {
  const buf = fs.readFileSync(file);
  const entries = [];
  let off = 0;
  while (off < buf.length) {
    if (off + 5 > buf.length) break; // not enough header
    const op = buf.readUInt8(off); off += 1; // op (enum as uint8)
    const size = buf.readUInt32LE(off); off += 4;
    if (off + size > buf.length) break;
    const payload = buf.toString('utf8', off, off + size); off += size;
    try {
      const j = JSON.parse(payload);
      entries.push(j);
    } catch (err) {
      // ignore unparsable
    }
  }
  return entries;
}

function sendToEngine(payload) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let buffer = '';
    client.connect(ENGINE_PORT, ENGINE_HOST, () => {
      client.write(JSON.stringify(payload));
    });
    client.on('data', chunk => { buffer += chunk.toString(); });
    client.on('end', () => {
      try { resolve(JSON.parse(buffer)); } catch (e) { resolve(buffer); }
    });
    client.on('error', err => reject(err));
  });
}

function mapWalEntryToCommand(e) {
  const op = (e.op || '').toString().toUpperCase();
  if (op === 'PUT' || op === 'INSERT') {
    return { action: 'insert', userId: e.userId || e.user_id, dbName: e.db || e.database, collection: e.collection, data: e.data };
  }
  if (op === 'DELETE') {
    const id = e.id || (e.data && e.data.id) || (e.filter && e.filter.id);
    return { action: 'deleteOne', userId: e.userId || e.user_id, dbName: e.db || e.database, collection: e.collection, filter: { id: String(id) } };
  }
  if (op === 'UPDATE') {
    return { action: 'updateOne', userId: e.userId || e.user_id, dbName: e.db || e.database, collection: e.collection, filter: e.filter || {}, update: e.update || e.data || {} };
  }
  // unknown: send as-is for manual inspection
  return null;
}

async function replayAll({ concurrency = 8, dry = false } = {}) {
  const walDirs = findWalFiles(DATA_ROOT);
  console.log('Found', walDirs.length, '.wal files under', DATA_ROOT);

  const allEntries = [];
  for (const f of walDirs) {
    const entries = parseWalFile(f);
    console.log('  ', path.relative(DATA_ROOT, f), '->', entries.length, 'entries');
    for (const e of entries) allEntries.push(e);
  }

  console.log('Total WAL entries:', allEntries.length);
  if (dry) return;

  let idx = 0; let sent = 0; let errors = 0;
  const start = Date.now();

  const workers = new Array(concurrency).fill(0).map(async () => {
    while (true) {
      const i = idx++;
      if (i >= allEntries.length) break;
      const e = allEntries[i];
      const cmd = mapWalEntryToCommand(e);
      if (!cmd) { console.warn('Skipping unknown entry', e); continue; }
      try {
        await sendToEngine(cmd);
        sent++;
      } catch (err) {
        console.error('Send error', err && err.message ? err.message : err);
        errors++;
      }
    }
  });

  await Promise.all(workers);
  const ms = Date.now() - start;
  console.log(`Done. sent=${sent} errors=${errors} time=${ms}ms`);
}

if (require.main === module) {
  const args = process.argv.slice(2);
  const concurrency = Number(args[0] || process.env.REPLAY_CONCURRENCY || 8);
  const dry = args.includes('--dry');
  replayAll({ concurrency, dry }).catch(err => { console.error(err); process.exit(1); });
}

module.exports = { replayAll };
