#include <SPI.h>
#include <can.h>
#include <mcp2515.h>
#include <SoftwareSerial.h>

#define ID_MFL 0x5c3
#define ID_TUERSENSOREN 0x470
#define ID_BATTERIESPANNUNG 0x571
#define ID_ZUENDUNGSSTATUS 0x2c3
#define ID_LICHTSTATUS 0x635
#define ID_GESCHWINDIGKEIT 0x351
#define ID_AUDIOSOURCE 0x661
#define ID_ZENTRALVERRIEGELUNG 0x291
#define ID_FIS_LINE1_RADIO 0x363 // 363
#define ID_FIS_LINE2_RADIO 0x365 // 365
#define ID_FIS_LINE1_FSE 0x667
#define ID_FIS_LINE2_FSE 0x66B

#define PIN_PLAYPAUSE 6
#define PIN_NEXT 7
#define PIN_PREVIOUS 5


// usable interrupt_pins on arduino nano : D2 and D3
#define PIN_INTERRUPT 2 

#define PIN_CS_MCP2515 10
#define PIN_RX_HC05 8 // connect to TX of HC05 board
#define PIN_TX_HC05 9 // connect to RX of HC05 board (through voltage divider)
#define PIN_HC05_EN 3
#define PIN_HC05_STATE A0
#define PIN_HC05_POWER 4

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

bool printMessagesForPCAnalysis = true;
bool printCANMessages = false;
bool usingHC05inATCommandMode = false;

struct can_frame canMsg_in;
struct can_frame canMsg_out;
MCP2515 mcp2515(PIN_CS_MCP2515); 
volatile bool interrupt = false;

uint16_t MAXWAITFISSEND = 2000;
bool FISSentLine1Recently = false;
unsigned long lastTimeLine1WasSent = 0;
bool FISSentLine2Recently = false;
unsigned long lastTimeLine2WasSent = 0;
unsigned long waitUntilFISLine1Scroll = 0;
unsigned long ScrollIntervalFISLine1 = 500;
unsigned long waitUntilFISLine2Scroll = 0;
unsigned long ScrollIntervalFISLine2 = 500;
char completeFISLine1[100] = "Zeile 1 ";
char completeFISLine2[100] = "Zeile 2 ";
char actualFISLine1[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
char actualFISLine2[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
uint8_t FIS1Position = 0;
uint8_t FIS2Position = 0;

uint8_t mRadioMode = 0;
uint8_t mIgnitionStatus = 1;
float mSpeed = 0;
uint8_t mDirection = 0;
float mVolt = 0;

SoftwareSerial serialHC05(PIN_RX_HC05, PIN_TX_HC05); // RX, TX

void setup() {  

  pinMode(PIN_PLAYPAUSE, OUTPUT);
  digitalWrite(PIN_PLAYPAUSE, LOW);
  pinMode(PIN_NEXT, OUTPUT);
  digitalWrite(PIN_NEXT, LOW);
  pinMode(PIN_PREVIOUS, OUTPUT);
  digitalWrite(PIN_PREVIOUS, LOW);

  pinMode(PIN_HC05_EN, OUTPUT);
  pinMode(PIN_HC05_STATE, INPUT);
  digitalWrite(PIN_HC05_EN, LOW);

  // turn on bc547b to set VCC of HC05 to 5V
  pinMode(PIN_HC05_POWER, OUTPUT);
  digitalWrite(PIN_HC05_POWER, HIGH);
  
  Serial.begin(115200);  

  attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT), irqHandler, FALLING);
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_100KBPS, MCP_8MHZ);
  while (mcp2515.setNormalMode() != MCP2515::ERROR_OK)
  {
    Serial.println("Failed to start MCP2515 communication!");
    delay(1000);
  }  

  serialHC05.begin(9600); // Default communication rate of the HC05  
  
  Serial.println("------- CAN Read (at 100kbps) ----------");
  Serial.println("ID  DLC   DATA"); // DLC = Data Length Code

  // Wake up BT module
  PressPlayPauseButton(100);  
}

void irqHandler() {
    interrupt = true;
}

