#include <SPI.h>
#include <can.h>
#include <mcp2515.h>

#define ID_MFL 0x5c3
#define ID_TUERSENSOREN 0x470
#define ID_BATTERIESPANNUNG 0x571
#define ID_ZUENDUNGSSTATUS 0x2c3
#define ID_LICHTSTATUS 0x635
#define ID_GESCHWINDIGKEIT 0x351
#define ID_AUDIOSOURCE 0x661
#define ID_ZENTRALVERRIEGELUNG 0x291
#define ID_FIS_LINE1 0x363
#define ID_FIS_LINE2 0x365

/*
 * On Arduino Nano: 
 * MISO PIN 12
 * MOSI PIN 11
 * CLK PIN 13
 */

/*
 * Source on: https://github.com/autowp/arduino-mcp2515
 */

/*
 * struct can_frame {
 * uint32_t can_id;  // 32 bit CAN_ID + EFF/RTR/ERR flags 
 * uint8_t  can_dlc;
 * uint8_t  data[8];
 * };
 */

uint32_t FILTEREDIDS[10]; 
bool filterActivated = false;
bool printMessagesForPCAnalysis = true;

struct can_frame canMsg;
MCP2515 mcp2515(10); // setup with CS PIN 10

struct can_frame canMsg1;
struct can_frame canMsg2;

void setup() {
  canMsg1.can_id  = 0x5c3;
  canMsg1.can_dlc = 8;
  canMsg1.data[0] = 0x39;
  canMsg1.data[1] = 0x06;
  canMsg1.data[2] = 0x32;
  canMsg1.data[3] = 0xFA;
  canMsg1.data[4] = 0x26;
  canMsg1.data[5] = 0x8E;
  canMsg1.data[6] = 0xBE;
  canMsg1.data[7] = 0x86;

  canMsg2.can_id  = 0x2c3;
  canMsg2.can_dlc = 8;
  canMsg2.data[0] = 0x0B;
  canMsg2.data[1] = 0x00;
  canMsg2.data[2] = 0x00;
  canMsg2.data[3] = 0x08;
  canMsg2.data[4] = 0x01;
  canMsg2.data[5] = 0x00;
  canMsg2.data[6] = 0x00;
  canMsg2.data[7] = 0xA0;

  
  Serial.begin(115200);
  SPI.begin();
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_100KBPS, MCP_8MHZ);
  mcp2515.setLoopbackMode();

  // set up to 10 IDs to be filtered
  FILTEREDIDS[0] = 0x5c3;
  FILTEREDIDS[1] = 0x470;
  FILTEREDIDS[2] = 0x571;
  FILTEREDIDS[3] = 0x2c3;
  FILTEREDIDS[4] = 0x635;
  FILTEREDIDS[5] = 0x351;
  FILTEREDIDS[6] = 0x661;
  FILTEREDIDS[7] = 0x00;
  FILTEREDIDS[8] = 0x00;
  FILTEREDIDS[9] = 0x00;
  
  Serial.println("------- CAN Read (at 100kbps) ----------");
  Serial.println("ID  DLC   DATA"); // DLC = Data Length Code
}
    

void loop() {
  
  //sendCanMessage(&canMsg1); // remove later --> test purpose
  sendRandomCanMessage();
  //Serial.println("Message sent"); // remove later --> test purpose
  delay(50); // remove later --> test purpose

  if (Serial.available() > 0) 
  {
      String serialCommand = Serial.readString();    
      serialCommand.trim(); 
      if (serialCommand == "requestCANSpeed")
      { 
         Serial.println("##CanSPEED##100kbps");
      }               
  }
  
  if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) 
  {  
    if (filterActivated && checkIfFilteredId(canMsg.can_id))
    {
      printCanMessage(canMsg);
      handleData(canMsg);
    }
    else if (!filterActivated)
    {
      printCanMessage(canMsg);
      handleData(canMsg);
    }
  }
}

void sendRandomCanMessage()
{
  uint8_t randomID = random(11, 22);
  uint8_t randomDLC = random(0, 8);
  
  canMsg2.can_id  = randomID;
  canMsg2.can_dlc = randomDLC;
  canMsg2.data[0] = 0x0B;
  canMsg2.data[1] = 0x00;
  canMsg2.data[2] = 0x00;
  canMsg2.data[3] = 0x08;
  canMsg2.data[4] = 0x01;
  canMsg2.data[5] = 0x00;
  canMsg2.data[6] = 0x00;
  canMsg2.data[7] = 0xA0;

  sendCanMessage(&canMsg2);
}

