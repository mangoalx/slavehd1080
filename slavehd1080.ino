
/****************************************************************************************
 * MCP9808 simulator with Arduino feather 32u4
 *          by John Xu, Videri
 *    * To be done      
 *    + To be tested
 *    - Done
 *    
 * Version 0.1
 *    - Basic version
 *    - I2C slave device 
 *    - Command line allow setting values and changing I2C address
 *    - Support repeat-start reading (write 1 byte register No. then restart to read data), new wire library
 * Version 0.2
 *    - Analog input to adjust value
 *    - Check address pins to learn I2C slave address
 *    - Remove definition of slave address mask
 *    - Remove LEDON LEDOFF command
 * Version 0.21  
 *    - Use F() macro for constant strings used in print function
 *    - Use PROGMEM for constant string arrays to put them in flash instead of in RAM
 *    - If received setval then stop using analog input to set value until analog input changed enough.
 *    - Allow value with fraction
 *    - Compare and set bit flag for upper/lower limit and critical value (full mcp9808 function)
 * Next Version
 *    * output alert signal
 *    * update slave address dynamically, like a real sensor does
 ***************************************************************************************/

 

#include <Wire.h>
//#include <HardWire.h>
 
/*******************************************************************
 * MCP9808 Register map
 * 
 * addr   len   name
 * 0      2     RFU
 * 1      2     Config
 * 2      2     Tupper
 * 3      2     Tlower
 * 4      2     Tcrit
 * 5      2     Ta
 * 6      2     ManufacturerID
 * 7      2     DeviceID/revision
 * 8      1     Resolution
 */
/********************************************************************/
// If we need to use interrupt then un-comment these
//#define  UseInterrupt
//#define  INTERRUPT_PIN           2      //I don't need this pin 

#define DEFAULT_ADDRESS 0x18      //MCP9808 default address (A2, A1, A0 pulled down)
#define ADDRESS_A0 6
#define ADDRESS_A1 9
#define ADDRESS_A2 10

#define ANALOGINPIN A0

//#define ALERT 5

//#define  SLAVE_ADDRESS           0x1C  //slave address,any number from 0x01 to 0x7F
//#define  SLAVE_MASK              0x4   //This value is put in TWAMR, the bit will be ignored, so 0x1E will also
                                        //be accepted as slave address (shifted left for 1 so it is 4 instead of 2)
//twi_slaaddr = TWDR >> 1;
#define  REG_MAP_SIZE            18     //9 registers of 2 bytes data, last one Resolution is single byte, check later

#define  MAX_SENT_BYTES          3      //3 bytes can be written in - reg address, 2 bytes of data maximum

#define  MANUFACTUREID           0x0054

#define DEVICEID                0X0400

#define RFU                     0x001F

const byte defaultReg[REG_MAP_SIZE] = {0x0,0x1F,      //RFU
                                       0x0,0x0,       //config
                                       0x0,0x0,       //Tupper
                                       0x0,0x0,      //Tlower
                                       0x0,0x0,      //Tcrit
                                       0x0,0x0,      //Ta
                                       0x0,0x54,      //ManufactureID
                                       0x04,0x0,      //DeviceID
                                       0x03,0x0     //Resolution, 1 byte, fill 0 the last byte
};

const byte TaRegister=5;             //no.5 register is Temperature ambient, will be used as default pointer value                                       
const byte TupperReg=2;
const byte TlowerReg=3;
const byte TcritReg=4;
                                       
#define OutTcritFlag 0x80
#define OutTupperFlag 0x40
#define OutTlowerFlag 0x20
/********* Global  Variables  ***********/

byte registerMap[REG_MAP_SIZE];

byte registerMapTemp[REG_MAP_SIZE];

byte receivedCommands[MAX_SENT_BYTES];

byte regPointer=TaRegister;           //write only, to specify the register to access,default to Ta register
byte dataReceived=0;                  //how many byte data received 
byte mapPointer=2*regPointer;         //index for registerMap
byte slaveAddress=0x1c;

int sensorValue=0;                    //Sensor read value
int mappedValue=0;                    //Map sensor value to temperature value, 0-1023 to -100 ~ 250

int sensorValueOld=0;
bool autoUpdateVal=true;
#define MINSENSORDIFF 20              //If user has set the value with setval command, the autoUpdate of value will be disabled, until you adjust the potentiometer big enough

#include "commandline.h"

void setup()

