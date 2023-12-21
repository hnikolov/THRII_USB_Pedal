 /*THR30II Pedal using Teensy 3.6
* Martin Zwerschke 04/2021
* 
* Heavily modified by Buhjuhwuh
*
* Further modified by hnnikolov
*
* Prerequesites:
*  - Library "USBHost_t3" for Teensy 3.6  
*  - Variant "2_8_Friendly_t3" of Library "ST7789_t3" for Teensy 3.6
*  
*  - Library "Bounce2"   Web: https://github.com/thomasfredericks/Bounce2
*  - Library "SPI" version=1.0  Web: http://www.arduino.cc/en/Reference/SPI  , architectures=sam
*  - Library "SDFAT"   (only if you want to use a SD-card for the patches, can be avoided with Teensy's big PROGMEM)
*  - Library "ArduinoJson"  https://arduinojson.org (Copyright Benoit Blanchon 2014-2021)
*  - Library "Adafruit_gfx.h" https://github.com/adafruit/Adafruit-GFX-Library
*	-Library "ArduinoQueue.h"  https://github.com/EinarArnason/ArduinoQueue
*
* IDE:
*  I use "PlatformIO"  IDE.
*  But genuine Arduino-IDE with additions for Teensy (Teensyduino) should work as well.
*  THR30II_Pedal.cpp
*
* last modified 19/02/2023 Buhjuhwuh
* Author: Martin Zwerschke
*/


#include <Arduino.h>
#include <USBHost_t36.h>  // Without Paul Stoffregen's Teensy3.6-Midi library nothing will work! (Mind increasing rx queue size in MIDI_big_buffer class in "USBHost_t36.h" :   RX_QUEUE_SIZE = 2048)
//#include <SPI.h>    	  // For communication with TFT-Display (SD-Card uses SDIO -SDHC-Interface on Teensy 3.6)
// NOTE: Adafruit_GFX.h include has to be here to avoid compilation error!
#include <Adafruit_GFX.h> // Virtual superclass for drawing (included in "2_8_Friendly_t3.h")
//#include <TFT_eSPI.h>		// TFT screen library
//#include <AceButton.h>	// Button library - debouncing and short/long press distinction
//using namespace ace_button;

//#include <Adafruit_NeoPixel.h>	//Status LED library

//#include <ArduinoJson.h> 	// For patches stored in JSON (.thrl6p) format
//#include <algorithm>  	 	// For the std::find_if function
//#include <vector>			  	// In some cases we will use dynamic arrays - though heap usage is problematic
							  	        // Where ever possibly we use std::array instead to keep values on stack instead
#include "THR30II_Pedal.h"
#include "Globals.h"		 	// For the global keys	
//#include "THR30II.h"   	// Constants for THRII devices, already included in "THR30II_Pedal.h"

// Old includes--------------------------------------------
// #include <ArduinoQueue.h>   //Include in THR30II_pedal.h (for queuing in- and outgoing SysEx-messages) 
// #include <2_8_Friendly_t3.h>  //For 240*320px Color-TFT-Display (important: use special DMA library here, because otherwise it is too slow)
// #include <Bounce2.h>	//Debouncing Library for the foot switch buttons
// #include <EasyButton.h>	//Better debouncing library	
// #include "rammon.h"  //(for development only!) Teensy 3.x RAM Monitor copyright by Adrian Hunt (c) 2015 - 2016
// #include <ST7789_t3.h>
// #include "TFT_Help.h"  		  	//Some local extensions to the ST7789 Library (Martin Zwerschke)
//---------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////////////////
// BUTTONS
/////////////////////////////////////////////////////////////////////////////////////////
#include <AceButton.h>
using namespace ace_button;

const int NUM_BUTTONS = 10;

// Use an array
AceButton buttons[NUM_BUTTONS];
uint8_t pins[NUM_BUTTONS] = {14, 15, 16, 17, 18, 19, 20, 21, 22, 23}; // Buttons range from 1 to 10, therefore, we subtract 13 in the handler

// Forward reference to prevent Arduino compiler becoming confused.
void handleEvent(AceButton*, uint8_t, uint8_t);
void handleButtons(uint8_t);

// Contains 1-10 for pressed, 11-20 for hold buttons 
uint8_t button_state = 0;

