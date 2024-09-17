Date: September 17, 2004

Release notes
=============

**HW Setup**
- Teensy 4.1
- Micro SDCard
- 3.2" TFT with ILI9341 controller
- 9 push buttons for guitar stomp-boxes
- Standard Buzzer with PWM control
- Battery powerbank
- Use PWM to control the TFT backlight brightness via pin 24


**Pedal Board Modes**
- ***Presets mode. Use 5 pedal board buttons to select***
  - The 5 memory banks on THRII, or
  - 5 User presets from a bank (select bank +/- done with other buttons)
  - 5 Factory presets from a bank (select bank +/- done with other buttons). The factory presets are taken from the THR Remote App.
  - 5 presets are selected with one of the 5 buttons of the board. Pressing the same button of a preset again toggles the Solo boost on and off

- ***Manual mode***
  - Individual effects can be selected and switched on and off by using the pedal board buttons.
  - Pressing button 6 toggles the Solo boost on and off
  - Pressing the bank up/down buttons switches to next/previous preset. Convenient when needed to switch effects in a preset and still switch to different presets without the need to select different mode.
  
- ***EDIT mode with stompbox graphics and rotating knobs***
  - Audio volume knob of the amp can be used to change values of every stompbox parameter 
  - Changing parameters from the amp itself is reflected in the Edit mode GUI accordingly
  - Write preset to SDCard. In edit mode, long press on button 3 will write the changes back to the SD card if a user preset has been edited. 
    - In case of factory preset has been edited, then the changes are stored only in RAM. 
    - Writing to one of the 5 THRII memory banks can, of course, be done by a long press of one of the memory buttons on the amp.
    - Short press of button 3 switches back to the previous mode, no writing to SDCard. Settings are retained until another preset is selected.

- ***Metronome/Tabata mode. Using an onboard buzzer to generate the clicks. Controlled with the board buttons. Can set:***
  - Practice and rest times in Tabata mode
  - Time signature (eg 3/4, 4/4, etc.) and tempo (beats per minute)


**Loading patches/presets from SDCard**
- Every preset is in a separate .thrl6p file
  - Read and sort the files based on their names before initializing the presets
  - To have control on the order of loaded presets, the suggested file names format is: 012_file_name.thrl6. Use leading zeroes.
- Split in 2 groups
  - User presets locations: /thrii/user_presets 
  - Factory presets location: /thrii/factory_presets
NOTE: No embedded patches from program memory support (Patches.h)

**Defined default settings for all (effects, echoes, reverbs)**
  - This makes all effects usable even if they were not set by a preset or by the amp


**Improved display layout**
- Enlarged patch name area to better place the patch name on 2 lines.
  - Split patch name in 2 lines if too long. Split at ' ', '-', or '_' characters.
- Changed signal chain
  - Comp-Amp-NSGate-Effect-Echo_Reverb
  - Removed guitar and audio volume due to space limit
- Updated color scheme of the stompboxes to improve visibility
- Using palette to reduce memory usage of sprites in edit mode


In the code
-----------
- Using the actual THRII model name, which is obtained and displayed after connecting to THRII
- Using the actual product ID and model number, which is obtained after connecting to THRII
- Implemented the CRC32 calculation over the symbol table
  - This CRC32 actually is the 'magic' key to activate THRII usb/midi communication


Known Issues
------------
- Reconnecting - If for some reason the board is disconnected and connected again while the amp is on and activated, behaviour is not always as expected. Monitoring the connection and the actions taken need to be revisited.
- If metronome is enabled during the ready counting state in tabata, the effect is 'pause'.
- Cannot set the AMP by sending parameter - instead, a whole patch is created and sent to THRII.
- Solo boost amount requires some fine-tuning
  - Solo boost uses master volume and EQ boost if needed to increase overall volume. 
