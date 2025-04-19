const express = require('express');
const bodyParser = require('body-parser');
const fs = require('fs');
const sharp = require('sharp');
const cors = require('cors');

const app = express();
const PORT = 3000;


//allow requests from origin
app.use(cors());

//latest switch state
let lastSwitchState = "unknown";

//parse incoming POST q/ application/octet-stream into a buffer
app.use(bodyParser.raw({ type: 'application/octet-stream', limit: '15mb' }));

//POST /ping (recieves grayscale images + switchstate)
app.post('/ping', async (req, res) => {
  console.log('Received a POST request at /ping');

  //read headers
  const switchState = req.headers['switch-state'];
  console.log('Switch State: ', switchState);

  //update switch state
    lastSwitchState = switchState;
  
  

  //if no image data is received, return an error
  if (!req.body || req.body.length === 0) {
    return res.status(400).json({ message: 'No data received' });
  }

  //image dimensions
  const width = 160;
  const height = 120;
 
  try {
    //Convert the raw grayscale data to PNG using Sharp in-memory
    await sharp(req.body, {
      raw: {
        width: width,
        height: height,
        channels: 1
      }
    })
    .png()
    .toFile('output.png'); //saves the PNG to disk

    console.log('Saved output.png successfully!');

    //respond + list switch state
    res.status(200).json({ 
      message: 'Image received and converted successfully',
      switchState
    });

  } catch (err) {
    console.error('Failed to process image:', err);
    res.status(500).json({ message: 'Failed to convert image' });
  }
});

//GET route for switchstate(returns as JSON)
app.get('/switchState', (req, res) => {
  res.json({ switchState: lastSwitchState });
});

//serve static files in directory
app.use(express.static(__dirname));

//start server
app.listen(PORT, "192.168.1.213", () => {
  console.log(`HTTP server is running on http://192.168.1.213:${PORT}`);
});