bool sendCanMessage(struct can_frame *msg)
{
  if (mcp2515.sendMessage(msg) == MCP2515::ERROR_OK) {
    // send success
    return true;
  }
  else {
    Serial.println("Failed to send message.");
    return false;
  }
}

bool checkIfFilteredId(uint32_t id)
{
  bool isFilteredId = false;  
  for (int i = 0; i < 10; i++)
  {
    if (FILTEREDIDS[i] == id)
    {
      isFilteredId = true;
    }
  }
  return isFilteredId;
}

// format: ##START##0x5c3%8%39%07%00%00%00%00%00%00%##END##
void printCanMessage(struct can_frame &canMsg)
{    
    if (printMessagesForPCAnalysis)
    {
      Serial.print("##START##");
      Serial.print(canMsg.can_id, HEX); // print ID
      Serial.print("%"); 
      Serial.print(canMsg.can_dlc, HEX); // print DLC
      Serial.print("%");

      Serial.print(canMsg.data[0], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[1], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[2], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[3], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[4], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[5], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[6], HEX);
      Serial.print("%");
      Serial.print(canMsg.data[7], HEX);      
      
      /*for (int i = 0; i < canMsg.can_dlc; i++)  {  // print the data        
        Serial.print(canMsg.data[i], HEX);        
        Serial.print("%");
      }*/
      
      Serial.print("##END##\n");     
    }
    else
    {
      Serial.print(canMsg.can_id, HEX); // print ID
      Serial.print(" "); 
      Serial.print(canMsg.can_dlc, HEX); // print DLC
      Serial.print(" ");
    
      for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
        
        Serial.print(canMsg.data[i], HEX);
        Serial.print(" ");

      }
      Serial.println();      
    }    
}

void handleData(struct can_frame canMsg)
{  
  if (canMsg.can_id == 0x5c3) // MFL
  {
      uint8_t mfl_data[2];
      mfl_data[0] = canMsg.data[0];
      mfl_data[1] = canMsg.data[1];
      handleMFLAction(mfl_data);
  }

  if (canMsg.can_id == 0x470) // Türsensoren
  {
    handleDoorSensorAction(canMsg.data[1]);
  }

  if (canMsg.can_id == 0x571) // Batteriespannung
  {
      float volt = 5 + (0.05 * canMsg.data[0]);
      Serial.print("Batteriespannung: ");
      Serial.print(volt);
      Serial.print("V\n");
  }

  if (canMsg.can_id == 0x2c3) // Zündungsstatus
  {
    handleIgnitionAction(canMsg.data[0]);
  }
}

void handleMFLAction(uint8_t data[2])
{
  switch (data[0]) // Aktuelles Submenu
  {
    case 0x39: // Radio/AUX
      switch (data[1])
      {
        case 0x00: break; // 39 00 --> empty
        case 0x01: Serial.println("Mode button double press"); break;
        case 0x02: Serial.println("Prev"); break;
        case 0x03: Serial.println("Next"); break;
        case 0x04: Serial.println("ScanUp"); break;
        case 0x05: Serial.println("ScanDown"); break;
        case 0x06: Serial.println("Vol+"); break;
        case 0x07: Serial.println("Vol-"); break;
      }
      break;
    case 0x3C: // Phone
      break;
  }
}

void handleIgnitionAction(uint8_t data)
{ 
  switch (data)
  {
    case 0x10: Serial.println("Kein Schlüssel"); break;
    case 0x11: Serial.println("Schlüssel steckt"); break;
    case 0x01: Serial.println("Lenkradschloss entriegelt"); break;
    case 0x05: Serial.println("Display AN"); break;
    case 0x07: Serial.println("Zündung EIN"); break;
    case 0x0B: Serial.println("Zündung EIN und ANLASSER"); break;
  }
}

void handleDoorSensorAction(uint8_t data)
{
  switch (data)
  {
    case 0x00: Serial.println("Alle Türen zu"); break;
    case 0x01: Serial.println("Vorne links offen"); break;
    case 0x02: Serial.println("Vorne rechts offen"); break;
    case 0x04: Serial.println("Hinten links offen"); break;
    case 0x08: Serial.println("Hinten rechts offen"); break;
    case 0x60: Serial.println("Heckklappe offen"); break;
    case 0x10: Serial.println("Motorhaube offen"); break;
  }
}
