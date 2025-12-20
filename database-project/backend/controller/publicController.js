const { sendCommand } = require('../engineClient');

function parseConnectionURL(connectionURL) {
  try {
    // allow rdb:// and http(s) schemes — URL handles custom protocols too
    const u = new URL(connectionURL);
    const dbName = (u.pathname || '').replace(/^\//, '');
    const auth = u.searchParams.get('auth') || u.searchParams.get('token') || null;
    return { dbName, auth, username: u.username || null, password: u.password || null };
  } catch (err) {
    throw new Error('Invalid connection URL');
  }
}

/**
 * Public HTTP insert endpoint
 * Body: { connectionURL, schemaName, data }
 * connectionURL must include ?auth=<userId> so we can map to the correct user
 */
async function insertViaPublicURL(req, res) {
  try {
    const { connectionURL, schemaName } = req.body;
    const payloadData = req.body.data || req.body.payload || req.body;

    if (!connectionURL) return res.status(400).json({ error: 'connectionURL required' });
    if (!schemaName) return res.status(400).json({ error: 'schemaName required' });

    const parsed = parseConnectionURL(connectionURL);
    const userId = parsed.auth;
    const dbName = parsed.dbName;

    if (!userId) return res.status(400).json({ error: 'connectionURL must include auth=<userId>' });
    if (!dbName) return res.status(400).json({ error: 'connectionURL must include database name in path' });

    // ensure document contains an 'id' string so LSM will persist updates later
    if (!payloadData || typeof payloadData !== 'object') payloadData = {};
    if (!payloadData.id) payloadData.id = String(Date.now());

    const engineRes = await sendCommand({
      action: 'insert',
      userId,
      dbName,
      collection: schemaName,
      data: payloadData
    });
    return res.status(201).json({ success: true, insertedId: payloadData.id, engineRes });
  } catch (err) {
    console.error('[public insert] error', err);
    return res.status(500).json({ error: err.message || 'Failed to insert' });
  }
}

async function updateViaPublicURL(req, res) {
  try {
    const { connectionURL, schemaName, recordId } = req.body;
    const updatePayload = req.body.update || req.body.data || req.body.payload || req.body;

    if (!connectionURL) return res.status(400).json({ error: 'connectionURL required' });
    if (!schemaName) return res.status(400).json({ error: 'schemaName required' });
    if (!recordId && !req.body.filter) return res.status(400).json({ error: 'recordId or filter required' });

    const parsed = parseConnectionURL(connectionURL);
    const userId = parsed.auth;
    const dbName = parsed.dbName;

    if (!userId) return res.status(400).json({ error: 'connectionURL must include auth=<userId>' });
    if (!dbName) return res.status(400).json({ error: 'connectionURL must include database name in path' });

    let filter = req.body.filter || (recordId ? { id: String(recordId) } : {});

    // If a non-id filter was provided, resolve it to an id first (engine expects id present for writes)
    if (filter && !filter.id) {
      const found = await sendCommand({
        action: 'find',
        userId,
        dbName,
        collection: schemaName,
        filter
      });

      if (!Array.isArray(found) || !found.length) {
        return res.status(404).json({ error: 'No matching record found to update' });
      }

      // take the first matched document and use its id
      const first = found[0];
      if (!first.id) return res.status(400).json({ error: 'Matched record missing id, cannot update' });
      filter = { id: String(first.id) };
    }

    const engineRes = await sendCommand({
      action: 'updateOne',
      userId,
      dbName,
      collection: schemaName,
      filter,
      update: updatePayload
    });

    return res.json({ success: true, engineRes });
  } catch (err) {
    console.error('[public update] error', err);
    return res.status(500).json({ error: err.message || 'Failed to update' });
  }
}

async function deleteViaPublicURL(req, res) {
  try {
    // allow delete via body or query
    const body = req.body || {};
    const query = req.query || {};
    const connectionURL = body.connectionURL || query.connectionURL;
    const schemaName = body.schemaName || query.schemaName;
    const recordId = body.recordId || query.recordId;
    const filter = body.filter || query.filter || (recordId ? { id: String(recordId) } : null);

    if (!connectionURL) return res.status(400).json({ error: 'connectionURL required' });
    if (!schemaName) return res.status(400).json({ error: 'schemaName required' });
    if (!filter) return res.status(400).json({ error: 'recordId or filter required' });

    const parsed = parseConnectionURL(connectionURL);
    const userId = parsed.auth;
    const dbName = parsed.dbName;

    if (!userId) return res.status(400).json({ error: 'connectionURL must include auth=<userId>' });
    if (!dbName) return res.status(400).json({ error: 'connectionURL must include database name in path' });

    let resolvedFilter = filter;

    if (resolvedFilter && !resolvedFilter.id) {
      const found = await sendCommand({
        action: 'find',
        userId,
        dbName,
        collection: schemaName,
        filter: resolvedFilter
      });

      if (!Array.isArray(found) || !found.length) {
        return res.status(404).json({ error: 'No matching record found to delete' });
      }

      const first = found[0];
      if (!first.id) return res.status(400).json({ error: 'Matched record missing id, cannot delete' });
      resolvedFilter = { id: String(first.id) };
    }

    const engineRes = await sendCommand({
      action: 'deleteOne',
      userId,
      dbName,
      collection: schemaName,
      filter: resolvedFilter
    });

    return res.json({ success: true, engineRes });
  } catch (err) {
    console.error('[public delete] error', err);
    return res.status(500).json({ error: err.message || 'Failed to delete' });
  }
}

module.exports = { insertViaPublicURL, updateViaPublicURL, deleteViaPublicURL };
