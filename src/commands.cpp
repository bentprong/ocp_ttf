//===================================================================
// commands.cpp
//
// Functions specific to an OCP project - these are the command
// functions called by the CLI.  Also contains pin definitions that
// are used by the pins, read and write commands. See also main.hpp
// for the actual Arduino pin numbering that aligns with variants.cpp
// README.md in platformio folder of repo has details about this file.
//===================================================================
#include "main.hpp"
#include "eeprom.hpp"
#include <math.h>

extern char             *tokens[];
extern EEPROM_data_t    EEPROMData;

// pin defs used for 1) pin init and 2) copied into volatile status structure
// to maintain state of inputs pins that get written 3) pin names (nice, right?) ;-)
// NOTE: Any I/O that is connected to the DIP switches HAS to be an input because those
// switches can be strapped to ground.  Thus, if the pin was an output and a 1 was
// written, there would be a dead short on that pin (no resistors).
// NOTE: The order of the entries in this table is the order they are displayed by the
// 'pins' command. There is no other signficance to the order.
 const pin_mgt_t     staticPins[] = {
  {               TEMP_WARN, OUTPUT,   "TEMP_WARN"},
  {               TEMP_CRIT, OUTPUT,   "TEMP_CRIT"},
  {              FAN_ON_AUX, OUTPUT,   "FAN_ON_AUX"},
  {           OCP_SCAN_LD_N, OUTPUT,   "SCAN_LD_N"},
  {         OCP_MAIN_PWR_EN, OUTPUT,  "MAIN_EN"},
  {          OCP_AUX_PWR_EN, OUTPUT,  "AUX_EN"},
  {        OCP_SCAN_DATA_IN, OUTPUT,  "SCAN_DATA_IN"},  // "in" to NIC 3.0 card
  {       OCP_SCAN_DATA_OUT, INPUT,   "SCAN_DATA_OUT"}, // "out" from NIC 3.0 card
  {              P1_LINKA_N, INPUT,   "P1_LINKA_N"},
  {            P1_LED_ACT_N, INPUT,   "P1_LED_ACT_N"},
  {              LINK_ACT_2, INPUT,   "LINK_ACT_2"},
  {              P3_LINKA_N, INPUT,   "P3_LINKA_N"},
  {            P3_LED_ACT_N, INPUT,   "P3_LED_ACT_N"},
  {           OCP_PRSNTB0_N, INPUT,   "PRSNTB0_N"},
  {           OCP_PRSNTB2_N, INPUT,   "PRSNTB2_N"},
  {           OCP_PRSNTB1_N, INPUT,   "PRSNTB1_N"},
  {           OCP_PRSNTB3_N, INPUT,   "PRSNTB3_N"},
  {              SCAN_VER_0, INPUT,   "SCAN_VER_0"},
  {              SCAN_VER_1, INPUT,   "SCAN_VER_1"},
  {              OCP_WAKE_N, INPUT,   "WAKE_N"},
  {            OCP_PWRBRK_N, INPUT,   "PWRBRK_N"},
};

uint16_t      static_pin_count = sizeof(staticPins) / sizeof(pin_mgt_t);

static char             outBfr[OUTBFR_SIZE];
uint8_t                 pinStates[PINS_COUNT] = {0};

void writePin(uint8_t pinNo, uint8_t value);
void readAllPins(void);

// --------------------------------------------
// configureIOPins()
// --------------------------------------------
void configureIOPins(void)
{
  pin_size_t        pinNo;
  uint8_t           pinFunc;

  for ( int i = 0; i < static_pin_count; i++ )
  {
      // output only pins are always defined as outputs
      // in-out pins are defined as inputs except when
      // being written to
      if ( staticPins[i].pinFunc == OUTPUT )
        pinFunc = OUTPUT;
      else
        pinFunc = INPUT;

      pinNo = staticPins[i].pinNo;
      pinMode(pinNo, pinFunc);

      // increase drive strength on output pins
      // NOTE: IN_OUT_PINs are defined as outputs
      if ( staticPins[i].pinFunc == OUTPUT )
      {
          // see ttf/variants.cpp for the data in g_APinDescription[]
          // this will source 7mA, sink 10mA
          PORT->Group[g_APinDescription[pinNo].ulPort].PINCFG[g_APinDescription[pinNo].ulPin].bit.DRVSTR = 1;

          // NOTE: This assumes that all outputs are active high!
          writePin(pinNo, 0);
      }
  }
}

//===================================================================
//                    READ, WRITE COMMANDS
//===================================================================

bool readPin(uint8_t pinNo)
{
    uint8_t         index = getPinIndex(pinNo);

    if ( staticPins[index].pinFunc == OUTPUT )
    {
        pinStates[index] = (*portOutputRegister(digitalPinToPort(pinNo)) & digitalPinToBitMask(pinNo)) == 0 ? 0 : 1;
    }
    else
    {
        pinStates[index] = digitalRead((pin_size_t) pinNo);
    }

    return(pinStates[pinNo]);
}

