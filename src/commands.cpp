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
#include "commands.hpp"
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
  {               TEMP_WARN, OUTPUT,    ACT_HI, "TEMP_WARN"},
  {               TEMP_CRIT, OUTPUT,    ACT_HI, "TEMP_CRIT"},
  {              FAN_ON_AUX, OUTPUT,    ACT_HI, "FAN_ON_AUX"},
  {           OCP_SCAN_LD_N, OUTPUT,    ACT_LO, "SCAN_LD_N"},
  {         OCP_MAIN_PWR_EN, OUTPUT,    ACT_HI, "MAIN_EN"},
  {          OCP_AUX_PWR_EN, OUTPUT,    ACT_HI, "AUX_EN"},
  {        OCP_SCAN_DATA_IN, OUTPUT,    ACT_HI, "SCAN_DATA_IN"},  // "in" to NIC 3.0 card
  {       OCP_SCAN_DATA_OUT, INPUT,     ACT_HI, "SCAN_DATA_OUT"}, // "out" from NIC 3.0 card
  {              P1_LINKA_N, INPUT,     ACT_LO, "P1_LINKA_N"},
  {            P1_LED_ACT_N, INPUT,     ACT_LO, "P1_LED_ACT_N"},
  {              LINK_ACT_2, INPUT,     ACT_LO, "LINK_ACT_2"},
  {              P3_LINKA_N, INPUT,     ACT_LO, "P3_LINKA_N"},
  {            P3_LED_ACT_N, INPUT,     ACT_LO, "P3_LED_ACT_N"},
  {           OCP_PRSNTB0_N, INPUT,     ACT_LO, "PRSNTB0_N"},
  {           OCP_PRSNTB2_N, INPUT,     ACT_LO, "PRSNTB2_N"},
  {           OCP_PRSNTB1_N, INPUT,     ACT_LO, "PRSNTB1_N"},
  {           OCP_PRSNTB3_N, INPUT,     ACT_LO, "PRSNTB3_N"},
  {              SCAN_VER_0, INPUT,     ACT_HI, "SCAN_VER_0"},
  {              SCAN_VER_1, INPUT,     ACT_HI, "SCAN_VER_1"},
  {              OCP_WAKE_N, INPUT,     ACT_LO, "WAKE_N"},
  {            OCP_PWRBRK_N, INPUT,     ACT_LO, "PWRBRK_N"},
  {              NCSI_RST_N, OUTPUT,    ACT_LO, "NCSI_RST_N"},
};

uint16_t      static_pin_count = sizeof(staticPins) / sizeof(pin_mgt_t);

static char             outBfr[OUTBFR_SIZE];
uint8_t                 pinStates[PINS_COUNT] = {0};

void writePin(uint8_t pinNo, uint8_t value);
void readAllPins(void);

/**
  * @name   configureIOPins
  * @brief  configure all I/O pins
  * @param  None
  * @retval None
  */
void configureIOPins(void)
{
  pin_size_t        pinNo;

  for ( int i = 0; i < static_pin_count; i++ )
  {
      pinNo = staticPins[i].pinNo;
      pinMode(pinNo, staticPins[i].pinFunc);

      if ( staticPins[i].pinFunc == OUTPUT )
      {
          // increase drive strength on output pins
          // see ttf/variants.cpp for the data in g_APinDescription[]
          // NOTE: this will source 7mA, sink 10mA
          PORT->Group[g_APinDescription[pinNo].ulPort].PINCFG[g_APinDescription[pinNo].ulPin].bit.DRVSTR = 1;

          // deassert pin
          writePin(pinNo, (staticPins[i].activeState == ACT_LO) ? 1 : 0);
      }
  }
}

//===================================================================
//                    READ, WRITE COMMANDS
//===================================================================

/**
  * @name   readPin
  * @brief  wrapper to digitalRead via pinStates[]
  * @param  None
  * @retval None
  */
bool readPin(uint8_t pinNo)
{
    uint8_t         index = getPinIndex(pinNo);

    if ( staticPins[index].pinFunc == INPUT )
        pinStates[index] = digitalRead((pin_size_t) pinNo);

    return(pinStates[index]);
}

/**
  * @name   writePin
  * @brief  wrapper to digitalWrite via pinStates[]
  * @param  pinNo = Arduino pin #
  * @param  value = value to write 0 or 1
  * @retval None
  */
void writePin(uint8_t pinNo, uint8_t value)
{
    value = (value == 0) ? 0 : 1;
    digitalWrite(pinNo, value);
    pinStates[getPinIndex(pinNo)] = (bool) value;
}

/**
  * @name   readCmd
  * @brief  read an I/O pin
  * @param  arg 1 = Arduino pin #
  * @retval 0=OK 1=pin # not found
  * @note   displays pin info
  */
