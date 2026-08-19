/*
  ============================================================
  M5Stack ESP32 CORE
  I2C CLUSTER COORDINATOR
  VERSION 0.6.4
  ============================================================

  PA-HUB       : 0x70
  NODE         : 0x10

  I2C SDA      : GPIO21
  I2C SCL      : GPIO22
  I2C SPEED    : 100kHz

  DISCOVERY:

      TX:
          02 00

      RX:
          81 SEQ 0C
          NODE_ID[4]
          HARDWARE_ID[8]

  Discovery sequence is deliberately ignored.

  Buttons:

      A = previous node
      B = scan
      C = next node
  ============================================================
*/

#include <M5Stack.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define VERSION "0.6.4"

#define PAHUB_ADDR 0x70
#define NODE_ADDR  0x10

#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_SPEED 100000

#define MAX_NODES 6

#define CMD_GET_ID       0x02
#define CMD_SET_NODE_ID  0x03
#define CMD_SET_LED      0x04
#define CMD_REBOOT       0x05
#define CMD_WIFI_RESET   0x06
#define CMD_IDENTIFY     0x11

#define RESP_ACK 0x80
#define RESP_ID  0x81


// ============================================================
// NODE STRUCTURE
// ============================================================

struct Node
{
  bool used;
  bool online;

  uint8_t channel;

  uint32_t nodeId;
  uint64_t hardwareId;

  uint8_t ip[4];

  int8_t rssi;

  uint32_t uptime;
  uint32_t heap;

  uint8_t fwMajor;
  uint8_t fwMinor;
  uint8_t fwPatch;

  uint32_t lastSeen;
};


// ============================================================
// GLOBALS
// ============================================================

Node nodes[MAX_NODES];

Preferences prefs;

WebServer server(80);
WebSocketsServer ws(81);

uint8_t commandSequence = 1;

uint32_t nextNodeId = 1;

uint32_t lastScan = 0;
uint32_t lastScreen = 0;

int selectedNode = -1;


// ============================================================
// PA-HUB
// ============================================================

bool selectHub(uint8_t channel)
{
  if (channel >= MAX_NODES)
    return false;

  Wire.beginTransmission(PAHUB_ADDR);

  Wire.write(
    (uint8_t)(1 << channel)
  );

  uint8_t result =
    Wire.endTransmission();

  if (result != 0)
  {
    Serial.printf(
      "CH%d PA-HUB ERROR=%d\n",
      channel,
      result
    );

    return false;
  }

  return true;
}


// ============================================================

void disableHub()
{
  Wire.beginTransmission(PAHUB_ADDR);
  Wire.write((uint8_t)0);
  Wire.endTransmission();
}


// ============================================================
// NODE WRITE
// ============================================================

bool writeNode(
  uint8_t channel,
  const uint8_t *data,
  uint8_t length
)
{
  if (!selectHub(channel))
    return false;

  Wire.beginTransmission(NODE_ADDR);

  Wire.write(
    data,
    length
  );

  uint8_t result =
    Wire.endTransmission();

  Serial.printf(
    "CH%d NODE WRITE RESULT=%d\n",
    channel,
    result
  );

  return result == 0;
}


// ============================================================
// NODE READ
// ============================================================

uint8_t readNode(
  uint8_t channel,
  uint8_t *buffer,
  uint8_t requested
)
{
  if (!selectHub(channel))
    return 0;

  uint8_t received =
    Wire.requestFrom(
      NODE_ADDR,
      requested
    );

  Serial.printf(
    "CH%d REQUEST=%u RECEIVED=%u\n",
    channel,
    requested,
    received
  );

  uint8_t count = 0;

  uint32_t start = millis();

  while (
    count < received &&
    count < requested &&
    millis() - start < 100
  )
  {
    if (Wire.available())
    {
      buffer[count++] =
        Wire.read();
    }
  }

  Serial.printf(
    "CH%d ACTUALLY READ=%u\n",
    channel,
    count
  );

  return count;
}


// ============================================================
// DISCOVERY
// ============================================================

