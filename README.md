# m1k-firmware

[![Build Status](https://travis-ci.com/zaunkoenig-firmware/m1k-firmware.svg?branch=master)](https://travis-ci.com/zaunkoenig-firmware/m1k-firmware)

This is the open-source firmware for the [Zaunkoenig M1K](https://zaunkoenig.co/). 

## This is how to install a new firmware on the M1K on Windows:
1. [Download the command-line programmer for Atmel USB microcontrollers called «Atmel USB DFU Programmer».](https://sourceforge.net/projects/dfu-programmer/)
2. After you installed DFU Programmer open up the folder «dfu-prog-usb-1.2.2» and right click on atmel_usb_dfu.inf and click on «install».
3. Copy the firmware files you want to install into the DFU Programmer folder. You need one file ending with .eep and one ending with .hex.
4. Now you have to put the M1K into the «firmware update mode». There are two ways to do that. The first way is to short-circuit the two golden pins to the right side of the left switch: you know you succeeded when the mouse cursor freezes. However, for this method you have to open your mouse. The other way is possible without having to open up your M1K:
5. Plug in your mouse while holding down both mouse buttons. After five seconds the cursor will move clockwise in a square to indicate that Angle Snapping and LOD have been changed to default values. This time though, do not let go of your mouse buttons yet. Keep them pressed down for five more seconds until your mouse cursor has frozen. What follows is how you would install a firmware called example.hex:
6. Start the Windows Command Prompt and execute the following:
7. `cd c:\program files (x86)\dfu-programmer-win-0.7.2`
8. Now run the following four commands:
9. `dfu-programmer atmega32u2 erase --force`
10. `dfu-programmer atmega32u2 flash example.hex`
11. `dfu-programmer atmega32u2 flash-eeprom example.eep --force`
12. `dfu-programmer atmega32u2 start`
13. After you executed the start command your mouse should be up and running with the new firmware.