int readCmd(int arg)
{
    uint8_t       pinNo = atoi(tokens[1]);
    uint8_t       index = getPinIndex(pinNo);

    if ( pinNo > PINS_COUNT )
    {
        terminalOut((char *) "Invalid pin number; please use Arduino numbering");
        return(1);
    }

    (void) readPin(pinNo);
    sprintf(outBfr, "%s Pin %d (%s) = %d", (staticPins[index].pinFunc == INPUT) ? "Input" : "Output", 
            pinNo, getPinName(pinNo), pinStates[index]);
    terminalOut(outBfr);
    return(0);
}

/**
  * @name   writeCmd
  * @brief  write a pin with 0 or 1
  * @param  arg 1 Arduino pin #
  * @param  arg 2 value to write
  * @retval None
  */
int writeCmd(int argCnt)
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

/**
  * @name   
  * @brief  
  * @param  None
  * @retval 0
  */
char getPinChar(int index)
{
    if ( staticPins[index].pinFunc == INPUT )
        return('<');
    else if ( staticPins[index].pinFunc == OUTPUT )
        return('>');
    else
        return('=');
}

/**
  * @name   pinCmd
  * @brief  display I/O pins
  * @param  argCnt = CLI arg count
  * @retval None
  */
int pinCmd(int arg)
{
    int         count = static_pin_count;
    int         index = 0;

    terminalOut((char *) " ");
    terminalOut((char *) " #           Pin Name   D/S              #        Pin Name      D/S ");
    terminalOut((char *) "-------------------------------------------------------------------- ");

	readAllPins();
	
    while ( count > 0 )
    {
      if ( count == 1 )
      {
          sprintf(outBfr, "%2d %20s %c %d ", staticPins[index].pinNo, staticPins[index].name,
                  getPinChar(index), readPin(staticPins[index].pinNo));
          terminalOut(outBfr);
          break;
      }
      else
      {
          sprintf(outBfr, "%2d %20s %c %d\t\t%2d %20s %c %d ", 
                  staticPins[index].pinNo, staticPins[index].name, 
                  getPinChar(index), readPin(staticPins[index].pinNo),
                  staticPins[index+1].pinNo, staticPins[index+1].name, 
                  getPinChar(index+1), readPin(staticPins[index+1].pinNo));
          terminalOut(outBfr);
          count -= 2;
          index += 2;
      }
    }

    terminalOut((char *) "D/S = Direction/State; < input, > output");
    return(0);
}

//===================================================================
//                         Status Display Screen
//===================================================================

/**
  * @name   padBuffer
  * @brief  pad outBfr with spaces 
  * @param  pos = position to start padding at in 'outBfr'
  * @retval None
  */
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

/**
  * @name   readAllPins
  * @brief  read all I/O pins into pinStates[]
  * @param  None
  * @retval None
  */
void readAllPins(void)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        (void) readPin(staticPins[i].pinNo);
    }
}

/**
  * @name   statusCmd
  * @brief  display status screen
  * @param  argCnt = number of CLI arguments
  * @retval None
  */
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
        sprintf(outBfr, "TEMP WARN         %d", readPin(TEMP_WARN));
        displayLine(outBfr);

        CURSOR(3,57);
        sprintf(outBfr, "P1_LINK_A_N      %u", readPin(P1_LINKA_N));
        displayLine(outBfr);

        CURSOR(4,1);
        sprintf(outBfr, "TEMP CRIT         %u", readPin(TEMP_CRIT));
        displayLine(outBfr);

        CURSOR(4,56);
        sprintf(outBfr, "PRSNTB [3:0]   %u%u%u%u", readPin(OCP_PRSNTB3_N), readPin(OCP_PRSNTB2_N), 
                readPin(OCP_PRSNTB1_N), readPin(OCP_PRSNTB0_N));
        displayLine(outBfr);

        CURSOR(5,1);
        sprintf(outBfr, "FAN ON AUX        %u", readPin(FAN_ON_AUX));
        displayLine(outBfr);

        CURSOR(5,58);
        sprintf(outBfr, "LINK_ACT_2      %u", readPin(LINK_ACT_2));
        displayLine(outBfr);

        CURSOR(6,1);
        sprintf(outBfr, "SCAN_LD_N         %d", readPin(OCP_SCAN_LD_N));
        displayLine(outBfr);

        CURSOR(6,53);
        sprintf(outBfr, "SCAN VERS [1:0]     %u%u", readPin(SCAN_VER_1), readPin(SCAN_VER_0));
        displayLine(outBfr);

        CURSOR(7,1);
        sprintf(outBfr, "AUX_EN            %d", readPin(OCP_AUX_PWR_EN));
        displayLine(outBfr);      

        CURSOR(7,60);
        sprintf(outBfr, "PWRBRK_N      %d", readPin(OCP_PWRBRK_N));
        displayLine(outBfr);

        CURSOR(8,1);
        sprintf(outBfr, "MAIN_EN           %d", readPin(OCP_MAIN_PWR_EN));
        displayLine(outBfr);  

        CURSOR(8,62);
        sprintf(outBfr, "WAKE_N      %d", readPin(OCP_WAKE_N));
        displayLine(outBfr);

        CURSOR(9,1);
        sprintf(outBfr, "P3_LED_ACT_N      %d", readPin(P3_LED_ACT_N));
        displayLine(outBfr);  

        CURSOR(9,58);
        sprintf(outBfr, "P3_LINKA_N      %d", readPin(P3_LINKA_N));
        displayLine(outBfr);

        CURSOR(10,1);
        sprintf(outBfr, "P1_LED_ACT_N      %d", readPin(P1_LED_ACT_N));
        displayLine(outBfr);

        CURSOR(10, 58);
        sprintf(outBfr, "NCSI_RST_N      %d", readPin(NCSI_RST_N));
        displayLine(outBfr);

        if ( oneShot )
        {
            CURSOR(12,1);
            displayLine((char *) "Status delay 0, set sdelay to nonzero for this screen to loop.");
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


/**
  * @name   getPinName
  * @brief  get name of pin
  * @param  Arduino pin number
  * @retval pointer to name or 'unknown' if not found
  */
const char *getPinName(int pinNo)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        if ( pinNo == staticPins[i].pinNo )
            return(staticPins[i].name);
    }

    return("Unknown");
}