/////////////////////////////////////////////////////////////////////////////////////////
// TFT DISPLAY
/////////////////////////////////////////////////////////////////////////////////////////
#include <SPI.h>
#include <TFT_eSPI.h> // Include the graphics library (this includes the sprite functions)
// NOTES: 
// ST7789 controller is selected and SPI is configured in User_Setup.h of TFT_eSPI library src!
//    #define ST7789_2_DRIVER // Minimal configuration option, define additional parameters below for this display
//    #define TFT_WIDTH  240  // ST7789 240 x 240 and 240 x 320
//    #define TFT_HEIGHT 320  // ST7789 240 x 320
//    #define SPI_FREQUENCY  27000000
// ILI9341 controller
//    #define ILI9341_DRIVER  // Generic driver for common displays
//    no width and height specified
//    #define SPI_FREQUENCY  40000000
// SPI pins on Teensy 4.1
//    #define TFT_MISO  12
//    #define TFT_MOSI  11
//    #define TFT_SCK   13
//    #define TFT_DC   9 
//    #define TFT_CS   10  
//    #define TFT_RST  8

TFT_eSPI tft = TFT_eSPI(); // Declare object "tft"
TFT_eSprite spr = TFT_eSprite(&tft); // Declare Sprite object "spr" with pointer to "tft" object


// Normal TRACE/DEBUG
#define TRACE_THR30IIPEDAL(x) x	  // trace on
// #define TRACE_THR30IIPEDAL(x)	// trace off

// Verbose TRACE/DEBUG
#define TRACE_V_THR30IIPEDAL(x) x	 // trace on
// #define TRACE_V_THR30IIPEDAL(x) // trace off


/////////////////////////////////////////////////////////////////////////////////////////
// USB MIDI
/////////////////////////////////////////////////////////////////////////////////////////
USBHost Usb; // On Teensy3.6/4.1 this is the class for the native USB-Host-Interface  
// Note: It is essential to increase rx queue size in "USBHost_t3": RX_QUEUE_SIZE = 2048 should do the job, use 4096 to be safe

MIDIDevice_BigBuffer midi1(Usb);  
// MIDIDevice midi1(Usb); // Should work as well, as long as the RX_QUEUE_SIZE is big enough

ArduinoQueue<Outmessage> outqueue(30); // FIFO Queue for outgoing SysEx-Messages to THRII
// Messages, that have to be sent out,
// and flags to change, if an awaited acknowledge comes in (adressed by their ID)
ArduinoQueue <std::tuple<uint16_t, Outmessage, bool *, bool>> on_ack_queue;
ArduinoQueue<SysExMessage> inqueue(40);

bool complete = false; // Used in cooperation with the "OnSysEx"-handler's parameter "complete" 

String s1("MIDI");
#define MIDI_EVENT_PACKET_SIZE  64 // For THR30II, was much bigger on old THR10
#define OUTQUEUE_TIMEOUT 250       // Timeout if a message is not acknowledged or answered

uint8_t buf[MIDI_EVENT_PACKET_SIZE]; // Receive buffer for incoming (raw) Messages from THR30II to controller
uint8_t currentSysEx[310];           // Buffer for the currently processed incoming SysEx message (maybe coming in spanning over up to 5 consecutive buffers)
						                         // Was needed on Arduino Due. On Teensy3.6 64 could be enough

uint32_t msgcount = 0;

uint16_t cur = 0;     // Current index in actual SysEx
uint16_t cur_len = 0; // Length of actual SysEx if completed

volatile static byte midi_connected = false;

/////////////////////////////////////////////////////////////////////////////////////////
// PATCHES
/////////////////////////////////////////////////////////////////////////////////////////
extern int16_t presel_patch_id;   	 // ID of actually pre-selected patch (absolute number)
extern int16_t active_patch_id;   	 // ID of actually selected patch     (absolute number)

extern uint16_t npatches; // Counts the patches stored on SD-card or in PROGMEN
int8_t nUserPreset = -1; // Used to cycle the THRII User presets

extern std::vector<std::string> patchesII; // patches are read in dynamically, not as a static PROGMEM array

void init_patches_SDCard(); // Forward declaration
void init_patch_names();    // Forward declaration

//////////////////
// FSM
//////////////////
//void fsm_10b_1(UIStates &_uistate, uint8_t &button_state); // Forward declaration
void fsm_9b_1(UIStates &_uistate, uint8_t &button_state); // Forward declaration

/////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
/////////////////////////////////////////////////////////////////////////////////////////
class THR30II_Settings THR_Values;			     // Actual settings of the connected THR30II
class THR30II_Settings stored_THR_Values;    // Stored settings, when applying a patch (to be able to restore them later on)

