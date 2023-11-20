# THRII_USB_Pedal
**Another usb pedalboard for Yamaha THRII guitar amplifier series**

This project is based on the hard work and good results from martinzw and BuhJuhWuh:
<br> https://github.com/martinzw/THRII-direct-USB-pedalboard/tree/main
<br> https://github.com/BuhJuhWuh/THR30II-pedal/tree/main


**INTRODUCTION**

I am not familiar with neither Arduino nor Visual Studio Code (PlatformIO feature).

Initially, I stared with the code from https://github.com/martinzw/THRII-direct-USB-pedalboard/tree/main, however, I could not successfully compile it for Teensy 4.1 (it is written for Teensy 3.6). Therefore, I continued with the code from https://github.com/BuhJuhWuh/THR30II-pedal/tree/main, which was developed for Teensy 4.1. 
I set Visual Studio Code (PlatformIO) with all libraries needed but could not initially succeed compiling the code. After fixing the compilation errors, I could not make the 2" TFT with ST7789 controller working. Finally, I gave up Visual Studio Code and started from scratch with pure Arduino: adding Teensy support, adding the source code, installing the needed libraries, fixing compiler errors, etc. 

After some fixes, it seems that the GUI features from BuhJuhWuh's code base work now.


**FIXES/CHANGES THAT WERE NEEDED**

- I use THR10II-WIRELESS, 1.44.0.a fw version. Family ID and model number are hardcoded in the communication. Model number is different for each THRII flavour 
- Compiler error when using PROGMEM
  - Gives issues with fonts and patches.h
  - PROGMEM is currently not used
- Compiler error in the move constructor 'noexcept' 
  - See "SysExMessage(SysExMessage &&other ) noexcept; // Move Constructor" in THR30II.h
  - 'noexcept' word was removed 
- SysExMessage and OutMessage, when reused, caused run-time crashes
  - Code changed to instantiate new message every time it is used
  - See "SysExMessage m(sb.data(), sblast - sb.begin());" in CreatePatch()
- ST7789 controller is selected and SPI is configured in User_Setup.h of TFT_eSPI library source!


**CURRENT BEHAVIOUR**
- LED strip support removed, was in BuhJuhWuh version
- No analogue input support (for now)
- The AudioVolume and GuitarVolume parameters are shown as bars (GA) on the right part of the TFT screen i.s.o. the analogue pedals values (P).
- At start-up and connection with THR, GUI shows "INITIALIZING" and then "*THR* PANEL". 
  It checks and prints the actual THR name, e.g., in my case *THR* = **THR10II-W**.   
  It changes to a User preset name when selected from the THRII amp or by pressing Button 6.
- The GUI continues to work even THR is disconnected after that (useful to test pure GUI features)
- Patch name color changes to orange when parameter values are changed
  - Switching patches discard the changes - nothing is stored
- User preset name color changes to orange when parameter values are changed
  - The status is remembered when switching between User presets and patches (in the original code it was not)
  - In the original code '\*' was appended to the name when modified. To save space, this feature is removed

*BUTTONS (Original Layout)*
- Selecting a patch (Button 1), switches to a patch (white color)
  - Pressing Button 1 again switches to THR settings (THR Local, User preset, or User preset(*))
  - Pressing Button 1 again loads the preselected patch again (reloads, possible parameter changes are lost)
- Button 2 works as intended - cycle between AMP/COL/Cabinets.
  - Selection to what to cycle - press and hold
- Button 3 (Tap echo time) works as well.
  - Sets the time to both echo effects, some check and fine tuning might be needed 
- Button 4: patch select decrement by 1
  - If hold - switch to "load patch immediately" - no need to press button 1 to do so
- Button 5: patch select increment by 1
  - If hold - patch select increment by 5
- Button 6 (New): Cycle through the User presets (1-5) in the THRII amp
  - User preset is selected by sending a command to THRII. The User preset number is shown on THRII display
  - If (and initially) a User preset settings are not available on the pedal board, then settings are also requested and stored locally
  - If a User preset button on THRII is pressed, the preset settings are also requested by the pedal board and stored locally
  - When next time the same preset is selected via Button 6, the settings are not requested again. Only the command to switch to the User preset is sent to THRII. Locally stored settings are used to update the pedal board display. This gives better responsiveness of the GUI
- Button 7: On/off of selected: Comp/Gate/Boost switch
  - If hold - select (cycle through) Comp/Gate/Boost switch
- Boost function uses Master (was Gain)
  - If Master is not enough, then include also the EQ
- Button 8: On/off effect
  - If hold - cycle through and select effects
- Button 9: On/off echo
  - If hold - cycle through and select echos
- Button 10: On/off reverb
  - If hold - cycle through and select reverbs


**KNOWN ISSUES**
- Selecting different effects/echoes/reverbs does not change patch name color
- When changing what to switch (Button 7 - COMP/GATE/BOOST), it is not clear what has been selected
- Font size of the patch name is not ideal (currently, using smaller font size instead)


**DONE**
- Use button press event only for the tap echo time button/feature
- Remove '(\*)' from modified patch to save space on screen - change only the color of the patch name
- Invert patch name font color and background when Solo switch on
- When switching from patch to the THRII settings, the THRII (user preset) patch name is shown but the small-font number (0-4) was not
- At connection, start with 'THR Local' and show the (large-font) number of the preselected patch (1), was (-1)
- Select User presets from the pedal board (number of preset to be on the THR display)
  - Currently sends a complete patch to be loaded as active settings

**TODOs**
- Refactor: Move code to different files: SDCard, FSM, etc.
- Introduce button mapping (it has been removed)
- Define different FSMs as functions, currently fsm is in the loop() function
- Use Family ID and model number: obtain after connection and use in communication
  - In this way, all THRxII flavour should be supported
- Add new fonts - currently facing problems with this :(
- Cannot switch THRII amp/col without calling createPatch() 
  - Can switch from Boom SendSX Midi SW sending hex data. The 'same' data does not switch amp/col when sent from Teensy
- Use Guitar volume for Boost (Solo switch). Currently uses Master+EQ. Make it selectable?
- Define useable settings for all parameters/effects and initialize them at startup. I
  - In this case even if nothing is loaded from the amp, effects can be switched and used. Currently, they can be switched but not very usable
- Define new 10-button, 9-button, and 7-button versions
- Define Manual Mode, Bank/Patch/Memory Mode (with dedicated screen layout)


**FURTHER TODOs**
- Create an Edit mode
  - With zoomed-in effects 
  - Use Audio volume knob of the THRII amp to set parameters values
  - Setting patch name can be a lot of effort (does it worth the effort?)
- Write patches to sdcard
- Use guitar volume to adjust patch volume
  - Purpose: different patches to have similar overall volume
  - Extra info needs to be added to the patch preset! Does it worth the effort?
- Use a default symbol table to initialize the SW in case THRII is not connected
  - To be able to use the SW 'off line' for the FSM/GUI development 


**DISCLAIMER**
<br> In addition to the MIT license disclaimer about the software, consider the same disclaimer for hardware suggestions and all information in this project. Similarly to martinzw and BuhJuhWuh, I can not ensure, that sending messages in the suggested way will not damage the THRII. As well I can not guarantee, that the hardware, I describe will not damage your THRII device or it's power supply or your computer.
Finally, I have no connection to Yamaha / Line6. Every knowledge about the data communications protocol was gained by experimenting, try and error and by looking at the data stream. 