bool discoverNode(
  uint8_t channel,
  uint32_t &nodeId,
  uint64_t &hardwareId
)
{
  uint8_t tx[2];

  tx[0] = CMD_GET_ID;
  tx[1] = 0;

  Serial.println();

  Serial.printf(
    "CH%d GET_ID\n",
    channel
  );

  Serial.println(
    "GET_ID TX: 02 00"
  );

  if (!writeNode(
        channel,
        tx,
        sizeof(tx)
      ))
  {
    Serial.println(
      "GET_ID WRITE FAILED"
    );

    return false;
  }

  delay(5);

  /*
     3-byte header
     +
     12-byte payload
     =
     15 bytes
  */

  uint8_t rx[15];

  uint8_t count =
    readNode(
      channel,
      rx,
      sizeof(rx)
    );

  if (count < 3)
  {
    Serial.println(
      "RESPONSE TOO SHORT"
    );

    return false;
  }

  Serial.print(
    "  RX:"
  );

  for (
    uint8_t i = 0;
    i < count;
    i++
  )
  {
    Serial.printf(
      " %02X",
      rx[i]
    );
  }

  Serial.println();

  if (rx[0] != RESP_ID)
  {
    Serial.printf(
      "BAD RESPONSE TYPE=0x%02X\n",
      rx[0]
    );

    return false;
  }

  /*
     rx[1] is the node's sequence.

     DO NOT validate it.
  */

  Serial.printf(
    "  NODE SEQ=%u (IGNORED)\n",
    rx[1]
  );

  uint8_t payloadLength =
    rx[2];

  if (payloadLength != 12)
  {
    Serial.printf(
      "BAD PAYLOAD LENGTH=%u\n",
      payloadLength
    );

    return false;
  }

  if (count < 15)
  {
    Serial.printf(
      "INCOMPLETE RESPONSE %u/15\n",
      count
    );

    return false;
  }

  memcpy(
    &nodeId,
    &rx[3],
    4
  );

  memcpy(
    &hardwareId,
    &rx[7],
    8
  );

  Serial.printf(
    "  NODE ID=%lu\n",
    (unsigned long)nodeId
  );

  Serial.printf(
    "  HARDWARE ID=%016llX\n",
    (unsigned long long)hardwareId
  );

  return true;
}


// ============================================================
// FIND NODE
// ============================================================

int findNode(
  uint64_t hardwareId
)
{
  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    if (
      nodes[i].used &&
      nodes[i].hardwareId ==
      hardwareId
    )
    {
      return i;
    }
  }

  return -1;
}


// ============================================================
// EMPTY NODE
// ============================================================

int findEmptyNode()
{
  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    if (!nodes[i].used)
      return i;
  }

  return -1;
}


// ============================================================
// SET NODE ID
// ============================================================

bool assignNodeId(
  uint8_t channel,
  uint32_t id
)
{
  uint8_t seq =
    commandSequence++;

  uint8_t tx[7];

  tx[0] = CMD_SET_NODE_ID;
  tx[1] = seq;
  tx[2] = 4;

  memcpy(
    &tx[3],
    &id,
    4
  );

  if (!writeNode(
        channel,
        tx,
        sizeof(tx)
      ))
  {
    return false;
  }

  delay(5);

  uint8_t rx[8];

  uint8_t count =
    readNode(
      channel,
      rx,
      sizeof(rx)
    );

  if (count < 3)
    return false;

  if (rx[0] != RESP_ACK)
    return false;

  /*
     Sequence mismatch is only a warning.
  */

  if (rx[1] != seq)
  {
    Serial.printf(
      "SET_ID SEQ WARNING sent=%u received=%u\n",
      seq,
      rx[1]
    );
  }

  if (rx[2] != CMD_SET_NODE_ID)
    return false;

  return true;
}


// ============================================================
// SCAN
// ============================================================

