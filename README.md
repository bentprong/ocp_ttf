# Dell OCP NIC 3.0 Thermal Test Fixture
Written by Richard Lewis rlewis@astlenterprises.com for Fusion Manufacturing Services 
   rick@fmspcb.com. 
Posted February 18, 2023 at [GitHub](https://github.com/bentprong/ocp_ttf)
Initial Firmware Release: v1.0.5 (March 9, 2023)

## Release v1.0.5 Notes
1. Scan chain query is not yet implemented.
2. Only the Board Info Area of the FRU EEPROM is currently processed.  Comments welcome. See
   Issue # 9.

## Overview
These instructions have been tested on Windows and Mac.  If you are using a variant of Linux, the
instructions for Mac should apply except possibly for the "screen" terminal emulator program.  At
a minimum you need to select a Linux terminal emulation program and open the USB TTY with it.

The project is built using the PlatformIO extension for Visual Studio Code.  An ATMEL-ICE was
used to debug the code on the board and can be used to program the binary using Microchip Studio.

## Operating Instructions
Enter the 'help' command to get a list of the available commands, and details about usage of
each command.

The simulated EEPROM (in FLASH) is used to store 2 settings:
   sdelay - delay in seconds between status screen updates [default 3]
   pdelay - delay in milliseconds between asserting MAIN_EN and AUX_EN signals to power up
       the NIC 3.0 board [default 250]

Use the 'set <param> <value>' command to change these settings.

Do  not confuse this simulated EEPROM with the FRU EEPROM on a NIC 3.0 board.  The command to
access FRU EEPROM contents is just 'eepom' (see help for more).

The signature of the simulated EEPROM should always be DE110C03.  Decoded, this means:
   "DE11" = Dell
   "0C" = Open Compute
   "03" = 3rd OCP project (TTF); 01=Vulcan, 02=Xavier

---
WARNING: Flashing the board with (new) firmware WILL erase the EEPROM and you will need to re-enter
the settings afterwards.  The board will display "EEPROM Validation Failed..." to indicate that
this has happened, but it is a good practice to always check the settings after flashing the
board's firmware.  See [defaults] listed above.
---

### Tips:
Backspace and delete are implemented and erase the previous character typed.
Up arrow executes the previous command.
In this document, <ENTER> means press the keyboard Enter key.

## Getting Started
Follow the Wiring and Terminal Instructions below to get started using the TTF board.

The board firmware prompt is "ttf>" and when you see that, you can enter "help<ENTER>" for help on the
available commands.

The purpose of the board is to provide support for thermal testing of NIC 3.0 cards.  

## Development Environment Setup
This varies slightly between platforms including Mac, Linux and Windows.  This is NOT needed if you only want to
flash the firmware into a board - see Binary Executable Instructions below in that case.

1. Download and install Visual Studio Code.

2. In VS Code ("VSC"), go to Extensions and seach for "platformio" and install it.  This will take some time, so
watch the status area in the bottom right of VSC for progress.  Note that PlatformIO ("PIO") also installs C/C++ 
extensions which are needed.

When finished you will see a prompt "Please restart VSCode" so do that.

Windows only: It is recommended that you install cygwin so that you have a bash terminal to use git.  There are other 
options such as GitBash.  Point is, command-line git needs to be run to clone the source code repo unless you 
already have/know a tool that will allow you to clone and manage a GitHub repository.

3. Set up a Projects folder if you don't already have one.  For these instructions it is assumed that this is
<home>/Documents/Projects.  VSC "may" be able to accomodate other directory structures, but of course, those
have not been tested.

    Windows:  <home> = C:/Users/<username>
    Mac:      <home> = Users/<username>

4. Log into GitHub.com using your OWN PERSONAL credentials then clone this repository: 
    bentprong/ocp_ttf 
    
into your Projects folder.  Eg, <home>/Documents/Projects/ocp_ttf

    GitHub Requirements:
        a. SSH key generated and installed on this (YOUR) computer for YOU (new GitHub requirement)
        b. SSH key for YOU installed in YOUR GitHub.com account

5. In VSC, choose File | Open Folder... and navigate to <home>/<Projects>/ocp_ttf then highlight that, and
click Select Folder.

6. In VSC, click the checkmark in the blue bar at the bottom to build.  This should install necessary files, 
libraries and tools.  It may take quite a bit of time.  This builds the release code - not debug code. The
release code is flashed to the board using Microchip Studio usually, while the debug code is used by VSC/PIO
for debugging purposes.

7. In the repo folder platformio, open the README file and follow the instructions to configure PIO for the
TTF board.  There are 2 steps to this process that are explained in the README.

---
** FLASHING NOTE ** Failure to exactly follow the instructions in the README in step #7 will
result in the code not building correctly!   That is because the files that must be copied or
updated define the pinouts for the TTF project.  You have been warned!
---

## Wiring Instructions
1. Connect TTF board to ATMEL-ICE and connect ATMEL-ICE to computer
2. Connect TTF board USB-C port to computer USB port using a DATA CABLE (not a charging only cable).
3. Windows only: If not already installed, install a terminal emulator program such as TeraTerm.
 
LED BEHAVIOR: Once the TTF board has been powered up, there will be 2 LEDs that are on solid: one
for 3.3V and one for 12V.  These 2 LEDs are near the Power connector.  If either is off, a hardware issue
exists in one or both power supplies.  These 2 LEDs should always be on.

A third LED may be lit if a NIC 3.0 board is inserted into the bay and the power is good to that board. 
This is the OCP_PWR_GOOD LED.  UPDATE: The NIC 3.0 board is now no longer powered up when the TTF
board is powered up.  Use the 'power' command to power up the NIC 3.0 board.

## Build/Debug Instructions
In VSC, click Run | Start Debugging.  The code will be built for debug and you should see the debugger
stop in main() at the init() call.   Click the blue |> icon in the debugger control area of VSC.

## Binary Executable Instructions
Firmware is prebuilt in GitHub and located at:
    bentprong/ocp_ttf/.pio/build/samd21g18a/firmware.bin

    https://github.com/bentprong/ocp_ttf/tree/main/.pio/build/samd21g18a

Use any flash utility such as Microchip Studio to erase and flash this .bin file into the TTF board.

NOTE: PIO "upload" does not work because there is intentionally no bootloader on the TTF board.  At this
time, Dell OCP does not support Arduino sketches.

## Building Release Firmware
In VSC, click the checkmark in the blue line at the bottom to build a firmware release.  If no problems
are reported (there should be none), the executable is located in the local directory here:
    <home>/Projects/ocp-ttf/.pio/build/samd21g18a/firmware.bin

Note that in this same location is also the firmware.elf file which is the debug version of firmware.

## Microchip Studio
1. Open Studio then select Tools | Device Programming.
2. Select Atmel-ICE from the Tool pulldown menu.  Verify that the Device is ATSAMD21G18A then click
the Apply button.  A menu of options will appear on the left of the screen now.
3. Select Memories then Erase Chip & Erase now.
4. In the Flash (256KB) section, use the ... button to navigate to the binary file described above.
That full path and filename should be shown in the long text area.
5. Click the Program button and wait for the status messages which should be:
    Erasing device...OK
    Programming Flash...OK
    Verifying Flash...OK
6. Click the Close button.

---
NOTE:  There may be a conflict between VSC and Microchip Studio if you are trying to use Studio to
flash the firmware.  Close out VSC and Studio, then restart Studio.  The conflict would be in these
2 software programs trying to use the Atmel-ICE at the same time.  If the problem persists, close
Studio, unplug the Atmel-ICE, wait a few seconds, plug the Atmel-ICE back in, then restart Studio.
---

## Terminal Instructions
Windows: In TeraTerm, open a new serial connection on the new COM port and press ENTER. You should see
the TTF welcome message and TTF prompt ttf> in the TeraTerm window.  Hint: The description of the new
COM port will include "OCP" if that COM port is assigned to the TTF board.

Mac: Get a listing of TTYs like this:
    user@computer ~ % ls -l /dev/tty.usb*
    crw-rw-rw-  1 root  wheel    9,   2 Jan 17 14:02 /dev/tty.usbmodem146201 

Enter "screen /dev/tty.usbmodem146201 115200" (or whatever the output of the ls command indicates)
and you should see the TTF welcome message and TTF prompt ttf> in the terminal window.  While the
baud rate of 115200 doesn't apply to serial over USB, it is required by the screen command.

Linux (eg Ubuntu): You can install screen or minicom using apt.  For screen, use the ls command
as shown in the Mac section above to find the USBn device, then enter the command:
"screen /dev/ttyUSB0 115200" if the connection is on ttyUSB0.  For minicom, please search online
for a tutorial on installation and usage.

## Issues
See:
    https://github.com/bentprong/ocp_ttf/issues

