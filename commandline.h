//Name this tab: CommandLine.h

#include <string.h>
#include <stdlib.h>

#include "debug.h"

#define HELPTEXTINPROGMEM
// To put command help text strings to progmem, to save ram space

#define VALUEFRACTION
// With this defined, the temperature value generated from potentiometer can contain fraction part. Setval command still can only use integar

//this following macro is good for debugging, e.g.  +2("myVar= ", myVar);
#define print2(x,y) (Serial.print(x), Serial.println(y))
#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))    //This is used to decide the length of an array

#define CR '\r'
#define LF '\n'
#define BS '\b'
#define NULLCHAR '\0'
#define SPACE ' '

#define COMMAND_BUFFER_LENGTH        64                        //length of serial buffer for incoming commands
char   CommandLine[COMMAND_BUFFER_LENGTH + 1];                 //Read commands into this buffer from Serial.  +1 in length for a termination char
bool nullToken=true;                                           //Mark end of the commandline when calling readnum, readhex,readword
const char *delimiters            = ", \n";                    //commands can be separated by return, space or comma

/****************************************************
   Add your commands prototype here
***************************************************/
void helpCommand(void);
void ledCommand(void);
void setaddrCommand(void);
void setbinCommand(void);
void setvalCommand(void);
void sethumCommand(void);
void nullCommand(void);
void listCommand(void);
void getvalCommand(void);
void gethumCommand(void);

/***************************************************
 *  Other functions
 **************************************************/
void updateValue(int val);
void updateHumid(int val);
//bool compareValue(byte reg,byte TaUpperByte,byte TaLowerByte);              //Compare Ta value against this register value, return true if exceeded
/*************************************************************************************************************
     your Command Names Here
*/
void (*g_pFunctionCmd[])(void)  =  {
  helpCommand,
  ledCommand,
  setbinCommand,
  setvalCommand,
  sethumCommand,
  setaddrCommand,
  listCommand,
  getvalCommand,
  gethumCommand,

  nullCommand                     //Keep this extra nullCommand always the last, to process unknown command
                                  //Do not put this command in the Commandlist, it will only be used when unknown command is received
};

const char *Commandlist[] = {
  "help",
  "led",

  "setbin",                       //set register value in binary, can access full register map
  "setval",                       //set temperature register value
  "sethum",                       //set humidity value                                
  "setaddr",                      //set i2c slave address, is it ok to re-begin with a new address?
  "list",
  "getval",                       //get temperature value
  "gethum"                        //get humidity value
};

#ifdef HELPTEXTINPROGMEM
// Command help text strings, use PROGMEM to keep them in flash instead of in ram.
const char helpText[] PROGMEM = {"help [command] - to see the help about using command \r\n command list:"};
const char ledText[] PROGMEM = {"led <0/1> - to turn off/on LED"};
const char setbinText[] PROGMEM = {"setbin <regAddr> byte0 byte1 ... - fill the registers with data byte0, byte1 ... starting from regAddr, data in HEX format"};
const char setvalText[] PROGMEM = {"setval <decVal> - set the temperature register with the decimal value"};
const char sethumText[] PROGMEM = {"sethum <decVal> - set the humidity register with the decimal value"};
const char setaddrText[] PROGMEM = {"setaddr <addr> - set the slave device's address with addr (in HEX format)"};
const char listText[] PROGMEM = {"list - list the registers content in HEX format"};
const char getvalText[] PROGMEM = {"getval - read the temperature register in decimal format"};
const char gethumText[] PROGMEM = {"gethum - read the humidity register in decimal format"};
const char nullText[] PROGMEM = {"unknown command - use help without a parameter to see the valid command list"};

void serialProgmemPrint(char *textp){
  char myChar,k;
  for (k = 0; k < strlen_P(textp); k++)
  {
    myChar =  pgm_read_byte_near(textp + k);
    Serial.print(myChar);
  }
}
void serialProgmemPrintln(char *textp){
  serialProgmemPrint(textp);
  Serial.println();
}
const char* const CommandHelp[] = {
  helpText,
  ledText,
  setbinText,
  setvalText,
  sethumText,
  setaddrText,
  listText,
  getvalText,
  gethumText,
  nullText  
};
#else
const char* const CommandHelp[] = {
  "help [command] - to see the help about using command \r\nCommand list:",
//  "command list:",
  "led <0/1> - to turn off/on LED",
  "setbin <regAddr> byte0 byte1 ... - fill the registers with data byte0, byte1 ... starting from regAddr, data in HEX format",
  "setval <decVal> - set the temperature register with the decimal value",
  "sethum <decVal> - set the humidity register with the decimal value",
  "setaddr <addr> - set the slave device's address with addr (in HEX format)",
  "list - list the registers content in HEX format",
  "getval - read the temperature register in decimal format",
  "gethum - read the humidity register in decimal format",
  "unknown command - use help without a parameter to see the valid command list"
};
#endif