void scanNodes()
{
  Serial.println();
  Serial.println(
    "========================================"
  );
  Serial.println(
    "        I2C CLUSTER SCAN v0.6.4"
  );
  Serial.println(
    "========================================"
  );

  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    if (nodes[i].used)
      nodes[i].online = false;
  }

  for (
    uint8_t channel = 0;
    channel < MAX_NODES;
    channel++
  )
  {
    Serial.println();
    Serial.printf(
      "CH %d\n",
      channel
    );

    if (!selectHub(channel))
    {
      Serial.println(
        "  PA-HUB CHANNEL SELECT FAILED"
      );

      continue;
    }

    Serial.println(
      "  PA-HUB CHANNEL SELECTED"
    );

    uint32_t nodeId = 0;
    uint64_t hardwareId = 0;

    if (!discoverNode(
          channel,
          nodeId,
          hardwareId
        ))
    {
      Serial.println(
        "  NO NODE"
      );

      continue;
    }

    int index =
      findNode(
        hardwareId
      );

    if (index < 0)
    {
      index =
        findEmptyNode();

      if (index < 0)
      {
        Serial.println(
          "  NODE TABLE FULL"
        );

        continue;
      }

      memset(
        &nodes[index],
        0,
        sizeof(Node)
      );

      nodes[index].used = true;

      nodes[index].hardwareId =
        hardwareId;

      if (nodeId == 0)
      {
        uint32_t assigned =
          nextNodeId++;

        if (
          assignNodeId(
            channel,
            assigned
          )
        )
        {
          nodeId =
            assigned;

          prefs.putUInt(
            "nextId",
            nextNodeId
          );

          Serial.printf(
            "  ASSIGNED NODE ID=%lu\n",
            (unsigned long)assigned
          );
        }
        else
        {
          Serial.println(
            "  NODE ID ASSIGNMENT FAILED"
          );
        }
      }
    }

    nodes[index].channel =
      channel;

    nodes[index].nodeId =
      nodeId;

    nodes[index].hardwareId =
      hardwareId;

    nodes[index].online =
      true;

    nodes[index].lastSeen =
      millis();

    Serial.println(
      "  *** NODE ONLINE ***"
    );
  }

  disableHub();

  Serial.println();
  Serial.println(
    "========================================"
  );
  Serial.println(
    "SCAN COMPLETE"
  );
  Serial.println(
    "========================================"
  );
}


// ============================================================
// JSON STATE
// ============================================================

void sendState(
  uint8_t client = 255
)
{
  JsonDocument doc;

  doc["version"] =
    VERSION;

  JsonArray a =
    doc["nodes"].to<JsonArray>();

  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    JsonObject n =
      a.add<JsonObject>();

    n["slot"] = i;
    n["channel"] =
      nodes[i].channel;

    n["used"] =
      nodes[i].used;

    n["online"] =
      nodes[i].online;

    n["nodeId"] =
      nodes[i].nodeId;

    char hw[24];

    snprintf(
      hw,
      sizeof(hw),
      "%016llX",
      (unsigned long long)
      nodes[i].hardwareId
    );

    n["hardwareId"] =
      hw;

    n["rssi"] =
      nodes[i].rssi;

    n["heap"] =
      nodes[i].heap;

    n["uptime"] =
      nodes[i].uptime;
  }

  String output;

  serializeJson(
    doc,
    output
  );

  if (client == 255)
    ws.broadcastTXT(output);
  else
    ws.sendTXT(
      client,
      output
    );
}


// ============================================================
// ROOT
// ============================================================

void handleRoot()
{
  server.send(
    200,
    "text/plain",
    "I2C Cluster Coordinator v0.6.4"
  );
}


// ============================================================
// WEBSOCKET
// ============================================================

void wsEvent(
  uint8_t client,
  WStype_t type,
  uint8_t *payload,
  size_t length
)
{
  if (
    type != WStype_TEXT
  )
    return;

  JsonDocument doc;

  if (
    deserializeJson(
      doc,
      payload,
      length
    )
  )
    return;

  const char *cmd =
    doc["command"];

  if (!cmd)
    return;

  if (
    !strcmp(
      cmd,
      "scan"
    )
  )
  {
    scanNodes();

    sendState(
      client
    );

    return;
  }

  /*
     LED command.
  */

  if (
    !strcmp(
      cmd,
      "led"
    )
  )
  {
    uint8_t channel =
      doc["channel"] | 0;

    uint8_t seq =
      commandSequence++;

    uint8_t tx[6];

    tx[0] = CMD_SET_LED;
    tx[1] = seq;
    tx[2] = 3;

    tx[3] =
      (uint8_t)(doc["r"] | 0);

    tx[4] =
      (uint8_t)(doc["g"] | 0);

    tx[5] =
      (uint8_t)(doc["b"] | 0);

    writeNode(
      channel,
      tx,
      sizeof(tx)
    );

    return;
  }

  /*
     IDENTIFY.
  */

  if (
    !strcmp(
      cmd,
      "identify"
    )
  )
  {
    uint8_t channel =
      doc["channel"] | 0;

    uint8_t seconds =
      doc["seconds"] | 3;

    uint8_t seq =
      commandSequence++;

    uint8_t tx[4];

    tx[0] =
      CMD_IDENTIFY;

    tx[1] =
      seq;

    tx[2] =
      1;

    tx[3] =
      seconds;

    writeNode(
      channel,
      tx,
      sizeof(tx)
    );

    return;
  }
}


