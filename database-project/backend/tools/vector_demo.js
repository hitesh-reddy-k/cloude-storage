const engine = require('../engineClient');

async function main() {
  const userId = 'vector-user';
  const dbName = 'vecdb';
  const collection = 'media';

  // Insert few vectors (pretend embeddings)
  const vectors = [
    {id:'img1', modality:'image', vector:[0.1,0.2,0.3,0.4], metadata:{label:'cat'}},
    {id:'img2', modality:'image', vector:[0.11,0.19,0.31,0.39], metadata:{label:'cat'}},
    {id:'img3', modality:'image', vector:[0.9,0.8,0.7,0.6], metadata:{label:'dog'}},
    {id:'aud1', modality:'audio', vector:[0.05,0.06,0.07,0.08], metadata:{label:'speech'}},
  ];

  for (const v of vectors) {
    const payload = {
      action: 'insertVector',
      userId, dbName, collection,
      data: v
    };
    const res = await engine.sendCommand(payload);
    console.log('insertVector', v.id, res);
  }

  // Query similar to img1
  const q = {
    action: 'queryVector',
    userId, dbName, collection,
    modality: 'image',
    vector: [0.1,0.2,0.3,0.4],
    k: 3,
    metric: 'cosine'
  };
  const results = await engine.sendCommand(q);
  console.log('query results', results);
}

main().catch(err=>{console.error(err); process.exit(1);});
