//===================================================================
// main.cpp
//
// Contains setup() initialization and main program loop() for an
// OCP Project that uses SAMD21G18A.  This file and the 2
// functions contained in it are generic.  See cli.cpp and commands.cpp
// for project-specific code.
//===================================================================
#include <Arduino.h>
#include "main.hpp"
#include "commands.hpp"
#include "eeprom.hpp"
#include "cli.hpp"
#include <Wire.h>
#include "main.hpp"

// timers
void timers_Init(void);

// heartbeat LED blink delays in ms (approx)
#define FAST_BLINK_DELAY            200
#define SLOW_BLINK_DELAY            1000

uint8_t         boardID;            // or'd BOARD_ID_bits 2..1
uint8_t         boardIDReal;        // adjust to align with X06+

/**
  * @name   setup
  * @brief  system initialization
  * @param  None
  * @retval None
  */
void setup() 
{
  uint16_t    counter = 20;

  Wire.begin();

  // configure I/O pins and read all inputs
  // into pinStates[]
  // NOTE: Output pins will be 0 initially
  // then updated on any writePin()
  configureIOPins();
  readAllPins();

  // board ID: X06 = 6 because all 3 ID pins should be low (0)
  boardID = (readPin(BOARD_ID_2) << 2) | (readPin(BOARD_ID_1) << 1) | readPin(BOARD_ID_0);
  boardIDReal = X06_VALUE + boardID;

  // turn on heartbeat LED, which indicates that the
  // board is being initialized
  // NOTE: LED pin is configured with other pins
  digitalWrite(OCP_HEARTBEAT_LED, LOW);

  // disable main & aux power to NIC 3.0 card
  writePin(OCP_MAIN_PWR_EN, 0);
  writePin(OCP_AUX_PWR_EN, 0);

  // deassert PHY reset
  writePin(NCSI_RST_N, 1);

  // init simulated EEPROM
  EEPROM_InitLocal();

  // start serial over USB and wait for a connection
  // NOTE: Baud rate isn't applicable to USB...
  SerialUSB.begin(115200);
  while ( !SerialUSB )
  {
      delay(FAST_BLINK_DELAY);
  }

  // begin issue #16 fix ------------------------------------
  // set LED on, close the USB connection because TeraTerm
  // won't reset properly; wait, then re-open the USB 
  // connection. This is a Windows-  // only issue but 
  // works on Mac as well.
  SerialUSB.end();
  delay(2000);

  // reconnect but don't wait for it; any output created
  // next will get queued and output in order
  SerialUSB.begin(115200);
  // end issue #16 fix ------------------------------------

  timers_Init();

} // setup()

/**
  * @name   loop
  * @brief  main program loop
  * @param  None
  * @retval None
  * @note   blink heartbeat LED & handle incoming characters over SerialUSB connection
  */
void loop() 
{
  int             byteIn;
  static char     inBfr[MAX_LINE_SZ];
  static int      inCharCount = 0;
  static char     lastCmd[80] = "help";
  const char      bs[4] = {0x1b, '[', '1', 'D'};  // terminal: backspace seq
  static bool     LEDstate = false;
  static uint32_t time = millis();
  static bool     isFirstTime = true;

  // output greeting and prompt
  if ( isFirstTime )
  {
    isFirstTime = false;
    doHello();
    doPrompt();
  }

  // blink heartbeat LED
  if ( millis() - time >= SLOW_BLINK_DELAY )
  {
      time = millis();
      LEDstate = LEDstate ? 0 : 1;
      digitalWrite(OCP_HEARTBEAT_LED, LEDstate);
  }

  // process incoming serial over USB characters
  if ( SerialUSB.available() )
  {
      byteIn = SerialUSB.read();
      if ( byteIn == 0x0a )
      {
          // line feed - echo it
          SerialUSB.write(0x0a);
          SerialUSB.flush();
      }
      else if ( byteIn == 0x0d )
      {
          // carriage return - EOL 
          // save as the last cmd (for up arrow) and call CLI with
          // the completed line less CR/LF
          terminalOut((char *) " ");
          inBfr[inCharCount] = 0;
          inCharCount = 0;
          strcpy(lastCmd, inBfr);
          cli(inBfr);
          SerialUSB.flush();
      }
      else if ( byteIn == 0x1b )
      {
          // handle ANSI escape sequence - only UP arrow is supported
          if ( SerialUSB.available() )
          {
              byteIn = SerialUSB.read();
              if ( byteIn == '[' )
              {
                if ( SerialUSB.available() )
                {
                    byteIn = SerialUSB.read();
                    if ( byteIn == 'A' )
                    {
                        // up arrow: echo last command entered then execute in CLI
                        terminalOut(lastCmd);
                        SerialUSB.flush();
                        cli(lastCmd);
                        SerialUSB.flush();
                    }
                }
            }
        }
    }
    else if ( byteIn == 127 || byteIn == 8 )
    {
        // delete & backspace do the same thing which is erase last char entered
        // and backspace once
        if ( inCharCount )
        {
            inBfr[inCharCount--] = 0;
            SerialUSB.write(bs, 4);
            SerialUSB.write(' ');
            SerialUSB.write(bs, 4);
            SerialUSB.flush();
        }
    }
    else
    {
        // all other keys get echoed & stored in buffer
        SerialUSB.write((char) byteIn);
        SerialUSB.flush();
        inBfr[inCharCount] = byteIn;
        if ( inCharCount < (MAX_LINE_SZ-1) )
        {
            inCharCount++;
        }
        else
        {
            terminalOut((char *) "Serial input buffer overflow!");
            inCharCount = 0;
        }
    }
  }

} // loop()

/**
  * @name   
  * @brief  
  * @param  None
  * @retval None
  */