// ============================================================
// SCREEN
// ============================================================

void updateScreen()
{
  M5.Lcd.fillScreen(
    BLACK
  );

  M5.Lcd.setTextColor(
    WHITE
  );

  M5.Lcd.setTextSize(
    1
  );

  M5.Lcd.setCursor(
    4,
    4
  );

  M5.Lcd.printf(
    "I2C CLUSTER v%s",
    VERSION
  );

  M5.Lcd.drawLine(
    0,
    18,
    320,
    18,
    WHITE
  );

  M5.Lcd.setCursor(
    4,
    23
  );

  M5.Lcd.print(
    "CH   NODE ID     STATUS"
  );

  for (
    int ch = 0;
    ch < MAX_NODES;
    ch++
  )
  {
    int y =
      42 +
      ch * 26;

    int index = -1;

    for (
      int i = 0;
      i < MAX_NODES;
      i++
    )
    {
      if (
        nodes[i].used &&
        nodes[i].channel == ch
      )
      {
        index = i;
        break;
      }
    }

    M5.Lcd.setCursor(
      4,
      y
    );

    M5.Lcd.printf(
      "%d",
      ch
    );

    if (index < 0)
    {
      M5.Lcd.setCursor(
        35,
        y
      );

      M5.Lcd.print(
        "--------"
      );

      M5.Lcd.setCursor(
        125,
        y
      );

      M5.Lcd.print(
        "NO NODE"
      );
    }
    else
    {
      M5.Lcd.setCursor(
        35,
        y
      );

      M5.Lcd.printf(
        "%08lu",
        (unsigned long)
        nodes[index].nodeId
      );

      M5.Lcd.setCursor(
        125,
        y
      );

      if (
        nodes[index].online
      )
      {
        M5.Lcd.print(
          "ONLINE"
        );
      }
      else
      {
        M5.Lcd.print(
          "OFFLINE"
        );
      }
    }
  }

  int online = 0;

  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    if (nodes[i].online)
      online++;
  }

  M5.Lcd.drawLine(
    0,
    202,
    320,
    202,
    WHITE
  );

  M5.Lcd.setCursor(
    4,
    208
  );

  M5.Lcd.printf(
    "ONLINE %d/%d",
    online,
    MAX_NODES
  );

  M5.Lcd.setCursor(
    4,
    222
  );

  M5.Lcd.print(
    "A=PREV  B=SCAN  C=NEXT"
  );
}


// ============================================================
// BUTTONS
// ============================================================

void processButtons()
{
  M5.update();

  if (
    M5.BtnB.wasPressed()
  )
  {
    Serial.println(
      "BUTTON B: MANUAL SCAN"
    );

    scanNodes();

    sendState();

    updateScreen();
  }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  M5.begin();

  Serial.begin(
    115200
  );

  delay(300);

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.printf(
    " I2C COORDINATOR v%s\n",
    VERSION
  );

  Serial.println(
    "========================================"
  );

  prefs.begin(
    "coord",
    false
  );

  nextNodeId =
    prefs.getUInt(
      "nextId",
      1
    );

  if (
    nextNodeId == 0
  )
    nextNodeId = 1;

  for (
    int i = 0;
    i < MAX_NODES;
    i++
  )
  {
    memset(
      &nodes[i],
      0,
      sizeof(Node)
    );

    nodes[i].channel =
      i;
  }

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );

  Wire.setClock(
    I2C_SPEED
  );

  Serial.printf(
    "I2C SDA=%d SCL=%d\n",
    I2C_SDA,
    I2C_SCL
  );

  WiFi.mode(
    WIFI_AP
  );

  WiFi.softAP(
    "I2C-NODE-COORDINATOR",
    "12345678"
  );

  Serial.print(
    "Coordinator IP: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  server.on(
    "/",
    handleRoot
  );

  server.begin();

  ws.begin();

  ws.onEvent(
    wsEvent
  );

  delay(500);

  scanNodes();

  updateScreen();

  lastScan =
    millis();

  lastScreen =
    millis();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  server.handleClient();

  ws.loop();

  processButtons();

  if (
    millis() -
    lastScan >=
    5000
  )
  {
    lastScan =
      millis();

    scanNodes();

    sendState();
  }

  if (
    millis() -
    lastScreen >=
    1000
  )
  {
    lastScreen =
      millis();

    updateScreen();
  }

  delay(2);
}