void loop() {       

  if (mRadioMode < 3 && mRadioMode != 0) // send FIS only in MP3/CD mode
  {
    // refresh FIS as soon as another packet with the same ID arrived
    if (FISSentLine1Recently == true)    
    {      
      FISSentLine1Recently = false;

      //String line1 = "Castle of GLASS - LinkinPark";
      //line1.toCharArray(completeFISLine1, line1.length() + 1);
      PrintFISLine1(); 
    } 
    // or refresh FIS if no packet was sent for MAXWAITFISSEND
    else if ((lastTimeLine1WasSent + MAXWAITFISSEND) < millis())
    {
      //String line1 = "Castle of GLASS - LinkinPark";
      //line1.toCharArray(completeFISLine1, line1.length() + 1);
      PrintFISLine1();
    }
    if (FISSentLine2Recently == true)
    {
      FISSentLine2Recently = false;

      //String line2 = "Zeile zwei";
      //line2.toCharArray(completeFISLine2, line2.length() + 1);
      PrintFISLine2();      
    }
    else if ((lastTimeLine2WasSent + MAXWAITFISSEND) < millis())
    {
      //String line2 = "Zeile zwei";
      //line2.toCharArray(completeFISLine2, line2.length() + 1);
      PrintFISLine2();  
    }
  }
  
  if (Serial.available() > 0) 
  {
      String serialCommand = Serial.readString();    
      serialCommand.trim(); 
      handleSerialCommand(serialCommand);      
  }  

  if (serialHC05.available() > 0)
  {
    String serialHC05Message = serialHC05.readString();
    serialHC05Message.trim();
    handleSerialHC05Message(serialHC05Message);
  }

  if (interrupt) 
  {
        interrupt = false;        

        uint8_t irq = mcp2515.getInterrupts();

        if (irq & MCP2515::CANINTF_RX0IF) {
          bool error = mcp2515.readMessage(MCP2515::RXB0, &canMsg_in);
            if (error == MCP2515::ERROR_OK) {
                // frame contains received from RXB0 message
                if (printCANMessages)
                {
                  printCanMessage(canMsg_in);
                }
                handleData(canMsg_in);
            }
            else
            {
              Serial.print("Error reading message from MCP2515: ");
              Serial.print(error);
              Serial.print("\n");
            }
        }

        if (irq & MCP2515::CANINTF_RX1IF) {
          bool error = mcp2515.readMessage(MCP2515::RXB1, &canMsg_in);
            if (error == MCP2515::ERROR_OK) {
                // frame contains received from RXB1 message
                if (printCANMessages)
                {
                  printCanMessage(canMsg_in);
                }
                handleData(canMsg_in);
            }
            else
            {
              Serial.print("Error reading message from MCP2515: ");
              Serial.print(error);
              Serial.print("\n");
            }
        }
        mcp2515.clearInterrupts();       
    }
}

void handleSerialHC05Message(String hc05message)
{
  Serial.print("HC05: ");
  Serial.println(hc05message);
  uint8_t tempLength = hc05message.length();
  char temp[100];
  hc05message.toCharArray(temp, 100);
  // FIS line1
  if (temp[0] == '%' && temp[1] == '%')
  {    
    for (uint8_t i = 0; i < tempLength - 1; i++)
    {
      temp[i] = temp[i+2];
    }
    for (uint8_t i = 0; i < 100; i++) {
      completeFISLine1[i] = temp[i];
    }
  }
  // FIS line2
  if (temp[0] == '&' && temp[1] == '&')
  {  
    for (uint8_t i = 0; i < tempLength - 1; i++)
    {
      temp[i] = temp[i+2];
    }
    for (uint8_t i = 0; i < 100; i++) {
      completeFISLine2[i] = temp[i];
    }
  }    
}

