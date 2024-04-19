# THRII_USB_Pedal
**Another usb pedalboard for Yamaha THRII guitar amplifier series**

This project is based on the hard work and good results from martinzw and BuhJuhWuh:
<br> https://github.com/martinzw/THRII-direct-USB-pedalboard/tree/main
<br> https://github.com/BuhJuhWuh/THR30II-pedal/tree/main


**INTRODUCTION**

Initially, I stared with the code from https://github.com/martinzw/THRII-direct-USB-pedalboard/tree/main, however, I could not successfully compile it for Teensy 4.1 (it is written for Teensy 3.6). Therefore, I continued with the code from https://github.com/BuhJuhWuh/THR30II-pedal/tree/main, which was developed for Teensy 4.1. 
I set Visual Studio Code (PlatformIO) with all libraries needed but could not initially succeed compiling the code. After fixing the compilation errors, I could not make the 2" TFT with ST7789 controller working. Finally, I gave up Visual Studio Code and started from scratch with pure Arduino: adding Teensy support, adding the source code, installing the needed libraries, fixing compiler errors, etc. 


**FIXES/CHANGES THAT WERE NEEDED**

- Compiler error in the move constructor 'noexcept' 
  - See "SysExMessage(SysExMessage &&other ) noexcept; // Move Constructor" in THR30II.h
  - 'noexcept' word was removed 
- SysExMessage and OutMessage, when reused, caused run-time crashes
  - Code changed to instantiate new message every time it is used
  - See "SysExMessage m(sb.data(), sblast - sb.begin());" in CreatePatch()
- setup_t conflict in TFT_eSPI library

**HARDWARE USED**
- Teensy 4.1
- 3.2" TFT with ILI9341 controller is used. It is selected and SPI is configured in User_Setup.h of TFT_eSPI library source!
- 9 buttons for guitar stomp-boxes
- Standard cheap buzzer (hat) for the metronome
- TODO: more details

**OPERATION MODES**

***THRII Presets (1-5)***
<br> This is the default mode which is selected after connecting to THRII
- At start-up and after connecting to THR, the GUI shows "INITIALIZING" and then "*THR* PANEL".
  It checks and shows the actual THR name, e.g., in my case *THR* = **THR10II-W**.   
  It changes to a preset name when selected from the THRII amp or by pressing Buttons 5-9
- The GUI continues to work even THR is disconnected - useful to test pure GUI features
- Preset name color changes to orange when parameter values are changed from THRII
  - Switching presets discard the changes - nothing is stored

![THRII USB Pedal](images/thr_presets.png)

| Button | Action | Function                            |
|--------|--------|-------------------------------------|
|   1    | Press  |                                     |
|        | Hold   | Toggle THRII/User(Factory) presets  |
|   2    | Press  |                                     |
|        | Hold   | Toggle User/Factory presets         |
|   3    | Press  | Toggle “Manual” mode                |
|        | Hold   | Toggle “Edit” mode                  |
|   4    | Press  | TAP Echo/Delay time                 |
|        | Hold   | Toggle Tabata/Metronome mode        |
|   5    | Press  | Select THRII preset/memory 1. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   6    | Press  | Select THRII preset/memory 2. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   7    | Press  | Select THRII preset/memory 3. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   8    | Press  | Select THRII preset/memory 4. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   9    | Press  | Select THRII preset/memory 5. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |


***User Presets / Factory Presets***
- User presets are loaded from SD card at start-up
- The THRII Factory presets provided with the remote App are copied in this repo. if present on the SD card, they are loaded as well
- User presets use the same format and file type as the THRII factory presets (.thrl6p)
- User/Factory preset name color changes to orange when parameter values are changed
  - The status is remembered when switching between User/Factory and THRII presets

![THRII USB Pedal](images/presets.png)

| Button | Action | Function                            |
|--------|--------|-------------------------------------|
|   1    | Press  | Bank up (+5 presets)                |
|        | Hold   | Toggle THRII/User(Factory) presets  |
|   2    | Press  | Bank down (-5 presets)              |
|        | Hold   | Toggle User/Factory presets         |
|   3    | Press  | Toggle “Manual” mode                |
|        | Hold   | Toggle “Edit” mode                  |
|   4    | Press  | TAP Echo/Delay time                 |
|        | Hold   | Toggle Tabata/Metronome mode        |
|   5    | Press  | Select preset 1 from the current bank. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   6    | Press  | Select preset 2 from the current bank. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   7    | Press  | Select preset 3 from the current bank. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   8    | Press  | Select preset 4 from the current bank. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |
|   9    | Press  | Select preset 5 from the current bank. Press again for “Solo boost” (on/off) |
|        | Hold   |                                     |


  
***Manual mode***

- In manual mode, individual units can be selected and switched on/off by using the pedal board
- Amp model/collection and speakers can be changed as well
- In manual mode, bank up/down buttons are used to select next/previous preset

![THRII USB Pedal](images/manual.png)