// Initialize in the beginning with the stored presets in the THRII. Use them to switch between them from the pedal board
// NOTE: We can just send command to activate a stored in the THRII preset. However, still we need to obtain the parameters settings in ordert o update the display
// Current behavior: The first time usere preset is selected from the pedal board, it switched the preset on the THRII and request the settings for the display
//                   Next time, switch to the user preset via command and use the local THR_Values_x data to update the display - it is faster this way
class THR30II_Settings THR_Values_1, THR_Values_2, THR_Values_3, THR_Values_4, THR_Values_5;
std::array< THR30II_Settings, 10 > thr_user_presets = {{ THR_Values_1, THR_Values_2, THR_Values_3, THR_Values_4, THR_Values_5 }};

//extern void updateStatusMask(uint8_t x, uint8_t y, THR30II_Settings &thrs);
extern void updateStatusMask(THR30II_Settings &thrs);

// Family-ID 24, Model-Nr: 1 = THR10II-W
/////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
/////////////////////////////////////////////////////////////////////////////////////////////////
void setup() // Do preparations
{
	// This is the magic trick for printf to support float
  asm(".global _printf_float");  //make printf work for float values on Teensy

	delay(100);
	uint32_t t_out = millis();
	while (!Serial)
	{
    // Timeout, if no serial monitor is used  (make shorter in release version)
		if(millis() - t_out > 3000) { break; }
	} // wait for Arduino Serial Monitor

  // Serial.begin(230400); // Debug via USB-Serial (Teensy's programming interface, where 250000 is maximum speed)
  Serial.begin(250000);
  Serial.println();
  Serial.println("THRxxII Footswitch");

	while( Serial.read() > 0 ) {}; // Make read buffer empty
  
  init_patches_SDCard();

  // ----------------------
  // Initialise the buttons
  // ----------------------
  for( uint8_t i = 0; i < NUM_BUTTONS; i++ )
  {  
    pinMode(pins[i], INPUT_PULLUP);    // Button uses the built-in pull up register
    buttons[i].init(pins[i], HIGH, i); // Initialize the corresponding AceButton
  }

  // Configure the ButtonConfig with the event handler, and enable Click and LongPress events.
  // NOTE: Only one event handler can be specified! Event handler should run fast
  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  //buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setClickDelay(500);	  	// short presses < 500ms (def 200ms)
  buttonConfig->setLongPressDelay(500);	// long  presses > 500ms (def 1000ms)

  // ----------------------------
  // Initialise the TFT registers
  // ----------------------------
  tft.init();
	//tft.begin();
  //tft.setRotation(1); // for the 2" tft (ST7789)
  tft.setRotation(3); // for the 3.2" tft (ILI9341)
  spr.setColorDepth(16); // 16 bit colour needed to show antialiased fonts
  tft.fillScreen(TFT_THRCREAM); // Show a splash screen instead? :)
  //THR_Values.updateStatusMask(0,85); // Show some flash-screen here?

  // -----------------------
	// Initialise patch status
  // -----------------------
	if( npatches > 0 ) { presel_patch_id =  1; } // Preselect the first available patch
	else               { presel_patch_id = -1; } // No preselected patch possible because no available patches

	active_patch_id = presel_patch_id; // Always start up with local settings, however show the patch number of the preselected patch

	TRACE_THR30IIPEDAL(Serial.printf(F("\n\rThere are %d JSON patches in patches.h / patches.txt.\n\r"), npatches);)
	TRACE_THR30IIPEDAL(Serial.println(F("Fetching Library Patch Names:")); )

  init_patch_names();

//	delay(250); // Could be reduced in release version

  // -----------
	// MIDI
  // -----------
  midi1.begin();
  midi1.setHandleSysEx(OnSysEx);
	
//	delay(250); // Could be reduced in release version
} // End of Setup()

/////////////////////////////////////////////////////////////////////////////////////////
// BUTTONS EVENT HANDLER 
/////////////////////////////////////////////////////////////////////////////////////////
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t button_pressed =  button->getPin() - 13; // Buttons logical numbers starts from 1
  switch (eventType) {
    case AceButton::kEventPressed:
      // Only the tap-echo time button reacts on key pressed event
      if( button_pressed == 4 ) { button_state = button_pressed; }
      break;

    // case AceButton::kEventReleased:    
    case AceButton::kEventClicked:
      if( button_pressed != 4 ) { button_state = button_pressed; } // Skip button #4
      break;

    case AceButton::kEventLongPressed:
      button_state = button_pressed + 10; // Buttons logical numbers starts from 1, add 10 for long press
      break;
//    case AceButton::kEventDoubleClicked:
//      button_state = (button->getPin() - 13) + 20; // Buttons logical numbers starts from 1, add 20 for double click
//      break;
  }
}