void handleSerialCommand(String serialCommand)
{
      if (serialCommand == "requestCANSpeed")
      { 
         Serial.println("##CanSPEED##100kbps");
      }     
      if (serialCommand == "requestSTATUS")
      {
        Serial.print("STATUS##");  
        Serial.print(mRadioMode);
        Serial.print("##"); 
        Serial.print(mIgnitionStatus);
        Serial.print("##"); 
        Serial.print(mVolt);
        Serial.print("##STATUSENDE");        
      }
      
      // format: SENDCAN?0x5c3?39?07?00?00?00?00?00?00
      /*if (serialCommand.indexOf("SENDCAN" > 0)
      {
         uint32_t id = stringToNumber(getValue(serialCommand, "?", 1));         
      }*/
      if (serialCommand == "executePLAYPAUSE")
      {
        PressPlayPauseButton(100);        
      }
      if (serialCommand == "executeNEXT")
      {
        PressNextButton(100); 
      }
      if (serialCommand == "executePREVIOUS")
      {
        PressPreviousButton(100); 
      }
      if (serialCommand == "startHC05ATMode")
      {        
        startHC05ATMode();
      }
      if (serialCommand == "endHC05ATMode")
      {        
        endHC05ATMode();
      }
      if (serialCommand == "enablePrintCANMessages")
      {
        printCANMessages = true;
        Serial.println("printCANMessages enabled.");
      }
      if (serialCommand == "disablePrintCANMessages")
      {
        printCANMessages = false;
        Serial.println("printCANMessages disabled.");
      }
      if (serialCommand == "enablePrintCANMessagesForPCAnalysis")
      {
        printMessagesForPCAnalysis = true;
        Serial.println("printMessagesForPCAnalysis enabled.");
      }
      if (serialCommand == "disablePrintCANMessagesForPCAnalysis")
      {
        printMessagesForPCAnalysis = false;
        Serial.println("printMessagesForPCAnalysis disabled.");
      }
}

void startHC05ATMode()
{
  usingHC05inATCommandMode = true;
  digitalWrite(PIN_HC05_EN, HIGH);
  serialHC05.end();
  serialHC05.begin(38400);  
  Serial.println("HC05 in AT Mode.");
}