| Button | Action | Function                                       |
|--------|--------|------------------------------------------------|
|   1    | Press  | Next preset (+1)                               |
|        | Hold   | Toggle Collection/Amp/Cabinet                  |
|   2    | Press  | Previous preset (-1)                           |
|        | Hold   | Next from selected Collection, Amp, or Cabinet |
|   3    | Press  | Toggle “Manual” mode                           |
|        | Hold   | Toggle “Edit” mode                             |
|   4    | Press  | TAP Echo/Delay time                            |
|        | Hold   | Toggle Tabata/Metronome mode                   |
|   5    | Press  | Toggle Compressor (on/off)                     |
|        | Hold   |                                                |
|   6    | Press  | Toggle “Solo boost” (on/off)                   |
|        | Hold   | Toggle Noise Gate (on/off)                     |
|   7    | Press  | Toggle Effect (on/off)                         |
|        | Hold   | Select Effect (Chorus/Flanger/Phaser/Tremolo)  |
|   8    | Press  | Toggle Delay (on/off)                          |
|        | Hold   | Select Delay (Tape/Digital)                    |
|   9    | Press  | Toggle Reverb (on/off)                         |
|        | Hold   | Select Reverb (Spring/Plate/Room/Hall)         |


***Edit mode***

- THRII knobs can be used to directly change parameter settings. New values are reflected in the GUI
- The THRII Audio knob can be used to change a parameter value. Press buttons 1 and 2 to select a parameter, then use the audio knob to set value

![THRII USB Pedal](images/edit.png)

| Button | Action | Function                                                       |
|--------|--------|----------------------------------------------------------------|
|   1    | Press  | Select Next parameter                                          |
|   	 | Hold	  | Toggle Collection/Amp/Cabinet                                  |
|   2    | Press  | Select Previous                                                |
|   	 | Hold	  | Next from selected Collection, Amp, or Cabinet                 |
|   3    | Press  | Exit/Cancel “Edit” mode                                        |
|   	 | Hold	  | Store the changes (TODO) and exit “Edit” mode                  |
|   4    | Press  | TAP Echo/Delay time                                            |
|   	 | Hold	  | Toggle Tabata/Metronome mode (changes not stored)              |
|   5    | Press  | Select Amp Unit                                                |
|   	 | Hold   |                                                                |
|   6    | Press  | Select Compressor Settings. Press again to toggle (on/off)     |
|   	 | Hold   | Select Noise Gate Settings. Press again to toggle (on/off)     |
|   7    | Press  | Select Effect Settings. Press again to toggle (on/off)         |
|        | Hold   | Select Effect (Chorus/Flanger/Phaser/Tremolo)                  |
|   8    | Press  | Select Delay Settings. Press again to toggle (on/off)          |
|        | Hold   | Select Delay (Tape/Digital)                                    |
|   9    | Press  | Select Reverb Settings. Press again to toggle (on/off)         |
|        | Hold   | Select Reverb (Spring/Plate/Room/Hall)                         |


***Tabata/Metronome Mode***

<br> There are 3 variants available: 
-	Metronome: this is the default mode
-	Tabata: a repeating sequence of 2 timed intervals: Practice (workout) and Rest (recovery). Default settings are 45s/15s
-	Metronome in Tabata: this is a tabata in which the metronome is on during the practice period

![THRII USB Pedal](images/metronome.png)

| Button | Action | Function                                          |
|--------|--------|---------------------------------------------------|
|   1    | Press  | Next time signature                               |
|        | Hold   |                                                   |
|   2    | Press  | Previous time signature                           |
|        | Hold   |                                                   |
|   3    | Press  | Toggle “Metronome in Tabata” mode                 |
|        | Hold   |                                                   |
|   4    | Press  | TAP BPM time                                      |
|        | Hold   | Exit Tabata/Metronome mode                        |
|   5    | Press  | BPM - 10 (Practice time - 10 when in Tabata mode) |
|        | Hold   | BPM - 20 (Rest time - 10 when in Tabata mode)     |
|   6    | Press  | BPM - 5 (Practice time - 5 when in Tabata mode)   |
|        | Hold   | BPM - 1 (Rest time - 5 when in Tabata mode)       |
|   7    | Press  | Start/Stop                                        |
|        | Hold   | Toggle (Metronome/Tabata)                         |
|   8    | Press  | BPM + 5 (Practice time + 5 when in Tabata mode)   |
|        | Hold   | BPM + 1 (Rest time + 5 when in Tabata mode)       |
|   9    | Press  | BPM + 10 (Practice time + 10 when in Tabata mode) |
|        | Hold   | BPM + 20 (Rest time + 10 when in Tabata mode)     |

**TODOs**

- Use Family ID and model number: obtain after connection and use in communication
  - In this way, all THRxII flavours should be supported
  - Currently, it is hard-coded to represent THR10II-W
- Cannot switch THRII amp/col without calling createPatch() 
  - Can switch from 'Boom SendSX' Midi software sending hex data. The 'same' data does not switch amp/col when sent from Teensy
- Use Guitar volume for Boost (Solo switch). Currently uses Master+EQ. Make it selectable?
  - Care to be taken when switching presets and the solo switch was on!
- Define useable settings for all parameters/effects and initialize them at startup. I
  - In this case even if nothing is loaded from the amp or preset, effects can be selected and used. Currently, they can be switched but not very usable
- Introduce button mapping (it has been removed from the original code)

**DISCLAIMER**
<br> In addition to the MIT license disclaimer about the software, consider the same disclaimer for hardware suggestions and all information in this project. Similarly to martinzw and BuhJuhWuh, I can not ensure, that sending messages in the suggested way will not damage the THRII. As well I can not guarantee, that the hardware, I describe will not damage your THRII device or it's power supply or your computer.
Finally, I have no connection to Yamaha / Line6. Every knowledge about the data communications protocol was gained by experimenting, try and error and by looking at the data stream. 

