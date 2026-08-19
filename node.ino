/*
  ============================================================
  M5Stack ATOM LITE
  I2C CLUSTER NODE
  VERSION 0.6.4
  ============================================================

  I2C ADDRESS:
      0x10

  GROVE:
      GPIO26
      GPIO32

  ONBOARD RGB LED:
      GPIO27

  DISCOVERY:

      Coordinator:
          02 00

      Node:
          81 SEQ 0C
          NODE_ID[4]
          HARDWARE_ID[8]

  Discovery sequence is NOT validated by coordinator.

  The node maintains its own sequence counter.

  ============================================================
*/

#include <M5Atom.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>


// ============================================================
// VERSION
// ============================================================

#define FW_MAJOR 0
#define FW_MINOR 6
#define FW_PATCH 4

#define VERSION "0.6.4"


// ============================================================
// I2C
// ============================================================

#define NODE_ADDR 0x10

#define I2C_SDA 26
#define I2C_SCL 32


// ============================================================
// COMMANDS
// ============================================================

#define CMD_GET_ID       0x02
#define CMD_SET_NODE_ID  0x03
#define CMD_SET_LED      0x04
#define CMD_REBOOT       0x05
#define CMD_WIFI_RESET   0x06
#define CMD_IDENTIFY     0x11


// ============================================================
// RESPONSES
// ============================================================

#define RESP_ACK 0x80
#define RESP_ID  0x81


// ============================================================
// BUFFER
// ============================================================

#define RX_BUFFER_SIZE 32
#define TX_BUFFER_SIZE 32


// ============================================================
// GLOBALS
// ============================================================

Preferences prefs;

volatile bool commandPending = false;

volatile uint8_t commandLength = 0;

volatile uint8_t commandBuffer[
  RX_BUFFER_SIZE
];

uint8_t responseBuffer[
  TX_BUFFER_SIZE
];

uint8_t responseLength = 0;

uint8_t sequenceCounter = 1;

uint32_t nodeId = 0;

uint64_t hardwareId = 0;

bool identifyActive = false;

uint32_t identifyUntil = 0;

uint8_t ledR = 0;
uint8_t ledG = 0;
uint8_t ledB = 0;


// ============================================================
// LED
// ============================================================

void setLED(
  uint8_t r,
  uint8_t g,
  uint8_t b
)
{
  ledR = r;
  ledG = g;
  ledB = b;

  uint32_t color =
    ((uint32_t)r << 16) |
    ((uint32_t)g << 8) |
    b;

  /*
     M5Atom library API.
  */

  M5.dis.drawpix(
    0,
    color
  );
}


// ============================================================
// HARDWARE ID
// ============================================================

uint64_t getHardwareId()
{
  uint64_t id = ESP.getEfuseMac();

  return id;
}


// ============================================================
// RESPONSE BUILDER
// ============================================================

void buildIdResponse()
{
  uint32_t id =
    nodeId;

  uint64_t hw =
    hardwareId;

  uint8_t seq =
    sequenceCounter++;


  /*
     Response:

       0       81
       1       sequence
       2       0C
       3-6     node ID
       7-14    hardware ID
  */

  responseLength =
    15;


  responseBuffer[0] =
    RESP_ID;

  responseBuffer[1] =
    seq;

  responseBuffer[2] =
    12;


  memcpy(
    &responseBuffer[3],
    &id,
    4
  );


  memcpy(
    &responseBuffer[7],
    &hw,
    8
  );
}


// ============================================================
// ACK
// ============================================================

void buildAck(
  uint8_t command,
  uint8_t sequence
)
{
  responseLength = 3;

  responseBuffer[0] =
    RESP_ACK;

  responseBuffer[1] =
    sequence;

  responseBuffer[2] =
    command;
}


// ============================================================
// I2C RECEIVE
//
// IMPORTANT:
//
// Keep this function extremely small.
//
// Do NOT perform Serial.print()
// Do NOT call Preferences
// Do NOT call WiFi
// Do NOT call M5 functions
// Do NOT delay
//
// ============================================================

void receiveEvent(
  int count
)
{
  uint8_t n = 0;

  while (
    Wire.available() &&
    n < RX_BUFFER_SIZE
  )
  {
    commandBuffer[n++] =
      Wire.read();
  }

  commandLength =
    n;

  commandPending =
    true;
}


// ============================================================
// I2C REQUEST
//
// Also intentionally small.
// ============================================================

void requestEvent()
{
  if (
    responseLength > 0
  )
  {
    Wire.write(
      responseBuffer,
      responseLength
    );
  }
  else
  {
    uint8_t zero = 0;

    Wire.write(
      &zero,
      1
    );
  }
}


// ============================================================
// PROCESS COMMAND
//
// This runs from loop(), NOT interrupt context.
// ============================================================

