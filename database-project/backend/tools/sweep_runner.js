const { exec } = require('child_process');
const fs = require('fs');
const batches = [25,100,250,500];
const conc = [5,10,20];
const total = 1000;
const out = 'sweep_results.txt';
if (fs.existsSync(out)) fs.unlinkSync(out);
(async ()=>{
  for (let bs of batches) {
    for (let c of conc) {
      const header = `RUN batchSize=${bs} concurrency=${c}\n`;
      fs.appendFileSync(out, header);
      console.log(header.trim());
      await new Promise((res)=>{
        const cmd = `node ./backend/tools/bulk_test_fixed.js ${total} ${c} ${bs}`;
        const p = exec(cmd, {cwd: process.cwd(), maxBuffer: 1024*1024}, (err, stdout, stderr)=>{
          if (stdout) fs.appendFileSync(out, stdout);
          if (stderr) fs.appendFileSync(out, stderr);
          if (err) fs.appendFileSync(out, `ERROR: ${err.message}\n`);
          res();
        });
      });
      // small pause
      await new Promise(r=>setTimeout(r,200));
    }
  }
  console.log('\nSweep complete. Results in ' + out);
})();
