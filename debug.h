/* This file will contain control commands useful for debug
 *     such as LED control and serial text message
 */


#ifndef Debug_h
#define Debug_h
 // Copied from example Blink

   #define SETUP_LED() pinMode(LED_BUILTIN, OUTPUT)
   #define LED_ON() digitalWrite(LED_BUILTIN, HIGH)
   #define LED_OFF() digitalWrite(LED_BUILTIN, LOW)   

#endif