//////////////////////////////////////////////////////////////////////////////////////
// Global variables, because values must be stored in between two calls of "parse_thr"
//////////////////////////////////////////////////////////////////////////////////////
byte maskUpdate = false;    // Set, if a value changed and the display mask should be updated soon
			                      // (e.g. after init or after turning a knob or restored local settings)

UIStates _uistate = UI_home_amp; // TODO: idle state not needed?

uint32_t tick1 = millis(); // Timer for regular mask update
uint32_t tick2 = millis(); // Timer for background mask update for forgotten values
uint32_t tick3 = millis(); // Timer for switching back to Grid after showing preselected patch
uint32_t tick4 = millis(); // Unused timer for regularly requesting all values 
uint32_t tick5 = millis(); // Unused timer for resetting the other timers 

////////////////////////
// GUI TIMING
////////////////////////
void timing()
{
  if( millis() - tick1 > 40 ) // Mask update if it was required because of changed values
  {
    if( maskUpdate )
    {
//      updateStatusMask(0, 85, THR_Values);
      updateStatusMask(THR_Values);
      maskUpdate = false;
      tick1 = millis(); // Start new waiting period
      return;
    }
	}

	else if( millis() - tick2 > 1000 ) // Force mask update to avoid "forgotten" value changes
	{
    // FIXME: There should not be 'forgotten' values
    if( maskUpdate )
		{
//		  updateStatusMask(0, 85, THR_Values); // Needed probably only because midi_connected is set too soon
		  updateStatusMask(THR_Values); // Needed probably only because midi_connected is set too soon
      maskUpdate = false;
		  tick2 = millis(); // Start new waiting period
		  return;
		}
	}

/*
	if( millis() - tick3 > 1500 ) // Switch back to mask or selected patchname after showing preselected patchname for a while
	{		
		if( preNameActive ) // NOT implemented yet
		{
			preNameActive = false; // Reset showing pre-selected name
			
			// Patch is not active => local Settings mask should be activated again
			// drawStatusMask(0,85); // TODO: Check is we need this
			maskUpdate = true;
			
			// tick3 = millis(); // Start new waiting not necessary, because it is a "1-shot"-Timer
			return;	
		}
	}
*/
//	 if(millis()-tick4>3500)  //force to get all actual settings by dump request
//	 {
//	    if(maskUpdate)
//		{
//		 //send_dump_request();  //not used - THR-Remote does not do this as well
//		 tick4=millis();  //start new waiting
//		 return;
//		}
//	 }
//
//	 if(millis()-tick5>15000)
//	 {
//		//TRACE_THR30IIPEDAL( Serial.println("resetting all timers (15s-timeout)"); )
//		// tick5=millis();
//		// tick4=millis();
//	 	// tick3=millis();
//		// tick2=millis();
//		// tick1=millis();
//	 }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP
/////////////////////////////////////////////////////////////////////////////////////////////////
void loop() // Infinite working loop:  check midi connection status, poll buttons & pedal inputs, run state machine
{
  if( midi1 ) // Is a device connected?
	{
		midi1.read(); // Teensy 3.6 Midi-Library needs this to be called regularly
			// Hardware IDs of USB-Device (should be VendorID = 0499, ProductID = 7183) Version = 0100
			// sprintf((char*)buf, "VID:%04X, PID:%04X", Midi.vid, Midi.pid);  
			// Serial.println((char*)buf);	

		if( complete && midi1.getSysExArrayLength() >= 0 )
		{
			// printf("\r\nMidi Rec. res. %0d.\t %1d By rec.\r\n",midi1.getSysExArrayLength(),(int)received);
			Serial.println("Main loop:" + String(midi1.getSysExArrayLength()) + String(" Bytes received"));
			// Serial.println(String(cur_len)+String(" Bytes received."));
	
			inqueue.enqueue(SysExMessage(currentSysEx, cur_len)); // Feed the queue of incoming messages
			
			complete = false;  
			
		} // of if(complete && ...
		else  
		{
			if( !midi_connected )
			{
				drawConnIcon(midi_connected);
				
				Serial.println(F("\r\nSending Midi-Interface Activation..\r\n")); 
				send_init();  // Send SysEx to activate THR30II-Midi interface

				// ToDo: only set true, if SysEx's were acknowledged
				midi_connected = true;  // A Midi Device is connected now. Proof later, if it is a THR30II
        
				drawConnIcon(midi_connected);
				drawPatchName(TFT_SKYBLUE, "INITIALIZING");

				maskUpdate = true; // Tell GUI to update settings mask one time because of changed settings		
			}
		} // of MIDI not connected so far
	} // of if( midi1 )  means: a device is connected
  else if( midi_connected )  // no device is connected (any more) but MIDI was connected before => connection got lost 
  {
    // Display "MIDI" icon in RED (because connection was lost)
	 	midi_connected = false;  // This will lead to Re-Send-Out of activation SysEx's
    // TODO: All this mechanism of re-connecting needs a review, WiP
		THR_Values.ConnectedModel = 0x00000000; // TODO: This will lead to show "NOT CONNECTED"
    _uistate = UI_home_amp;                 //
    THR_Values.thrSettings = true;          //
		drawConnIcon(midi_connected); // Not part of THR_Values.updateStatusMask(0, 85); 
    //THR_Values.updateStatusMask(0, 85); // FIXME: Update only to show "NOT CONNECTED"
    maskUpdate = true; // Tell GUI to update settings mask one time because of changed settings
  } // of: if(midi_connected)
		
	timing(); // GUI timing must be called regularly enough. Locate the call in the working loop	

	WorkingTimer_Tick(); // Timing for MIDI-message send/receive is shortest loop

  // Should be called every 4-5ms or faster, for the default debouncing time of ~20ms.
  for(uint8_t i = 0; i < NUM_BUTTONS; i++) { buttons[i].check(); }

//  fsm_10b_1(_uistate, button_state);
  fsm_9b_1(_uistate, button_state);

} // End of loop()

/////////////////////////////////////////////////////////////
void send_init()  //Try to activate THR30II MIDI 
{    
	//For debugging on old Arduino Due version: print USB-Endpoint information 
	// for(int i=0; i<5;i++)
	// {
	//  printf("\nEndpoint Index %0d: Dev.EP#%1lu, Pipe#%2lu, PktSize:%3lu, Direction:%4d\r\n"
	//  ,i, Midi.epInfo[i].deviceEpNum,Midi.epInfo[i].hostPipeNum,Midi.epInfo[i].maxPktSize,(byte)Midi.epInfo[i].direction );
	// }
	
	while(outqueue.item_count()>0) outqueue.dequeue(); //clear Queue (in case some message got stuck)
	
	Serial.println(F("\r\nOutque cleared.\r\n"));
	// PC to THR30II Message:
	//F0 7E 7F 06 01 F7  (First Thing PC sends to THR using "THR Remote" (prepares unlocking MIDI!) )
	//#S1
	//--------------------------------------------  Put outgoing  messages into the queue 
	//Universal Identity request
	outqueue.enqueue( Outmessage(SysExMessage( (const byte[6]) { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}, 6 ), 1, true, true) ); // Acknowledge means receiving 1st reply to Univ. Discovery, Answerded is receiving 2nd. Answer 
	
	// THR30II answers:         F0 7E 7F 06 02 00 01 0C 24 00 02 00 63 00 1E 01 F7    (1.30.0c)  (63 = 'c')
	// After Firmware-Update:   F0 7E 7F 06 02 00 01 0C 24 00 02 00 6B 00 1F 01 F7    (1.31.0k)  (6B = 'k')
	// After Firmware-Update:   F0 7E 7F 06 02 00 01 0C 24 00 02 00 61 00 28 01 F7    (1.40.0a)  (61 = 'a')
	TRACE_V_THR30IIPEDAL( Serial.println(F("Enqued Inquiry.\r\n")));

	// and afterwards a message containing Versions
	// like "L6ImageType:mainL6ImageVersion:1.3.0.0.c"

	/*  f0 00 01 0c 24 02 7e 7f 06 02 4c 36 49 6d 61 67 65 54 79 70 65 3a 6d 61 69 6e 00 4c 36 49 6d 61
		67 65 56 65 72 73 69 6f 6e 3a 31 2e 33 2e 30 2e 30 2e 63 00 f7
		(L6ImageType:mainL6ImageVersion:1.3.0.0.c)
	*/
	
	//#S2 invokes #R3 (ask Firmware-Version) For activating MIDI only necessary to get the right magic key for #S3/#S4  (not needed, if key is known)            
    outqueue.enqueue( Outmessage( SysExMessage( (const byte[29]) { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 },29), 2, false, true)); //answer will be the firmware version);
    //outqueue.enqueue( Outmessage( SysExMessage( (const byte[29]) { 0xf0, 0x00, 0x01, 0x0c, 0x22, 0x02, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 },29), 2, false, true)); //answer will be the firmware version);
	TRACE_V_THR30IIPEDAL( Serial.println(F("Enqued Ask Firmware-Version.\r\n")));

    //#S3 + #S4 will follow, when answer to "ask Firmware-Version" comes in 
	//#S3 + #S4  will invoke #R4 (seems always the same) "MIDI activate" (works only after ID_Req!)														   

}//end of send_init

void do_gain_boost()
{
  do_volume_patch();
	THR_Values.boost_activated = true;
}

void undo_gain_boost()
{
  undo_volume_patch();
	THR_Values.boost_activated = false;
}

bool g_vol = false;

void do_volume_patch() // Increases Volume and/or Tone settings for SOLO to be louder than actual patch
{
  g_vol = false;
	std::copy(THR_Values.control.begin(), THR_Values.control.end(), THR_Values.control_store.begin());  // Save volume and tone related settings

	if( THR_Values.guitar_volume < 100 / 1.333333 ) // Is there enough headroom to increase Master Volume by 33%?
	{
		THR_Values.sendChangestoTHR = true;
		THR_Values.SetGuitarVol(THR_Values.guitar_volume * 1.333333); // Do it
		THR_Values.sendChangestoTHR = false;
    g_vol = true;
	}
  else if( THR_Values.GetControl(CTRL_MASTER) < 100 / 1.333333 ) // Is there enough headroom to increase Master Volume by 33%?
	{
		THR_Values.sendChangestoTHR = true;
		THR_Values.SetControl(CTRL_MASTER, THR_Values.GetControl(CTRL_MASTER) * 1.333333); // Do it
		THR_Values.sendChangestoTHR = false;
	}	
  else // Try to increase volume by simultaneously increasing MID/TREBLE/BASS
	{
		THR_Values.sendChangestoTHR = true;
		double margin = (100.0 - THR_Values.GetControl(CTRL_MASTER)) / THR_Values.GetControl(CTRL_MASTER); // Maximum MASTER multiplier? (e.g. 0.17)
		THR_Values.SetControl(CTRL_MASTER, 100); // Use maximum MASTER
		double max_tone = std::max(std::max(THR_Values.GetControl(CTRL_BASS), THR_Values.GetControl(CTRL_MID)), THR_Values.GetControl(CTRL_TREBLE)); // Highest value of the tone settings 
		double tone_margin = (100.0 - max_tone) / max_tone; // Maximum equal TONE-Settings multiplier? (e.g. 0.28)
		
    if( tone_margin > 0.333333 - margin ) // Can we increase the TONE settings simultaneously to reach the missing MASTER-increasement?
		{
			THR_Values.SetControl(CTRL_BASS,   THR_Values.GetControl(CTRL_BASS)   * (1+0.333333 - margin));
			THR_Values.SetControl(CTRL_MID,    THR_Values.GetControl(CTRL_MID)    * (1+0.333333 - margin));
			THR_Values.SetControl(CTRL_TREBLE, THR_Values.GetControl(CTRL_TREBLE) * (1+0.333333 - margin));
		} 
		else // Increase as much as simultaneously possible (one tone setting reaches 100% this way)
		{
			THR_Values.SetControl(CTRL_BASS,   THR_Values.GetControl(CTRL_BASS)   * (1 + tone_margin));
			THR_Values.SetControl(CTRL_MID,    THR_Values.GetControl(CTRL_MID)    * (1 + tone_margin));
			THR_Values.SetControl(CTRL_TREBLE, THR_Values.GetControl(CTRL_TREBLE) * (1 + tone_margin));
		}
		THR_Values.sendChangestoTHR = false;
	}
}

void undo_volume_patch()
{
	THR_Values.sendChangestoTHR = true;
	// Restore volume related settings
  if( g_vol == true )
  {
    THR_Values.SetGuitarVol(THR_Values.guitar_volume / 1.333333);
  }
  else
  {
    THR_Values.SetControl( CTRL_MASTER, THR_Values.control_store[CTRL_MASTER] );
    THR_Values.SetControl( CTRL_BASS,   THR_Values.control_store[CTRL_BASS] );
    THR_Values.SetControl( CTRL_MID,    THR_Values.control_store[CTRL_MID] );
    THR_Values.SetControl( CTRL_TREBLE, THR_Values.control_store[CTRL_TREBLE] );    
  }
	THR_Values.sendChangestoTHR = false;
}

//////////////////////////////////////////////
// Manage patches activation
//////////////////////////////////////////////
/*
void patch_deactivate()
{
	// Restore local settings
	THR_Values = stored_THR_Values; // THR30II_Settings class is deep copyable!
	TRACE_THR30IIPEDAL(Serial.println(F("Patch_deactivate(): Restored local settings mirror..."));)
	
  // userSettingsHaveChanged is set to false in createPatch(). We need to keep the state in case of deactivate()
  bool isChanged = THR_Values.getUserSettingsHaveChanged();

	// Activate local settings in THRxxII again (sets userSettingsHaveChanged to false)
	THR_Values.createPatch();  // Create a patch from all the local settings and send it completely via MIDI-patch-upload
  
  // Set actual patch status
  THR_Values.setUserSettingsHaveChanged(isChanged);

	TRACE_THR30IIPEDAL(Serial.println(F("Patch_deactivate(): Local settings activated on THRII."));)
				
	// Sending/activating local settings means returning to the initial actual settings
	_uistate = UI_home_amp;
}
*/
void patch_activate(uint16_t pnr) // Check patchnumber and send patch as a SysEx message to THR30II
{
	if( pnr <= npatches ) // This patch-id is available
	{
		// Activate the patch (overwrite either the actual patch or the local settings)
		TRACE_THR30IIPEDAL(Serial.printf("Patch_activate(): Activating patch #%d \n\r", pnr);)
		send_patch( pnr ); // Now send this patch as a SysEx message to THR30II 
		active_patch_id = pnr;
		THR_Values.boost_activated = false;
		_uistate = UI_home_patch; // State "patch" reached
	} 
	else
	{
		TRACE_THR30IIPEDAL(Serial.printf("Patch_activate(): invalid pnr ");)
	}
}

void send_patch(uint8_t patch_id) // Send a patch from preset library to THRxxII
{ 
	DynamicJsonDocument djd(4096); // Contains accessible values from a JSON File (2048?)
	
	DeserializationError dse = deserializeJson(djd, patchesII[patch_id-1]);	// patchesII is zero-indexed
	
	if( dse == DeserializationError::Ok )
	{
		TRACE_THR30IIPEDAL(Serial.print(F("Send_patch(): Success-Deserializing. We fetched patch: "));)
		TRACE_THR30IIPEDAL(Serial.println( djd["data"]["meta"]["name"].as<const char*>() );)
		// Here we must copy values to local THRII-Settings and/or send them to THRII
		THR_Values.SetLoadedPatch(djd); // Set all local fields from thrl6p-JSON-File 
	}
	else
	{
		 THR_Values.SetPatchName("error", -1); // -1 = actual settings
		 TRACE_THR30IIPEDAL(Serial.println(F("Send_patch(): Error-Deserial"));)
	}
} // End of send_patch()


void WorkingTimer_Tick() // Latest martinzw version + BJW debug msgs
{
	// Care about queued incoming messages
	if (inqueue.item_count() > 0)
	{
		SysExMessage msg (inqueue.dequeue());
    // CHECK: If ParseSysEx() is called without printing the result, some timeout occurs, Are we too quick?
		// THR_Values.ParseSysEx(msg.getData(),msg.getSize());
		Serial.println("Work. Tmr: " + THR_Values.ParseSysEx(msg.getData(), msg.getSize()));
    if( THR_Values.userPresetDownloaded )
    {
      THR_Values.userPresetDownloaded = false;
      THR_Values.boost_activated = false;
      int idx = THR_Values.getActiveUserSetting();
      Serial.println("Work. Tmr: User preset #" + String(idx) + " " + THR_Values.getPatchName() + " to be copied");
      if( idx >= 0 && idx <= 5 )
      {
        // Store latest user preset settings localy
        thr_user_presets[idx] = THR_Values; // THR30II_Settings class is deep copyable
        nUserPreset = idx;
        Serial.println("Done");
      }
    }

		maskUpdate = true; // Tell GUI to update mask one time because of changed settings
	}		

	// Care about next queued outgoing SysEx, if at least one is pending
	if( outqueue.item_count() > 0 )   
	{
		// Serial.println("outqueue item count = " + String(outqueue.item_count()));
		Outmessage *msg = outqueue.getHeadPtr(); // Point to first message in queue
		
		if( !msg->_sent_out ) // Not sent out yet? ==> send it out
		{  
			midi1.sendSysEx(msg->_msg.getSize(), (uint8_t *)(msg->_msg.getData()), true);
			
			Serial.println("msg #" + String((msg->_id)) + " sent out");
			msg->_sent_out = true;
			msg->_time_stamp = millis();	
			msgcount += 1;
							
		} // of not sent out
		else if( !msg->_needs_ack && !msg->_needs_answer ) // Needs no ACK and no Answer? ==> done with this message
		{
			outqueue.dequeue();
			Serial.println("msg #" + String((msg->_id)) + " OK: dequ no ack, no answ, t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount)); 
		}
		else // Sent out and needs ack or answer ==> check it
		{
			if( msg->_needs_ack )              
			{
				if( msg->_acknowledged )
				{
					if( !msg->_needs_answer ) // Ack OK, no answer needed ==> done with this message
					{
						outqueue.dequeue(); // => ready
						Serial.println("msg #" + String((msg->_id)) + " OK: dequ ack, no answ, t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));						
					}
					else if( msg->_answered ) // Ack OK, Answer received
					{
						outqueue.dequeue();  // => ready
					  Serial.println("msg #" + String((msg->_id)) + " OK: dequ ack and answ, t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));						
					}
				}
				else if( millis()-msg->_time_stamp > OUTQUEUE_TIMEOUT )	// if msg not acknowledged, check for timeout.
																	                             	// if timed out, discard.  (Or re-send...?)
				{
					outqueue.dequeue();  // => ready
					Serial.println("Timeout waiting for acknowledge. Discarded Message #" + String((msg->_id)) + " t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));

					// try re-sending msg instead of discarding
					// midi1.sendSysEx(msg->_msg.getSize(),(uint8_t *)(msg->_msg.getData()),true);
					// Serial.println("Timeout waiting for acknowledge. Msg #" + String((msg->_id)) + " re-sent out. t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));
					// msg->_sent_out = true;
					// msg->_time_stamp=millis();	
					// msgcount += 1;
				}
				
			}
			else if( msg->_needs_answer ) // Only need Answer, no Ack
			{
				if (msg->_answered) // Answer received
				{
					outqueue.dequeue(); // => ready
					Serial.println("msg #" + String((msg->_id)) + " OK: dequ no ack but answ, t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));
				}
				else if( millis()-msg->_time_stamp > OUTQUEUE_TIMEOUT )
				{
					outqueue.dequeue(); // => ready
					Serial.println("Timeout waiting for answer. Discarded Message #" + String((msg->_id)) + " t=" + String(millis()-msg->_time_stamp) + " n=" + String(msgcount));
				}
			}
		}
	}
}

// TODO: Strip down this class to the minimum. Do we need all the copy/move constructors, and operators?
SysExMessage::SysExMessage(): Data(nullptr), Size(0) // Standard constructor
{
}

SysExMessage::~SysExMessage() // Destructor
{
	delete[] Data;
	Size=0;
}

SysExMessage::SysExMessage(const byte *data, size_t size): Size(size) // Constructor
{
	Data = new byte[size];
	memcpy(Data, data, size);
}
SysExMessage::SysExMessage(const SysExMessage &other): Size(other.Size)  // Copy Constructor
{
	Data = new byte[other.Size];
	memcpy(Data, other.Data, other.Size);
}
// FIXME: Compilation error: expected constructor, destructor, or type conversion before '(' token
//SysExMessage::SysExMessage( SysExMessage &&other ) noexcept : Data(other.Data), Size(other.Size) //Move Constructor
SysExMessage::SysExMessage( SysExMessage &&other ): Data(other.Data), Size(other.Size) // Move Constructor
{
	other.Data = nullptr;
	other.Size = 0;
}

SysExMessage & SysExMessage::operator=( const SysExMessage & other ) // Copy-assignment
{
	if(&other==this) return *this;
	delete[] Data;
	Data=new byte[other.Size];
	memcpy(Data,other.Data,Size=other.Size);	   
	return *this;
}
SysExMessage & SysExMessage::operator=( SysExMessage && other ) noexcept // Move-assignment
{
	if(&other==this) return *this;
	delete[] Data;
	Size=other.Size;
	Data=other.Data;
	other.Size=0;
	return *this;	   
} 

const byte * SysExMessage::getData() // Getter for the const byte-array
{
	return (const byte*) Data;
}

size_t SysExMessage::getSize() // Getter for the const byte-array
{
	return Size;
}

/////////////////////////////////////////////////////////////////////////////

void OnSysEx(const uint8_t *data, uint16_t length, bool complete)
{
	// Serial.println("SysEx Received " + String(length));
	// complete ? Serial.println("- completed ") : Serial.println("- continued ");
	
  memcpy(currentSysEx + cur, data, length);
	cur += length;

	if( complete )
	{
		cur_len = cur + 1;
		cur = 0;
		::complete = true;
	}
	else
	{		
	}
}
