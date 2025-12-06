const express = require('express')
const app = express()
const port = 9999
const  dotenv = require("dotenv")
const user = require("../backend/router/userrouter")
const database = require("../backend/router/databaseRoutes")
dotenv.config();


app.get('/', (req, res) => {
  res.send('Hello World!')
})

app.use(express.json());

app.use("/user",user)
app.use("/database",database)



app.listen(port, () => {
  console.log(`Example app listening on port ${port}`)
})