void endHC05ATMode()
{	
  usingHC05inATCommandMode = false;
  digitalWrite(PIN_HC05_EN, LOW);
  serialHC05.end();
  serialHC05.begin(9600);  
  Serial.println("HC05 in normal mode.");
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

// format: ##START##0x5c3%8%39%07%00%00%00%00%00%00%##END##
void printCanMessage(struct can_frame &canMsg_in)
{    
    if (printMessagesForPCAnalysis)
    {
      Serial.print("##START##");
      Serial.print(canMsg_in.can_id, HEX); // print ID
      Serial.print("%"); 
      Serial.print(canMsg_in.can_dlc, HEX); // print DLC
      Serial.print("%");

      Serial.print(canMsg_in.data[0], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[1], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[2], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[3], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[4], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[5], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[6], HEX);
      Serial.print("%");
      Serial.print(canMsg_in.data[7], HEX);                 
      
      Serial.print("##END##\n");     
    }
    else
    {
      Serial.print(canMsg_in.can_id, HEX); // print ID
      Serial.print(" "); 
      Serial.print(canMsg_in.can_dlc, HEX); // print DLC
      Serial.print(" ");
    
      for (int i = 0; i<canMsg_in.can_dlc; i++)  {  // print the data
        
        Serial.print(canMsg_in.data[i], HEX);
        Serial.print(" ");

      }
      Serial.println();      
    }    
}

void PressPlayPauseButton(int duration)
{
  Serial.println("Pressing button: PLAYPAUSE");
  digitalWrite(PIN_PLAYPAUSE, HIGH);
  delay(duration);
  digitalWrite(PIN_PLAYPAUSE, LOW);
}

void PressNextButton(int duration)
{
  Serial.println("Pressing button: NEXT");
  digitalWrite(PIN_NEXT, HIGH);
  delay(duration);
  digitalWrite(PIN_NEXT, LOW);
}

void PressPreviousButton(int duration)
{
  Serial.println("Pressing button: PREVIOUS");
  digitalWrite(PIN_PREVIOUS, HIGH);
  delay(duration);
  digitalWrite(PIN_PREVIOUS, LOW);
}

void handleData(struct can_frame canMsg)
{      
  if (canMsg.can_id == ID_FIS_LINE1_RADIO)
  {
    FISSentLine1Recently = true;
    lastTimeLine1WasSent = millis();
  }
  if (canMsg.can_id == ID_FIS_LINE2_RADIO)
  {
    FISSentLine2Recently = true;
    lastTimeLine2WasSent = millis();
  }
  
  if (canMsg.can_id == ID_MFL) // MFL
  {
      uint8_t mfl_data[2];
      mfl_data[0] = canMsg.data[0];
      mfl_data[1] = canMsg.data[1];
      handleMFLAction(mfl_data);
  }

  if (canMsg.can_id == ID_TUERSENSOREN) // Türsensoren
  {
    //handleDoorSensorAction(canMsg.data[1]);
  }

  if (canMsg.can_id == ID_BATTERIESPANNUNG) // Batteriespannung
  {
      mVolt = 5 + (0.05 * canMsg.data[0]);
      //Serial.print("Batteriespannung: ");
      //Serial.print(volt);
      //Serial.print("V\n");
  }

  if (canMsg.can_id == ID_ZUENDUNGSSTATUS) // Zündungsstatus 0x2c3
  {
    handleIgnitionAction(canMsg.data[0]);
  }

  if (canMsg.can_id == ID_GESCHWINDIGKEIT) // Geschwindigkeit 0x351
  {
    byte d0 = canMsg.data[0]; 
    byte d1 = canMsg.data[1]; 
    byte d2 = canMsg.data[2];
    
    if ( d0 = 0x00 ) {mDirection = 0;}
    if ( d0 = 0x02 ) {mDirection = 1;}
    
    byte in[2] = {d2, d1};
    
    mSpeed = ((d2 << 8)+ d1-1)/190;
  }

  if (canMsg.can_id == ID_AUDIOSOURCE) // Modus 0x661
  {    
    //printCanMessage(canMsg);    
    // bit 6 of data[0] is set if radio is in CD/MP3 mode   
    //Serial.println(determineIfBitIsSet(canMsg.data[0], 6));
    
    if (determineIfBitIsSet(canMsg.data[0], 6)) // CD/MP3
    {
      if (mRadioMode != 1){mRadioMode = 1;}
    }
    else // FM/AM
    {
      if (mRadioMode != 3){mRadioMode = 3;}
    }   
  }
}

void handleMFLAction(uint8_t data[2])
{
  bool wasempty = false;
  switch (data[0]) // Aktuelles Submenu
  {
    case 0x39: // Radio/AUX      
      switch (data[1])
      {
        case 0x00: wasempty = true; break; // 39 00 --> empty
        case 0x01: Serial.println("Mode button double press"); break;
        case 0x02: Serial.println("Prev"); break;
        case 0x03: Serial.println("Next"); break;
        case 0xB: Serial.println("ScanUp"); PressNextButton(100); break;
        case 0xC: Serial.println("ScanDown"); PressPreviousButton(100); break;
        case 0x08: Serial.println("ScanPush"); PressPlayPauseButton(100); break;
        case 0x06: Serial.println("Vol+"); break;
        case 0x07: Serial.println("Vol-"); break;
      }
      if (!wasempty)
      {
        Serial.print("39 ");
        Serial.println(data[1], HEX);
      }
      break;
    case 0x3C: // Phone
      switch (data[1])
      {
        case 0x2A: Serial.println("Phone button pressed"); break;
        case 0x00: Serial.println("PHONE: active"); break;
        case 0x07: Serial.println("PHONE: Vol-"); break;
        case 0x06: Serial.println("PHONE: Vol+"); break;
      }      
      break;
    case 0x3B:
      Serial.println("Something volume button");
      //Serial.println("ZV schliessen...");
      //SendVolumeUp();
      //SendZentralverriegelungSchliessen();
      break;
  }
}

void handleIgnitionAction(uint8_t data)
{ 
  switch (data)
  {
    case 0x10: if (mIgnitionStatus != 1){mIgnitionStatus = 1;} break; // Kein Schlüssel
    case 0x11: if (mIgnitionStatus != 2){mIgnitionStatus = 2;} break; // Schlüssel steckt
    case 0x01: if (mIgnitionStatus != 3){mIgnitionStatus = 3;} break; // Lenkradschloss entriegelt
    case 0x05: if (mIgnitionStatus != 4){mIgnitionStatus = 4;} break; // Display AN
    case 0x07: if (mIgnitionStatus != 5){mIgnitionStatus = 5;} break; // Zuendung EIN
    case 0x0B: if (mIgnitionStatus != 6){mIgnitionStatus = 6;} break; // Zuendung EIN und ANLASSER
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

void SendZentralverriegelungSchliessen()
{
  canMsg_out.can_id = 0x291;
  canMsg_out.can_dlc = 5;
  canMsg_out.data[0] = 0x49;
  canMsg_out.data[1] = 0xAA;
  canMsg_out.data[2] = 0x02;
  canMsg_out.data[3] = 0x00;
  canMsg_out.data[4] = 0x00;
  canMsg_out.data[5] = 0x00;
  canMsg_out.data[6] = 0x00;
  canMsg_out.data[7] = 0x00;

  sendCanMessage(&canMsg_out);
}

void PrintFISLine1()
{
  // scroll FIS line 1 every *ScrollIntervalFISLine1*
  if ((unsigned long)(millis() - waitUntilFISLine1Scroll) >= ScrollIntervalFISLine1) 
  {    
     waitUntilFISLine1Scroll = millis(); 
     CalculateFISLine1();
  }
  SendFISLine1(actualFISLine1);
}

void PrintFISLine2()
{
  // scroll FIS line 2 every *ScrollIntervalFISLine2*
  if ((unsigned long)(millis() - waitUntilFISLine2Scroll) >= ScrollIntervalFISLine2) 
  {
     waitUntilFISLine2Scroll = millis(); 
     CalculateFISLine2();
  }
  SendFISLine1(actualFISLine1);
}

void CalculateFISLine1()
{
  int length = strlen(completeFISLine1);
        
        for (int i = 0; i <= 7; i++){
          actualFISLine1[i] = ' ';
        }
        
        if (length <=8) {
          for (int i = 0; i <= 7; i++){
          actualFISLine1[i] = completeFISLine1[i];
          }
        }
        
        if (length > 8) {
          if (FIS1Position <= length - 9) {
            for (int i = 0; i <= 7; i++){
              actualFISLine1[i] = completeFISLine1[i+FIS1Position];
            }
            FIS1Position = FIS1Position+1;
          } else {
            for (int i = 0; i <= 7; i++){
              actualFISLine1[i] = completeFISLine1[i+FIS1Position];
            }
            FIS1Position = 0;
          }
        }
}

void CalculateFISLine2()
{
  int length = strlen(completeFISLine2);
        
        for (int i = 0; i <= 7; i++){
          actualFISLine2[i] = ' ';
        }
        
        if (length <=8) {
          for (int i = 0; i <= 7; i++){
          actualFISLine2[i] = completeFISLine2[i];
          }
        }
        
        if (length > 8) {
          if (FIS2Position <= length - 9) {
            for (int i = 0; i <= 7; i++){
              actualFISLine2[i] = completeFISLine2[i+FIS2Position];
            }
            FIS2Position = FIS2Position+1;
          } else {
            for (int i = 0; i <= 7; i++){
              actualFISLine2[i] = completeFISLine2[i+FIS2Position];
            }
            FIS2Position = 0;
          }
        }
}

void SendFISLine1(char text[])
{
  int length = strlen(text);  
  canMsg_out.can_id = ID_FIS_LINE1_RADIO;  
  canMsg_out.can_dlc = 8;
  
  for (int i=0;i<=length-1;i++) {
    canMsg_out.data[i] = convertCharToByte(text[i]); 
  }            
  sendCanMessage(&canMsg_out);
}

void SendFISLine2(char text[])
{
  int length = strlen(text);  
  canMsg_out.can_id = ID_FIS_LINE2_RADIO;  
  canMsg_out.can_dlc = 8;
  
  for (int i=0;i<=length-1;i++) {
    canMsg_out.data[i] = convertCharToByte(text[i]); 
  }            
  sendCanMessage(&canMsg_out);
}

void SendVolumeUp()
{
  canMsg_out.can_id = ID_MFL;
  canMsg_out.can_dlc = 2;
  canMsg_out.data[0] = 0x39;
  canMsg_out.data[1] = 0x06;
  canMsg_out.data[2] = 0x00;
  canMsg_out.data[3] = 0x00;
  canMsg_out.data[4] = 0x00;
  canMsg_out.data[5] = 0x00;
  canMsg_out.data[6] = 0x00;
  canMsg_out.data[7] = 0x00;

  sendCanMessage(&canMsg_out);
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

byte convertCharToByte(char pChar) { 
  switch (pChar) {
    // ---------------------------------
    // -------- Kleinbuchstaben --------
    // ---------------------------------
    case 'a':
      return 0x01;
      break;
    case 'b':
      return 0x02;
      break;
    case 'c':
      return 0x03;
      break;
    case 'd':
      return 0x04;
      break;
    case 'e':
      return 0x05;
      break;
    case 'f':
      return 0x06;
      break;
    case 'g':
      return 0x07;
      break;
    case 'h':
      return 0x08;
      break;
    case 'i':
      return 0x09;
      break;
    case 'j':
      return 0x0A;
      break;
    case 'k':
      return 0x0B;
      break;
    case 'l':
      return 0x0C;
      break;
    case 'm':
      return 0x0D;
      break;
    case 'n':
      return 0x0E;
      break;
    case 'o':
      return 0x0F;
      break;
    case 'p':
      return 0x10;
      break;
    case 'q':
      return 0x71;
      break;
    case 'r':
      return 0x72;
      break;
    case 's':
      return 0x73;
      break;
    case 't':
      return 0x74;
      break;
    case 'u':
      return 0x75;
      break;
    case 'v':
      return 0x76;
      break;
    case 'w':
      return 0x77;
      break;
    case 'x':
      return 0x78;
      break;
    case 'y':
      return 0x79;
      break;
    case 'z':
      return 0x7A;
      break;
    // ---------------------------------
    // -------- Großbuchstaben ---------
    // ---------------------------------
    case 'A':
      return 0x41;
      break;
    case 'B':
      return 0x42;
      break;
    case 'C':
      return 0x43;
      break;
    case 'D':
      return 0x44;
      break;
    case 'E':
      return 0x45;
      break;
    case 'F':
      return 0x46;
      break;
    case 'G':
      return 0x47;
      break;
    case 'H':
      return 0x48;
      break;
    case 'I':
      return 0x49;
      break;
    case 'J':
      return 0x4A;
      break;
    case 'K':
      return 0x4B;
      break;
    case 'L':
      return 0x4C;
      break;
    case 'M':
      return 0x4D;
      break;
    case 'N':
      return 0x4E;
      break;
    case 'O':
      return 0x4F;
      break;
    case 'P':
      return 0x50;
      break;
    case 'Q':
      return 0x51;
      break;
    case 'R':
      return 0x52;
      break;
    case 'S':
      return 0x53;
      break;
    case 'T':
      return 0x54;
      break;
    case 'U':
      return 0x55;
      break;
    case 'V':
      return 0x56;
      break;
    case 'W':
      return 0x57;
      break;
    case 'X':
      return 0x58;
      break;
    case 'Y':
      return 0x59;
      break;
    case 'Z':
      return 0x5A;
      break;
  case 'Ö':
      return 0x60;
      break;
    // ---------------------------------
    // ------------ Zahlen -------------
    // ---------------------------------
    case 0:
      return 0x30;
      break;
    case 1:
      return 0x31;
      break;
    case 2:
      return 0x32;
      break;
    case 3:
      return 0x33;
      break;
    case 4:
      return 0x34;
      break;
    case 5:
      return 0x35;
      break;
    case 6:
      return 0x36;
      break;
    case 7:
      return 0x37;
      break;
    case 8:
      return 0x38;
      break;
    case 9:
      return 0x39;
      break;
    // ---------------------------------
    // -------- Sonderzeichen ----------
    // ---------------------------------
    case '_':
      return 0x12;
      break; 
    case '!':
      return 0x21;
      break; 
    case '"':
      return 0x22;
      break; 
    case '#':
      return 0x23;
      break; 
    case '$':
      return 0x24;
      break; 
    case '%':
      return 0x25;
      break; 
    case '&':
      return 0x26;
      break; 
    case '(':
      return 0x28;
      break; 
    case ')':
      return 0x29;
      break; 
    case '*':
      return 0x2A;
      break; 
    case '+':
      return 0x2B;
      break;
    case ',':
      return 0x2C;
      break;
    case '-':
      return 0x2D;
      break;
    case '.':
      return 0x2E;
      break;
    case '/':
      return 0x2F;
      break;
    case ':':
      return 0x3A;
      break;
    case ';':
      return 0x3B;
      break;
    case '<':
      return 0x3C;
      break;
    case '=':
      return 0x3D;
      break;
    case '>':
      return 0x3E;
      break;
    case '?':
      return 0x3F;
      break;
    case '§':
      return 0xCF;
      break;
    case ' ':
      return 0x20;
      break;
  }
}

bool determineIfBitIsSet(char hexValue, uint8_t bitPosition)
{
  // shifts all bits rights to the point where the LSB equals the desired bit
  // then we use logical AND which only sets a bit if both bits were set
  // since 0x01 has only the LSB set, we will know the status of the desired bit
  uint8_t result = ((hexValue >> (bitPosition-1))  & 0x01);
  if (result == 1) {return true;}
  else {return false;}
}
