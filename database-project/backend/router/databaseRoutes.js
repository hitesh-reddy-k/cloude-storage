const express = require("express");
const {verifyToken} = require("../middleware/authMiddleware")
const router = express.Router();

const {
  insertData,
  updateData,
  deleteData,
  readData
} = require("../config/dbmanager");

const {
  askDatabasePermission,
  addSchema,
  listUserDatabases,
  setupDatabaseCredentials,
  connectDatabase
} = require("../controller/databasecontroller");

const { insertViaPublicURL } = require("../controller/publicController");

router.post("/:userId/create-db",verifyToken,askDatabasePermission);

router.post("/:userId/:dbName/set-credentials", verifyToken, setupDatabaseCredentials);

router.post("/:userId/:dbName/create-schema",verifyToken,addSchema);

router.get("/:userId/databases",verifyToken,listUserDatabases);

router.get("/connect/:userId/:dbName/:schemaName", connectDatabase);

// Public HTTP insert endpoint (no auth) — expects body: { connectionURL, schemaName, data }
router.post("/public/insert", insertViaPublicURL);


router.post("/:userId/:dbName/:schemaName/insert", verifyToken, async (req, res) => {
  try {
    const result = await insertData(
      req.params.userId,
      req.params.dbName,
      req.params.schemaName,
      req.body
    );

    return res.status(201).json({
      success: true,
      message: "Data inserted successfully",
      id: result.inserted.id,     
      inserted: result.inserted
    });

  } catch (err) {
    return res.status(400).json({ success:false, error: err.message });
  }
});



router.get("/:userId/:dbName/:schemaName", verifyToken, async (req, res) => {
  try {
    const data = await readData(req.params.userId, req.params.dbName, req.params.schemaName);
    return res.json({ success:true, data });
  } catch (err) {
    return res.status(400).json({ success:false, error: err.message });
  }
});



router.put("/:userId/:dbName/:schemaName/update/:recordId", verifyToken, async (req, res) => {
  try {
    const result = await updateData(
      req.params.userId,
      req.params.dbName,
      req.params.schemaName,
      req.params.recordId,
      req.body
    );
    return res.json({ success:true, message:"Updated", result });
  } catch (err) {
    return res.status(400).json({ success:false, error: err.message });
  }
});



router.delete("/:userId/:dbName/:schemaName/delete/:recordId", verifyToken, async (req, res) => {
  try {
    const result = await deleteData(
      req.params.userId,
      req.params.dbName,
      req.params.schemaName,
      req.params.recordId
    );
    return res.json({ success:true, message:"Deleted", result });
  } catch (err) {
    return res.status(400).json({ success:false, error: err.message });
  }
});



module.exports = router;