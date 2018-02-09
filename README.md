This is project to simulate a hdc1080 sensor using an arduino feather 32u4 board, so we can easily get whatever temperature or humidity data we want, for test purpose.

Basically, it is an I2C slave device, its I2C address can be set by sending setaddr command via serial port. The default address is 0x40

A simple command line interface is created for user to control the device via serial communication. The source code is in commandline.h. Use help to see a list of available commands.

The registers of temperature and humidity can be set with setval and sethum commands.

Help texts for the commands are put in progmem to save dynamic memory, you can disable it by commenting out pre-definition of HELPTEXTINPROGMEM, so to be easier to add/remove commands

Please note that setval/sethum command only takes integar value.