void processCommand()
{
  if (!commandPending)
    return;


  /*
     Make a local copy.

     commandBuffer is volatile because it is written
     by the I2C callback.
  */

  uint8_t localBuffer[
    RX_BUFFER_SIZE
  ];

  uint8_t localLength;


  noInterrupts();

  localLength =
    commandLength;

  if (
    localLength >
    RX_BUFFER_SIZE
  )
  {
    localLength =
      RX_BUFFER_SIZE;
  }

  memcpy(
    localBuffer,
    (const void*)commandBuffer,
    localLength
  );

  commandPending =
    false;

  interrupts();


  if (
    localLength < 1
  )
    return;


  uint8_t cmd =
    localBuffer[0];


  uint8_t seq = 0;

  uint8_t payloadLength = 0;


  if (
    localLength >= 2
  )
  {
    seq =
      localBuffer[1];
  }


  if (
    localLength >= 3
  )
  {
    payloadLength =
      localBuffer[2];
  }


  Serial.printf(
    "I2C CMD 0x%02X SEQ=%u LEN=%u\n",
    cmd,
    seq,
    payloadLength
  );


  // ==========================================================
  // GET ID
  // ==========================================================

  if (
    cmd ==
    CMD_GET_ID
  )
  {
    /*
       Discovery does not depend on the incoming sequence.
    */

    buildIdResponse();

    return;
  }


  // ==========================================================
  // SET NODE ID
  // ==========================================================

  if (
    cmd ==
    CMD_SET_NODE_ID
  )
  {
    if (
      localLength >= 7 &&
      payloadLength == 4
    )
    {
      uint32_t newId;

      memcpy(
        &newId,
        &localBuffer[3],
        4
      );


      nodeId =
        newId;


      prefs.putUInt(
        "nodeId",
        nodeId
      );


      Serial.printf(
        "NODE ID SET TO %lu\n",
        (unsigned long)nodeId
      );


      buildAck(
        cmd,
        seq
      );
    }

    return;
  }


  // ==========================================================
  // SET LED
  // ==========================================================

  if (
    cmd ==
    CMD_SET_LED
  )
  {
    if (
      localLength >= 6 &&
      payloadLength == 3
    )
    {
      uint8_t r =
        localBuffer[3];

      uint8_t g =
        localBuffer[4];

      uint8_t b =
        localBuffer[5];


      setLED(
        r,
        g,
        b
      );


      buildAck(
        cmd,
        seq
      );
    }

    return;
  }


  // ==========================================================
  // REBOOT
  // ==========================================================

  if (
    cmd ==
    CMD_REBOOT
  )
  {
    buildAck(
      cmd,
      seq
    );

    delay(20);

    ESP.restart();

    return;
  }


  // ==========================================================
  // WIFI RESET
  // ==========================================================

  if (
    cmd ==
    CMD_WIFI_RESET
  )
  {
    WiFi.disconnect(
      true
    );

    delay(100);

    buildAck(
      cmd,
      seq
    );

    return;
  }


  // ==========================================================
  // IDENTIFY
  // ==========================================================

  if (
    cmd ==
    CMD_IDENTIFY
  )
  {
    if (
      localLength >= 4 &&
      payloadLength == 1
    )
    {
      uint8_t seconds =
        localBuffer[3];

      if (
        seconds == 0
      )
        seconds = 3;

      identifyActive =
        true;

      identifyUntil =
        millis() +
        ((uint32_t)seconds * 1000UL);

      buildAck(
        cmd,
        seq
      );
    }

    return;
  }
}


// ============================================================
// IDENTIFY UPDATE
// ============================================================

void updateIdentify()
{
  if (
    !identifyActive
  )
    return;


  if (
    millis() >=
    identifyUntil
  )
  {
    identifyActive =
      false;

    setLED(
      ledR,
      ledG,
      ledB
    );

    return;
  }


  /*
     Flash yellow.
  */

  static uint32_t lastFlash = 0;

  static bool state = false;


  if (
    millis() -
    lastFlash >=
    200
  )
  {
    lastFlash =
      millis();

    state =
      !state;


    if (state)
    {
      setLED(
        255,
        255,
        0
      );
    }
    else
    {
      setLED(
        0,
        0,
        0
      );
    }
  }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  /*
     Start Atom.

     No long-running work is performed from interrupts.
  */

  M5.begin(
    true,
    false,
    true
  );


  Serial.begin(
    115200
  );


  delay(300);


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.printf(
    "       ATOM I2C NODE v%s\n",
    VERSION
  );

  Serial.println(
    "========================================"
  );


  /*
     Preferences.
  */

  prefs.begin(
    "node",
    false
  );


  nodeId =
    prefs.getUInt(
      "nodeId",
      0
    );


  hardwareId =
    getHardwareId();


  Serial.printf(
    "Node ID     : %lu\n",
    (unsigned long)nodeId
  );

  Serial.printf(
    "Hardware ID : %016llX\n",
    (unsigned long long)hardwareId
  );


  /*
     Initial LED.
  */

  setLED(
    0,
    0,
    8
  );


  /*
     I2C slave.

     Grove pins:
         GPIO26
         GPIO32
  */

  Wire.begin(
    NODE_ADDR,
    I2C_SDA,
    I2C_SCL,
    100000
  );


  Wire.onReceive(
    receiveEvent
  );

  Wire.onRequest(
    requestEvent
  );


  Serial.println(
    "I2C address : 0x10"
  );

  Serial.printf(
    "I2C SDA     : GPIO%d\n",
    I2C_SDA
  );

  Serial.printf(
    "I2C SCL     : GPIO%d\n",
    I2C_SCL
  );


  Serial.println(
    "Node waiting for coordinator..."
  );


  setLED(
    0,
    0,
    16
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  M5.update();


  processCommand();


  updateIdentify();


  /*
     Button can be used as a simple local
     heartbeat/test.
  */

  if (
    M5.Btn.wasPressed()
  )
  {
    Serial.println(
      "LOCAL BUTTON"
    );

    setLED(
      0,
      32,
      0
    );
  }


  delay(1);
}
