const engine = require('../engineClient');

(async () => {
  try {
    const res = await Promise.race([
      engine.sendCommand({ action: 'ping' }),
      new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), 3000))
    ]);
    console.log('ping response:', res);
  } catch (err) {
    console.error('ping error:', err && err.message ? err.message : err);
  }
})();