#ifdef VALUEFRACTION
//  #ifdef HELPTEXTINPROGMEM
  
//  #else
    // .00 .0625 .125 .1875 .25 .3125 .375 .4375 .50 .5625 .625 .6875 .75 .8125 .875 .9375
    const char* const Fraction4bit[] = {
      ".00",".06",".13",".19",".25",".31",".38",".44",".50",".56",".63",".69",".75",".81",".88",".94"
    };
//  #endif
#endif

const int CommandLength = ARRAY_LENGTH(Commandlist);
/*************************************************************************************************************
    getCommandLineFromSerialPort()
      Return the string of the next command. Commands are delimited by return"
      Handle BackSpace character
      Make all chars lowercase
*************************************************************************************************************/

bool
getCommandLineFromSerialPort(char * commandLine)
{
  static uint8_t charsRead = 0;                      //note: COMAND_BUFFER_LENGTH must be less than 255 chars long
  //read asynchronously until full command input
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case CR:      //likely have full command in buffer now, commands are terminated by CR and/or LS
      case LF:
        commandLine[charsRead] = NULLCHAR;       //null terminate our command char array
        if (charsRead > 0)  {
          charsRead = 0;                           //charsRead is static, so have to reset
          Serial.println(commandLine);
          return true;
        }
        break;
      case BS:                                    // handle backspace in input: put a space in last char
        if (charsRead > 0) {                        //and adjust commandLine and charsRead
          commandLine[--charsRead] = NULLCHAR;
          Serial << byte(BS) << byte(SPACE) << byte(BS);  //no idea how this works, found it on the Internet
        }
        break;
      default:
        // c = tolower(c);
        if (charsRead < COMMAND_BUFFER_LENGTH) {
          commandLine[charsRead++] = c;
        }
        commandLine[charsRead] = NULLCHAR;     //just in case
        break;
    }
  }
  return false;
}


/* ****************************
   readNumber: return a 16bit (for Arduino Uno) signed integer from the command line
   readWord: get a text word from the command line

*/
int
readNumber () {
  char * numTextPtr = strtok(NULL, delimiters);         //K&R string.h  pg. 250
  if(numTextPtr==NULL) nullToken=true;
  else nullToken=false;
  return atoi(numTextPtr);                              //K&R string.h  pg. 251
}
int
readHex () {
  char * numTextPtr = strtok(NULL, delimiters);         //K&R string.h  pg. 250

  if(numTextPtr==NULL) nullToken=true;
  else nullToken=false;
  
  return (int)strtol(numTextPtr,NULL,16);                              //K&R string.h  pg. 251
}

char * readWord() {
  char * numTextPtr = strtok(NULL, delimiters);               //K&R string.h  pg. 250
  if(numTextPtr==NULL) nullToken=true;
  else nullToken=false;
  
  return numTextPtr;
}

void
nullCommand(void) {
  Serial.println(F("Command not found... use help to see the command list"));      //
}

//This is used to print hex data with leading 0 if it is only 1 digit
void serialPrintHex(byte b){
  if(b<16)
    Serial.print("0");
  Serial.print(b,HEX);
}


/****************************************************
   DoMyCommand
*/
bool
DoMyCommand(char * commandLine) {
  //  print2("\nCommand: ", commandLine);
  int result,cmdNo;

  char * ptrToCommandName = strtok(commandLine, delimiters);
  //  print2("commandName= ", ptrToCommandName);

  for(cmdNo=0;cmdNo<CommandLength;cmdNo++){
    if (strcmp(ptrToCommandName, Commandlist[cmdNo]) == 0)                    //find out the command number
      break;
  } 

  (*g_pFunctionCmd[cmdNo])();                                                 //call the command function
}

/****************************************************
   Add your commands functions here
***************************************************/
void helpCommand(){
  int i=0;
  char *pcCmd=readWord();                                         //get the parameter after help command
  
  if(!nullToken)
    for(i=0;i<CommandLength;i++)
      if (strcmp(pcCmd, Commandlist[i]) == 0)                    //find out the command number corresponding the parameter,
        break;                                                   //if not found, use nullCommand number, if no parameter, use 0 (for help command)
#ifdef HELPTEXTINPROGMEM
  serialProgmemPrintln(CommandHelp[i]);
#else
  Serial.println(CommandHelp[i]);                                 //print help text 
#endif
  if(i==0)
    for(int i=0;i<CommandLength;i++)
      Serial.println(Commandlist[i]);                             //if not specified which command to help,list all commands
}

void ledCommand() {
  int firstOperand = readNumber();
  if (firstOperand == 0)
      LED_OFF();
  else LED_ON();
}

void setaddrCommand(){
  byte addr;
  addr = readHex();
//  ina219.setAddress((uint8_t)addr);
  Serial.print(F("Set to new address: 0x"));
  Serial.println(addr,HEX);
  Wire.begin(addr);                               // To change the slave address, re-run begin with new address
  
}