void writePin(uint8_t pinNo, uint8_t value)
{
    digitalWrite(pinNo, value);
    pinStates[pinNo] = (bool) value;
}

// --------------------------------------------
// readCmd() - read pin
// --------------------------------------------
int readCmd(int arg)
{
    uint8_t       pinNo = atoi(tokens[1]);

    if ( pinNo > PINS_COUNT )
    {
        terminalOut((char *) "Invalid pin number; please use Arduino numbering");
        return(1);
    }

    (void) readPin(pinNo);
    sprintf(outBfr, "Pin %d (%s) = %d", pinNo, getPinName(pinNo), pinStates[pinNo]);
    terminalOut(outBfr);
    return(0);
}

// --------------------------------------------
// writeCmd() - write a pin with 0 or 1
// --------------------------------------------
int writeCmd(int arg)
{
    uint8_t     pinNo = atoi(tokens[1]);
    uint8_t     value = atoi(tokens[2]);
    uint8_t     index = getPinIndex(pinNo);

    if ( pinNo > PINS_COUNT || index == -1 )
    {
        terminalOut((char *) "Invalid pin number; use 'pins' command for help.");
        return(1);
    }    

    if ( staticPins[index].pinFunc == INPUT )
    {
        terminalOut((char *) "Cannot write to an input pin! Use 'pins' command for help.");
        return(1);
    }  

    if ( value != 0 && value != 1 )
    {
        terminalOut((char *) "Invalid pin value; please enter either 0 or 1");
        return(1);
    }

    writePin(pinNo, value);

    sprintf(outBfr, "Wrote %d to pin # %d (%s)", value, pinNo, getPinName(pinNo));
    terminalOut(outBfr);
    return(0);
}

char getPinChar(int index)
{
    if ( staticPins[index].pinFunc == INPUT )
        return('I');
    else if ( staticPins[index].pinFunc == OUTPUT )
        return('O');
    else
        return('B');
}

// --------------------------------------------
// pinCmd() - dump all I/O pins on terminal
// --------------------------------------------
int pinCmd(int arg)
{
    int         count = static_pin_count;
    int         index = 0;
    uint8_t     pinNo;

    terminalOut((char *) " #           Pin Name   I/O              #        Pin Name      I/O ");
    terminalOut((char *) "-------------------------------------------------------------------- ");

	readAllPins();
	
    while ( count > 0 )
    {
      if ( count == 1 )
      {
          sprintf(outBfr, "%2d %20s %c %d ", staticPins[index].pinNo, staticPins[index].name,
                  getPinChar(index), pinStates[staticPins[index].pinNo]);
          terminalOut(outBfr);
          break;
      }
      else
      {
          pinNo = staticPins[index].pinNo;
          sprintf(outBfr, "%2d %20s %c %d\t\t%2d %20s %c %d ", 
                  pinNo, staticPins[index].name, getPinChar(index), pinStates[pinNo],
                  staticPins[index+1].pinNo, staticPins[index+1].name, getPinChar(index+1),
                  pinStates[staticPins[index+1].pinNo]);
          terminalOut(outBfr);
          count -= 2;
          index += 2;
      }
    }

    return(0);
}

//===================================================================
//                         Status Display Screen
//===================================================================

// --------------------------------------------
// padBuffer() - pad a buffer with spaces
// --------------------------------------------
 char *padBuffer(int pos)
 {
    int         leftLen = strlen(outBfr);
    int         padLen = pos - leftLen;
    char        *s;

    s = &outBfr[leftLen];
    memset((void *) s, ' ', padLen);
    s += padLen;
    return(s);
 }

// --------------------------------------------
// readAllPins() 
// --------------------------------------------
void readAllPins(void)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        (void) readPin(staticPins[i].pinNo);
    }
}