{
  Serial.begin(115200);

//  while (!Serial);
// Should not wait for serial, it will stuck here until we setup a serial connection

#ifdef UseInterrupt

  {

    pinMode(INTERRUPT_PIN,OUTPUT);

    digitalWrite(INTERRUPT_PIN,HIGH);

  }
#endif
  for(byte i=0;i<REG_MAP_SIZE;i++)         //copy deault data into registers
    registerMap[i]=defaultReg[i];

//Check address pins to decide which address should be taken
  pinMode(ADDRESS_A2, INPUT_PULLUP);       //For address control A2,A1,A0, pull-down on the TSB, so keep them high-impedance
  pinMode(ADDRESS_A1, INPUT_PULLUP);
  pinMode(ADDRESS_A0, INPUT_PULLUP);

  slaveAddress=DEFAULT_ADDRESS;             //Base address is 0x18
  if(digitalRead(ADDRESS_A2)) slaveAddress+=4;
  if(digitalRead(ADDRESS_A1)) slaveAddress+=2;
  if(digitalRead(ADDRESS_A0)) slaveAddress+=1;

  Serial.print(F("Using I2C slave address:0x"));
  Serial.println(slaveAddress,HEX);

  Wire.begin(slaveAddress); 
#ifdef SLAVE_MASK
  TWAMR = SLAVE_MASK;
#endif                                      //if SLAVE_MASK is defined, it should be written to TWAMR

  Wire.onRequest(requestEvent);

  Wire.onReceive(receiveEvent);

//  Wire.onRequestData(requestData);          // New from hardwire library, when was read by master, return a byte
  

}

 

void loop()

{
  processDataReceved();               //Check if we received data to be written to registers
  if(getCommandLineFromSerialPort(CommandLine))      //global CommandLine is defined in CommandLine.h
      DoMyCommand(CommandLine);
  sensorValue = analogRead(ANALOGINPIN);

//When user changed the value by sending command setval, the autoUpdateVal will be disabled. When the sensorValue changed big enough, re-enable autoUpdateVal
  if(autoUpdateVal || (abs(sensorValue-sensorValueOld) > MINSENSORDIFF)){
    sensorValueOld=sensorValue;     //Update sensorValueOld
    autoUpdateVal=true;             //Enable autoUpdateVal
#ifdef VALUEFRACTION
    mappedValue = map(sensorValue,0,1023,-50*16,200*16);            //To support fraction, data transferred to updateValue should be multiplied by 16
#else
    mappedValue = map(sensorValue,0,1023,-50,200);
#endif
    updateValue(mappedValue,TaRegister);
  }
//  Serial.println(sensorValue);
//debug  Serial.println(mappedValue);
  delay(500);                       //delay 100ms

}
//This handler needs to be short,there is just several uS to get ready for sending data. Maybe it holds the scl if not ready
void requestEvent()

{

  Wire.write(registerMap+regPointer*2, 2);   //Send 2 bytes data only

}

 
//This handler for slave was read by master, and return a byte data to be send
//byte requestData(void)
//{
//  return registerMap[mapPointer++];
//}


//This handler needs to be short, cause it hold the i2c until quit
void receiveEvent(int bytesReceived)

{
  //LED_ON();
  
  //if only 1 byte is received, it is register address, put it into register pointer
  if(bytesReceived==1) regPointer=Wire.read();

  else{
    for (dataReceived = 0; dataReceived < bytesReceived && dataReceived<MAX_SENT_BYTES; dataReceived++)
  
        receivedCommands[dataReceived] = Wire.read();
  
  // if we receive more data than allowed just throw it away, I just leave them un-read here, should not need to read the rest
  
    if(dataReceived == 1 && (receivedCommands[0] < REG_MAP_SIZE/2)) 
  // if only 1 byte received, this is the reg address, and a read operation is coming
    {
      regPointer = receivedCommands[0];         //if the reg address is valid, then set the pointer
      dataReceived = 0;                         //clear dataReceived, no more process is needed
    }
  }
 
}

void processDataReceved(void)
{
  if(dataReceived != 0){
    int i = receivedCommands[0];
    if (i>0 && i<5 && dataReceived == 3) {                         //Register 1 to 4 
      registerMap[i*2]=receivedCommands[1];
      registerMap[i*2+1]=receivedCommands[2];
    }
    else if (i==8 && dataReceived == 2) {
      registerMap[i*2]=receivedCommands[1];                       //Register 8 resolution, 1 byte
    }
    dataReceived=0;                                               //Clear dataReceived
  }
}