/**
  * @name   getPinIndex
  * @brief  get index into static/dynamic pin arrays
  * @param  Arduino pin number
  * @retval None
  */
int8_t getPinIndex(uint8_t pinNo)
{
    for ( int i = 0; i < static_pin_count; i++ )
    {
        if ( staticPins[i].pinNo == pinNo )
            return(i);
    }

    return(-1);
}

/**
  * @name   set_help
  * @brief  help for set command
  * @param  None
  * @retval None
  */
void set_help(void)
{
    terminalOut((char *) "FLASH Parameters are:");
    sprintf(outBfr, "  sdelay <integer> - status display delay in seconds; current: %d", EEPROMData.status_delay_secs);
    terminalOut(outBfr);
    sprintf(outBfr, "  pdelay <integer> - power up sequence delay in milliseconds; current: %d", EEPROMData.pwr_seq_delay_msec);
    terminalOut(outBfr);
    terminalOut((char *) "'set <parameter> <value>' sets a parameter from list above to value");
    terminalOut((char *) "  value can be <integer>, <string> or <float> depending on the parameter");

    // TODO add more set command help here
}

/**
  * @name   setCmd
  * @brief  Set a parameter (seeing) in FLASH
  * @param  arg 1 = parameter name
  * @param  arg 2 = value to set
  * @retval 0
  * @note   no args shows help w/current values
  * @note   simulated EEPROM is called FLASH to the user
  */
int setCmd(int argCnt)
{
    char          *parameter = tokens[1];
    String        valueEntered = tokens[2];
//    float         fValue;
    int           iValue;
    bool          isDirty = false;

    if ( argCnt == 0 )
    {
        set_help();
        return(0);
    }
    else if ( argCnt != 2 )
    {
        set_help();
        return(0);
    }

    if ( strcmp(parameter, "sdelay") == 0 )
    {
        iValue = valueEntered.toInt();
        if (EEPROMData.status_delay_secs != iValue )
        {
          isDirty = true;
          EEPROMData.status_delay_secs = iValue;
        }
    }
    else if ( strcmp(parameter, "pdelay") == 0 )
    {
        iValue = valueEntered.toInt();
        if (EEPROMData.pwr_seq_delay_msec != iValue )
        {
          isDirty = true;
          EEPROMData.pwr_seq_delay_msec = iValue;
        }
    }
    else
    {
        terminalOut((char *) "Invalid parameter name");
        set_help();
        return(1);
    }

    if ( isDirty == true )
        EEPROM_Save();

    return(0);

} // setCmd()

static void pwrCmdHelp(void)
{
    terminalOut((char *) "Usage: power <up | down | status> <main | aux | card>");
    terminalOut((char *) "  'power status' requires no argument and shows the power status of NIC card");
    terminalOut((char *) "  main = MAIN_EN to NIC card; aux = AUX_EN to NIC card; ");
    terminalOut((char *) "  card = MAIN_EN=1 then pdelay msecs then AUX_EN=1; see 'set' command for pdelay");
}

/**
  * @name   pwrCmd
  * @brief  Control AUX and MAIN power to NIC 3.0 board
  * @param  argCnt  number of arguments
  * @param  tokens[1]  up, down or status
  * @param  tokens[2]   main, aux or card
  * @retval 0   OK
  * @retval 1   error
  * @note   Delay is changed with 'set pdelay <msec>'
  * @note   power <up|down|status> <main|aux|board> status has no 2nd arg
  */