// --------------------------------------------
// statusCmd() - status display (real-time)
// can be stopped by pressing any key
// --------------------------------------------
int statusCmd(int arg)
{
    uint16_t        count = EEPROMData.status_delay_secs;
    bool            oneShot = (count == 0) ? true : false;

    while ( 1 )
    {
        readAllPins();

        CLR_SCREEN();
        CURSOR(1, 29);
        displayLine((char *) "TTF Status Display");

        CURSOR(3,1);
        sprintf(outBfr, "TEMP WARN         %d", pinStates[TEMP_WARN]);
        displayLine(outBfr);

        CURSOR(3,57);
        sprintf(outBfr, "P1_LINK_A_N      %u", pinStates[P1_LINKA_N]);
        displayLine(outBfr);

        CURSOR(4,1);
        sprintf(outBfr, "TEMP CRIT         %u", pinStates[TEMP_CRIT]);
        displayLine(outBfr);

        CURSOR(4,56);
        sprintf(outBfr, "PRSNTB [3:0]   %u%u%u%u", pinStates[OCP_PRSNTB3_N], pinStates[OCP_PRSNTB2_N], pinStates[OCP_PRSNTB1_N], pinStates[OCP_PRSNTB0_N]);
        displayLine(outBfr);

        CURSOR(5,1);
        sprintf(outBfr, "FAN ON AUX        %u", pinStates[FAN_ON_AUX]);
        displayLine(outBfr);

        CURSOR(5,58);
        sprintf(outBfr, "LINK_ACT_2      %u", pinStates[LINK_ACT_2]);
        displayLine(outBfr);

        CURSOR(6,1);
        sprintf(outBfr, "SCAN_LD_N         %d", pinStates[OCP_SCAN_LD_N]);
        displayLine(outBfr);

        CURSOR(6,53);
        sprintf(outBfr, "SCAN VERS [1:0]     %u%u", pinStates[SCAN_VER_1], pinStates[SCAN_VER_0]);
        displayLine(outBfr);

        CURSOR(7,1);
        sprintf(outBfr, "AUX_EN            %d", pinStates[OCP_AUX_PWR_EN]);
        displayLine(outBfr);      

        CURSOR(7,60);
        sprintf(outBfr, "PWRBRK_N      %d", pinStates[OCP_PWRBRK_N]);
        displayLine(outBfr);

        CURSOR(8,1);
        sprintf(outBfr, "MAIN_EN           %d", pinStates[OCP_MAIN_PWR_EN]);
        displayLine(outBfr);  

        CURSOR(8,62);
        sprintf(outBfr, "WAKE_N      %d", pinStates[OCP_WAKE_N]);
        displayLine(outBfr);

        CURSOR(9,1);
        sprintf(outBfr, "P3_LED_ACT_N      %d", pinStates[P3_LED_ACT_N]);
        displayLine(outBfr);  

        CURSOR(9,58);
        sprintf(outBfr, "P3_LINKA_N      %d", pinStates[P3_LINKA_N]);
        displayLine(outBfr);

        CURSOR(10,1);
        sprintf(outBfr, "P1_LED_ACT_N      %d", pinStates[P1_LED_ACT_N]);
        displayLine(outBfr);

        if ( oneShot )
        {
            CURSOR(12,1);
            displayLine((char *) "Status delay 0, set to nonzero for this screen to loop.");
            return(0);
        }

        CURSOR(24, 22);
        displayLine((char *) "Hit any key to exit this display");

        while ( count-- > 0 )
        {
            if ( SerialUSB.available() )
            {
                // flush any user input and exit
                (void) SerialUSB.read();

                while ( SerialUSB.available() )
                {
                    (void) SerialUSB.read();
                }

                CLR_SCREEN();
                return(0);
            }

            delay(1000);
        }

        count = EEPROMData.status_delay_secs;
    }

    return(0);

} // statusCmd()


// --------------------------------------------
// getPinName() - get name of pin from pin #
// Returns pointer to pin name, or 'unknown' 
// if pinNo not found.
// --------------------------------------------
const char *getPinName(int pinNo)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        if ( pinNo == staticPins[i].pinNo )
            return(staticPins[i].name);
    }

    return("Unknown");
}

// --------------------------------------------
// getPinIndex() - get index into staticPins[]
// based on Arduino pin #. Returns -1 if pin
// not found, else index.
// --------------------------------------------
int8_t getPinIndex(uint8_t pinNo)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        if ( staticPins[i].pinNo == pinNo )
            return(i);
    }

    return(-1);
}

void set_help(void)
{
    terminalOut((char *) "EEPROM Parameters are:");
    terminalOut((char *) "  sdelay <int> ........ status display delay in seconds");

    // TODO add more set command help here
}

//===================================================================
//                              SET Command
//
// set <parameter> <value>
// 
// Supported parameters: 
//===================================================================
int setCmd(int arg)
{
    char          *parameter = tokens[1];
    String        valueEntered = tokens[2];
    float         fValue;
    int           iValue;
    bool          isDirty = false;

    if ( arg == 0 )
    {
        set_help();
        return(0);
    }

    if ( strcmp(parameter, "sdelay") == 0 )
    {
        // set ADC gain error
        iValue = valueEntered.toInt();
        if (EEPROMData.status_delay_secs != iValue )
        {
          isDirty = true;
          EEPROMData.status_delay_secs = iValue;
        }
    }
    else
    {
        terminalOut("Invalid parameter name");
        set_help();
        return(1);
    }

    if ( isDirty )
        EEPROM_Save();

    return(0);

} // setCmd()
