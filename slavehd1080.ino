
/****************************************************************************************
 * HDC1080 simulator with Arduino feather 32u4
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
 ***************************************************************************************/

 

#include <Wire.h>
//#include <HardWire.h>
 
/*******************************************************************
 * HDC1080 Register map
 * 
 * Pointer    Name            Reset value       Description
 *  0x00      Temperature     0x0000            Temperature measurement output
 *  0x01      Humidity        0x0000            Relative Humidity measurement output
 *  0x02      Configuration   0x1000            HDC1080 configuration and status
 *  0xFB      Serial ID       device dependent  First 2 bytes of the serial ID of the part
 *  0xFC      Serial ID       device dependent  Mid 2 bytes of the serial ID of the part
 *  0xFD      Serial ID       device dependent  Last byte bit of the serial ID of the part
 *  0xFE      Manufacturer ID 0x5449            ID of Texas Instruments
 *  0xFF      Device ID       0x1050            ID of the device
 */
/********************************************************************/
// If we need to use interrupt then un-comment these
//#define  UseInterrupt
//#define  INTERRUPT_PIN           2      //I don't need this pin 

//#define UseAnalogInput          //Use potentiometer to get an analog input to adjust the sensor value
                                  //Not implemented yet, do not un-commment this option
                                  
#define ANALOGINPIN A0            //Analog input pin


#define DEFAULT_ADDRESS 0x40      //HDC1080 default address 

#define  REG_MAP_SIZE            16     //8 registers of 2 bytes data, last one Resolution is single byte, check later

#define  MAX_SENT_BYTES          3      //3 bytes can be written in - reg address, 2 bytes of data maximum

#define  MANUFACTUREID           0x5449

#define DEVICEID                0X1050


const byte defaultReg[REG_MAP_SIZE] = {0x0,0x0,       //Temperature
                                       0x0,0x0,       //Humidity
                                       0x0,0x10,      //Configuration
                                       0x0,0x0,       //SerialID1
                                       0x0,0x0,       //SerialID2
                                       0x0,0x0,       //SerialID3
                                       0x49,0x54,     //ManufactureID
                                       0x50,0x10,     //DeviceID
};

const byte specialRegister=0xF8;      //register >= 0xF8 will be mapped as register-=0xF8                                    
                                       
/********* Global  Variables  ***********/

byte registerMap[REG_MAP_SIZE];

//byte registerMapTemp[REG_MAP_SIZE];

byte receivedCommands[MAX_SENT_BYTES];

byte regPointer=0x0;           //write only, to specify the register to access,default to Temperature register
byte dataReceived=0;                  //how many byte data received 
byte mapPointer=2*regPointer;         //index for registerMap
byte slaveAddress=0x40;

#ifdef UseAnalogInput
int sensorValue=0;                    //Sensor read value
int mappedValue=0;                    //Map sensor value to temperature value, 0-1023 to -100 ~ 250

int sensorValueOld=0;
bool autoUpdateVal=true;
#define MINSENSORDIFF 20              //If user has set the value with setval command, the autoUpdate of value will be disabled, until you adjust the potentiometer big enough
#endif

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


  slaveAddress=DEFAULT_ADDRESS;             //0x40

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
#ifdef UseAnalogInput                 //If using analog input, then update sensor value with analog input
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
#else
  delay(50);
#endif
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
  if(bytesReceived==1){
    regPointer=Wire.read();
    if(regPointer >= specialRegister) regPointer -= specialRegister;    //map 0xFA ~ 0xFF to 0x3 ~ 0x7 by subtracting 0xF8
  }

  else{
    for (dataReceived = 0; dataReceived < bytesReceived && dataReceived<MAX_SENT_BYTES; dataReceived++)
  
        receivedCommands[dataReceived] = Wire.read();
  
  // if we receive more data than allowed just throw it away, I just leave them un-read here, should not need to read the rest
/*  don't have to check if only 1 byte is received, it has been checked before
    if(dataReceived == 1 && (receivedCommands[0] < REG_MAP_SIZE/2)) 
  // if only 1 byte received, this is the reg address, and a read operation is coming
    {
      regPointer = receivedCommands[0];         //if the reg address is valid, then set the pointer
      dataReceived = 0;                         //clear dataReceived, no more process is needed
    }*/
  }
 
}

void processDataReceved(void)
{
  if(dataReceived != 0){
    int i = receivedCommands[0];
    if (i==2 && dataReceived == 3) {                         //Register 0x2 for config is the only reg could be written
      registerMap[i*2]=receivedCommands[1];
      registerMap[i*2+1]=receivedCommands[2];
    }
    dataReceived=0;                                               //Clear dataReceived
  }
}