int pwrCmd(int argCnt)
{
    int             rc = 0;
    bool            isPowered = false;
    uint8_t         mainPin = readPin(OCP_MAIN_PWR_EN);
    uint8_t         auxPin = readPin(OCP_AUX_PWR_EN);

    if ( argCnt == 0 )
    {
        pwrCmdHelp();
        return(1);
    }

    if (  mainPin == 1 &&  auxPin == 1 )
        isPowered = true;

    if ( argCnt == 1 )
    {
        if ( strcmp(tokens[1], "status") == 0 )
        {
            sprintf(outBfr, "Status: NIC card is powered %s", (isPowered) ? "up" : "down");
            SHOW();
            return(rc);
        }
        else
        {
            terminalOut((char *) "Incorrect number of command arguments");
            pwrCmdHelp();
            return(1);
        }
    }
    else if ( argCnt != 2 )
    {
        terminalOut((char *) "Incorrect number of command arguments");
        pwrCmdHelp();
        return(1);
    }

    if ( strcmp(tokens[1], "up") == 0 )
    {
        if ( strcmp(tokens[2], "card") == 0 )
        {
            if ( isPowered == false )
            {
                sprintf(outBfr, "Starting NIC power up sequence, delay = %d msec", EEPROMData.pwr_seq_delay_msec);
                SHOW();
                writePin(OCP_MAIN_PWR_EN, 1);
                delay(EEPROMData.pwr_seq_delay_msec);
                writePin(OCP_AUX_PWR_EN, 1);
                queryScanChain();
                terminalOut((char *) "Power up sequence complete");
            }
            else
            {
                terminalOut((char *) "Power is already up on NIC card");
            }
        }
        else if ( strcmp(tokens[2], "main") == 0 )
        {
            if ( mainPin == 1 )
            {
                terminalOut((char *) "MAIN_EN is already 1");
                return(0);
            }
            else
            {
                writePin(OCP_MAIN_PWR_EN, 1);
                terminalOut((char *) "Set MAIN_EN to 1");
                return(0);
            }
        }
        else if ( strcmp(tokens[2], "aux") == 0 )
        {
            if ( auxPin == 1 )
            {
                terminalOut((char *) "AUX_EN is already 1");
                return(0);
            }
            else
            {
                writePin(OCP_AUX_PWR_EN, 1);
                terminalOut((char *) "Set AUX_EN to 1");
                return(0);
            }
        }
        else
        {
            terminalOut((char *) "Invalid argument");
            pwrCmdHelp();
            return(1);
        }
    }
    else if ( strcmp(tokens[1], "down") == 0 )
    {
        if ( strcmp(tokens[2], "card") == 0 )
        {
            if ( isPowered == true )
            {
                writePin(OCP_MAIN_PWR_EN, 0);
                writePin(OCP_AUX_PWR_EN, 0);
                terminalOut((char *) "Powered down NIC card");
            }
            else
            {
                terminalOut((char *) "Power is already down on NIC card");
            }
        }
        else if ( strcmp(tokens[2], "main") == 0 )
        {
            if ( mainPin == 0 )
            {
                terminalOut((char *) "MAIN_PWR_EN is already 0");
                return(0);
            }
            else
            {
                writePin(OCP_MAIN_PWR_EN, 0);
                terminalOut((char *) "Set MAIN_PWR_EN to 0");
                return(0);
            }
        }
        else if ( strcmp(tokens[2], "aux") == 0 )
        {
            if ( auxPin == 0 )
            {
                terminalOut((char *) "AUX_PWR_EN is already 0");
                return(0);
            }
            else
            {
                writePin(OCP_AUX_PWR_EN, 0);
                terminalOut((char *) "Set AUX_PWR_EN to 0");
                return(0);
            }
        }
        else
        {
            terminalOut((char *) "Invalid argument");
            pwrCmdHelp();
            return(1);
        }
    }
    else
    {
        terminalOut((char *) "Invalid subcommand: use 'up', 'down' or 'status'");
        rc = 1;
    }

    return(rc);
}

/**
  * @name   versCmd
  * @brief  Displays firmware version
  * @param  arg not used
  * @retval None
  */
int versCmd(int arg)
{
    sprintf(outBfr, "Firmware version %s built on %s at %s", VERSION_ID, BUILD_DATE, BUILD_TIME);
    terminalOut(outBfr);
    return(0);
}

/**
  * @name   queryScanChain
  * @brief  extract info from scan chain output
  * @param  None
  * @retval None
  */
void queryScanChain(void)
{
    // TODO implementation
    terminalOut((char *) "Scan chain info not available");
}