void setbinCommand(){
  byte regAddr = readHex()*2;
  byte binData = readHex();

  while((!nullToken)&&(regAddr<REG_MAP_SIZE)){
    registerMap[regAddr++]=binData;
    binData = readHex();
  }
}

/****************************************************
 * set temperature value 
 * Usage: setval value 
 * The value should be integar, and between -256 to 255
 * 
 ******************************************************/
void setvalCommand(){
  int Temperature;

  Temperature=readNumber();
  if((nullToken)||(Temperature>125)||(Temperature<-40)){
    Serial.println(F("Temperature out of range,valid range is -40 to 125"));
    return;
  }
  updateValue(Temperature);
  
}
void sethumCommand(){
  int humidity;

  humidity=readNumber();
  if((nullToken)||(humidity>100)||(humidity<0)){
    Serial.println(F("Humidity out of range,valid range is 0 to 100"));
    return;
  }
  updateHumid(humidity);
  
}

// list command to print out full register map in HEX
void listCommand(void){
  for(int i=0;i<REG_MAP_SIZE;i++){
    serialPrintHex(registerMap[i]);
    Serial.print(" ");
  }
  Serial.println("");
}

void getvalCommand(void){
  byte UpperByte,LowerByte;
  unsigned int temp;
  float Temperature=0;

   
    UpperByte=registerMap[0];
    LowerByte=registerMap[1];

    temp = UpperByte * 256 + LowerByte;
    Temperature = temp*165.0f/65532-40;

    Serial.print(F("The value is: "));
    Serial.println(Temperature);
    
}
void gethumCommand(void){
  byte UpperByte,LowerByte;
  unsigned int temp;
  int humidity=0;

   
    UpperByte=registerMap[2];
    LowerByte=registerMap[3];

    temp = UpperByte * 256 + LowerByte;
    humidity = temp*100.0f/65532;

    Serial.print(F("The value is: "));
    Serial.println(humidity);
    
}



void updateValue(int val){
  byte UpperByte,LowerByte;
  unsigned int temp;
//  float tempf;

  temp = (val + 40.0f)*65532/165.0f;
  temp += 2;                    //We will remove bit1 and bit0, so add 2, if bit1 is 1 it will be increased to bit2

  UpperByte= temp >> 8;
  LowerByte= temp & 0xFC;
/*
#ifdef VALUEFRACTION
// When value fraction is enabled, the data val was multipled by 16
  UpperByte = (val>>8)&0x1F;    // Now upper byte should be Val/256, and with 0x1F to keep sign flag
  LowerByte = val&0xFF;         // lower byte is the lower byte of val
#else
  UpperByte=0;
  if(val<0) {
    UpperByte = 0x10;
    val=val&0xFF;
  }
  UpperByte|=(val/16)&0x0F;
  LowerByte=(val*16)&0xF0;
#endif

  if(reg==TaRegister){              //If updating Ta, check the flags
    if(compareValue(TcritReg,UpperByte,LowerByte)) UpperByte|=OutTcritFlag;
    if(compareValue(TupperReg,UpperByte,LowerByte)) UpperByte|=OutTupperFlag;
    if(!compareValue(TlowerReg,UpperByte,LowerByte)) UpperByte|=OutTlowerFlag;      //Here should be lower than, anyway in compareValue() it returns false when Ta<=Tlower.
                                                                //So, when Ta=Tlower the flag will be set. Think nobody cares this
  }
*/  
  registerMap[0]=UpperByte;
  registerMap[1]=LowerByte;
  
}

/*
Temperature Register
The temperature register is a 16-bit result register in binary format (the 2 LSBs D1 and D0 are always 0). The
result of the acquisition is always a 14 bit value. The accuracy of the result is related to the selected conversion
time (refer to Electrical Characteristics (1) ). The temperature can be calculated from the output data with:

Temperature(C) = (TEMPERATURE[15:00]/2**16) * 165 - 40

Temperature Register Description (0x00)
Description
[15:02] Temperature measurement (read only)
[01:00] Reserved, always 0 (read only
*/

void updateHumid(int hum){
  byte UpperByte,LowerByte;
  unsigned int temp;
//  float tempf;

  temp = hum/100.0f*65532;
  temp += 2;                    //We will remove bit1 and bit0, so add 2, if bit1 is 1 it will be increased to bit2
  UpperByte= temp >> 8;
  LowerByte= temp & 0xFC;

  registerMap[2]=UpperByte;
  registerMap[3]=LowerByte;
  
}

/*
Humidity Register
The humidity register is a 16-bit result register in binary format (the 2 LSBs D1 and D0 are always 0). The
result of the acquisition is always a 14 bit value. The accuracy of the result is related to the selected conversion
time (refer to Electrical Characteristics (1) ). The temperature can be calculated from the output data with:

Relative Humidity(% RH) = (HUMIDITY[15:00]/2**16) * 165 - 40

Humidity Register Description (0x01)
Description
[15:02] relative humidity measurement (read only)
[01:00] Reserved, always 0 (read only
*/

