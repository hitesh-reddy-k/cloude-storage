const express = require('express');
const router = express.Router();

const { insertViaPublicURL, updateViaPublicURL, deleteViaPublicURL } = require('../controller/publicController');

// POST /public/insert
router.post('/insert', insertViaPublicURL);

// PUT /public/update  -> body: { connectionURL, schemaName, recordId?, filter?, update }
router.put('/update', updateViaPublicURL);

// DELETE /public/delete -> body or query: { connectionURL, schemaName, recordId?, filter? }
router.delete('/delete', deleteViaPublicURL);

module.exports = router;
