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

#include <ArduinoJson.h> 	// For patches stored in JSON (.thrl6p) format
#include <algorithm>  	 	// For the std::find_if function
#include <vector>			  	// In some cases we will use dynamic arrays - though heap usage is problematic
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
// NOTE: ST7789 controller is selected and SPI is configured in User_Setup.h of TFT_eSPI library src!
TFT_eSPI tft = TFT_eSPI(); // Declare object "tft"
TFT_eSprite spr = TFT_eSprite(&tft); // Declare Sprite object "spr" with pointer to "tft" object

// Include font definition file (belongs to Adafruit GFX Library )
//#include <Fonts/FreeSansBold18pt7b.h>
//#include <Fonts/FreeSansOblique24pt7b.h>
//#include <Fonts/FreeSansOblique18pt7b.h>
//#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMono9pt7b.h>
//#include <Fonts/FreeSans9pt7b.h>

// Locally supplied fonts
// #fonts/include "Free_Fonts.h"
#include "fonts/NotoSansBold15.h"
#include "fonts/NotoSansBold36.h"
#include "fonts/NotoSansMonoSCB20.h"
#include "fonts/Latin_Hiragana_24.h"
//#include "fonts/Final_Frontier_28.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
//#define AA_FONT_LARGE NotoSansBold36
#define AA_FONT_LARGE Latin_Hiragana_24
#define AA_FONT_MONO  NotoSansMonoSCB20 // NotoSansMono-SemiCondensedBold 20pt

// Set USE_SDCARD to 1 to use SD card reader to read from patch a patch txt file.
// If USE_SDCARD is set to 0 then the file patches.h is included which needs to 
// define a PROGMEM variable named patches.
// NOTE: Compiler errors when using PROGMEM

#define USE_SDCARD 1
// #define USE_SDCARD 0
#if USE_SDCARD
#include <SD.h>
const int sd_chipsel = BUILTIN_SDCARD;
File32 file;

#define PATCH_FILE "patches.txt" // If you want to use a SD card, a file with this name contains the patches 
#else
// #include <avr/pgmspace.h>
#include "patches.h"  // Patches located in PROGMEM  (for Teensy 3.6/4.0/4.1 there is enough space there for hundreds of patches)
#endif

// Normal TRACE/DEBUG
#define TRACE_THR30IIPEDAL(x) x	  // trace on
// #define TRACE_THR30IIPEDAL(x)	// trace off

// Verbose TRACE/DEBUG
#define TRACE_V_THR30IIPEDAL(x) x	 // trace on
// #define TRACE_V_THR30IIPEDAL(x) // trace off

// Define other input/output pins ///// TO BE REMOVED //////////
//#define LED_PIN       		12
//#define PEDAL_1_PIN       	24
//#define PEDAL_2_PIN       	25
//#define PEDAL_1_SENSE_PIN 	26
//#define PEDAL_2_SENSE_PIN 	27

//const uint8_t pedal_buf_size = 10;
//uint16_t pedal_1_buf[pedal_buf_size];
//uint16_t pedal_2_buf[pedal_buf_size];
//uint16_t pedal_1_sum = 0;
//uint16_t pedal_2_sum = 0;
uint16_t pedal_1_val = 0; // TODO Used to represent Guitar Volume
uint16_t pedal_2_val = 0; // TODO Used to represent Audio Volume
//uint16_t pedal_1_old_val = 0;
//uint16_t pedal_2_old_val = 0;
//uint16_t pedal_margin = 1;
//bool pedal_1_sense = 0;
//bool pedal_2_sense = 0;
////////////////////////////////////////////////

// Used in the FSM
enum ampSelectModes {COL, AMP, CAB};
ampSelectModes amp_select_mode = COL;
enum dynModes {Boost, Comp, Gate};
dynModes dyn_mode = Comp;

bool boost_activated = 0;
double scaledvolume = 0; // TODO: Can it be a local variable?

USBHost Usb; // On Teensy3.6/4.1 this is the class for the native USB-Host-Interface  
// Note: It is essential to increase rx queue size in "USBHost_t3": RX_QUEUE_SIZE = 2048 should do the job, use 4096 to be safe

MIDIDevice_BigBuffer midi1(Usb);  
// MIDIDevice midi1(Usb); // Should work as well, as long as the RX_QUEUE_SIZE is big enough

bool complete = false; // Used in cooperation with the "OnSysEx"-handler's parameter "complete" 

// Variables and constants for display 
//int16_t x=0, y=0, xx1=0, yy1=0;
String s1("MIDI");
#define MIDI_EVENT_PACKET_SIZE  64 // For THR30II, was much bigger on old THR10
#define OUTQUEUE_TIMEOUT 250       // Timeout if a message is not acknowledged or answered

uint8_t buf[MIDI_EVENT_PACKET_SIZE]; // Receive buffer for incoming (raw) Messages from THR30II to controller
uint8_t currentSysEx[310];           // Buffer for the currently processed incoming SysEx message (maybe coming in spanning over up to 5 consecutive buffers)
						                         // Was needed on Arduino Due. On Teensy3.6 64 could be enough

String patchname;  // Limitation by Windows THR30 Remote is 64 Bytes.
String preSelName; // Global variable: Name of the pre selected patch

class THR30II_Settings THR_Values;			     // Actual settings of the connected THR30II
class THR30II_Settings stored_THR_Values;    // Stored settings, when applying a patch (to be able to restore them later on)
//class THR30II_Settings stored_Patch_Values;  // Stored settings of a (modified) patch, when applying a solo (to be able to restore them later on)

// Initialize in the beginning with the stored presets in the THRII. Use them to switch between them from the pedal board
// TODO: Can we just send command to activate a stored in the THRII preset?
class THR30II_Settings THR_Values_1, THR_Values_2, THR_Values_3, THR_Values_4, THR_Values_5;
std::array< THR30II_Settings, 5 > thr_user_presets = {{ THR_Values_1, THR_Values_2, THR_Values_3, THR_Values_4, THR_Values_5 }};

// TODO: Why static volatile?
static volatile int16_t presel_patch_id;   	 // ID of actually pre-selected patch (absolute number)
static volatile int16_t active_patch_id;   	 // ID of actually selected patch     (absolute number)
static volatile bool send_patch_now = false; // pre-select patch to send (false) or send immediately (true)

static std::vector <String> libraryPatchNames; // All the names of the patches stored on SD-card or in PROGMEN

static volatile uint16_t npatches = 0; // Counts the patches stored on SD-card or in PROGMEN

int8_t nUserPreset = -1; // Used to cycle the THRII User presets
bool isUsrPreset = false;

#if USE_SDCARD
	std::vector<std::string> patchesII; // patches are read in dynamically, not as a static PROGMEM array
#else
	                                    // patchesII is declared in "patches.h" in this case
#endif

uint32_t msgcount = 0;

// Family-ID 24, Model-Nr: 1 = THR10II
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

  // Serial.begin(230400);  // Debug via USB-Serial (Teensy's programming interface, where 250000 is maximum speed)
  Serial.begin(250000);
  Serial.println();
  Serial.println("THR30II Footswitch");

	while( Serial.read() > 0 ) {}; // Make read buffer empty
    
  // -------------
  // SD-card setup
  // -------------
 	#if USE_SDCARD
	if (!SD.begin(sd_chipsel))
	{
    Serial.println(F("Card failed, or not present."));
  }
  else
	{
		Serial.println(F("Card initialized."));
		
    TRACE_THR30IIPEDAL( Serial.print(F("VolumeBegin->success. FAT-Type: "));)

			if(file.open(PATCH_FILE))  //Open the patch file "patches.txt" in ReadOnly mode
			{
				while(file.isBusy());
				uint64_t inBrack=0;  //actual bracket level
				uint64_t blockBeg=0, blockEnd=0;  //start and end point of actual patch inside the file
				bool error=false;
				
				Serial.println(F("\n\rFound \"patches.txt\"."));
				file.rewind();

				char c=file.peek();
				
				while(!error && file.available32()>0)
				{ 
					// TRACE_THR30IIPEDAL(Serial.printf("%c",c);)
				
					switch(c)
					{
						case '{':
							if(inBrack==0)  //top level {} block opens here
							{
								blockBeg = file.curPosition();
								TRACE_THR30IIPEDAL(Serial.printf("Found { at %d\n\r",blockBeg);)
							}
							inBrack++;
							// TRACE_THR30IIPEDAL(Serial.printf("+%d\n\r",inBrack);)
						break;

						case '}':
							if(inBrack==1)  //actually open top level {} block closes here
							{	
								blockEnd = file.curPosition();
								
								TRACE_THR30IIPEDAL(Serial.printf("Found } at %d\n\r",blockEnd);)
								
								error=!(file.seekSet(blockBeg-1));
								
								if(!error)
								{
									String pat = file.readString(blockEnd-blockBeg+1);

									if(file.getReadError()==0)
									{
										patchesII.emplace_back(std::string(pat.c_str()));
										//TRACE_V_THR30IIPEDAL(Serial.println(patchesII.back().c_str()) ;)
										npatches++;
										TRACE_V_THR30IIPEDAL(Serial.printf("Loaded npatches: %d",npatches) ;)
									}
									else
									{
										TRACE_THR30IIPEDAL(Serial.println("Read Error");)
										error=true;
									}
								}
								else
								{
									TRACE_THR30IIPEDAL( Serial.println("SeekSet(blockBeg-1) failed!" );)
								}

								error=!(file.seekSet(blockEnd));
								
								if(error)
								{
									TRACE_THR30IIPEDAL( Serial.println("SeekSet(blockEnd) failed!" );)	
								}
								else
								{
									TRACE_THR30IIPEDAL( Serial.println("SeekSet(blockEnd) succeeded." );)	
								}
							}
							else if(inBrack==0)  //block must not close, if not open!
							{
								TRACE_THR30IIPEDAL(Serial.printf("Unexpected } at %d\n\r",file.curPosition());)
								error=true; 	
								break;
							}
							inBrack--;
							// TRACE_THR30IIPEDAL(Serial.printf("-%d\n\r",inBrack);)
						break;
					}			

					if(!error)
					{
						if(file.available32()>0)
						{
							c=(char) file.read();
							while(file.isBusy());
						}
					}
					else
					{
						TRACE_THR30IIPEDAL( Serial.println("Error proceeding in file!" );)	
					}
					//TRACE_THR30IIPEDAL(Serial.println(inBrack);)

				}

				TRACE_THR30IIPEDAL(Serial.println("Read through \"patches.txt\".\n\r");)
				
				file.close();
			}
			else
			{
				Serial.println(F("File \"patches.txt\" was not found on SD-card."));
			}
	}
	#else
		npatches = patchesII.size();  //from file patches.h 
		TRACE_V_THR30IIPEDAL(Serial.printf("From PROGMEM: npatches: %d",npatches) ;)
	#endif	

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
  tft.setRotation(1);
  spr.setColorDepth(16); // 16 bit colour needed to show antialiased fonts
  tft.fillScreen(TFT_THRCREAM);
  //THR_Values.updateStatusMask(0,85); // Show some flash-screen here?

  // -----------------------
	// Initialise patch status
  // -----------------------
	if( npatches > 0 ) { presel_patch_id =  1; } // Preselect the first available patch
	else               { presel_patch_id = -1; } // No preselected patch possible because no available patches

	active_patch_id = presel_patch_id; // Always start up with local settings, however show the patch number of the preselected patch

	TRACE_THR30IIPEDAL(Serial.printf(F("\n\rThere are %d JSON patches in patches.h / patches.txt.\n\r"), npatches);)
	TRACE_THR30IIPEDAL(Serial.println(F("Fetching Library Patch Names:")); )

	DynamicJsonDocument djd (4096); // Buffer size for JSON decoding of thrl6p-files is at least 1967 (ArduinoJson Assistant) 2048 recommended
	
	for(int i = 0; i < npatches; i++ ) // Get the patch names by de-serializing
	{
    using namespace std;
    djd.clear();
    DeserializationError dse= deserializeJson(djd, patchesII[i]);
    if(dse==DeserializationError::Ok)
    {
      libraryPatchNames.push_back((const char*) (djd["data"]["meta"]["name"]));
      TRACE_THR30IIPEDAL(Serial.println((const char*) (djd["data"]["meta"]["name"]));)
    }
    else
    {
      Serial.print(F("deserializeJson() failed with code "));Serial.println(dse.f_str());
      libraryPatchNames.push_back("error");
      TRACE_THR30IIPEDAL(Serial.println("JSON-Deserialization error. PatchesII[i] contains:");)
      Serial.println(patchesII[i].c_str());
    }
	}

	delay(250); // Could be reduced in release version

  midi1.begin();
  midi1.setHandleSysEx(OnSysEx);
	
	delay(250); // Could be reduced in release version
} // End of Setup()

/////////////////////////////////////////////////////////////////////////////////////////
// BUTTONS EVENT HANDLER 
/////////////////////////////////////////////////////////////////////////////////////////
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t button_pressed =  button->getPin() - 13; // Buttons logical numbers starts from 1
  switch (eventType) {
    case AceButton::kEventPressed:
      // Only the tap-echo time button reacts on key pressed event
      if( button_pressed == 3 ) { button_state = button_pressed; }
      break;

    //case AceButton::kEventReleased:    
    case AceButton::kEventClicked:
      if( button_pressed != 3 ) { button_state = button_pressed; } // Skip button #3
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
uint16_t cur = 0;     // Current index in actual SysEx
uint16_t cur_len = 0; // Length of actual SysEx if completed

volatile static byte midi_connected = false;

static byte maskUpdate = false;    // Set, if a value changed and the display mask should be updated soon
static byte maskActive = false;    // Set, if local THR-settings or modified patch settings are valid
							                     // (e.g. after init or after turning a knob or restored local settings)
static byte preNameActive = false; // Pre-selected name is shown, not settings mask nor active patchname (NOT implemented yet?)

UIStates _uistate = UI_idle; // Always begin with idle state until actual settings are fetched

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
    if( maskActive && maskUpdate )
    {
      THR_Values.updateStatusMask(0, 85);
      tick1 = millis(); // Start new waiting period
      return;
    }
	}

	if( millis() - tick2 > 1000 ) // Force mask update to avoid "forgotten" value changes
	{
	  if( maskActive )
		{
		// THR_Values.updateConnectedBanner();
		  THR_Values.updateStatusMask(0,85);
		  tick2 = millis(); // Start new waiting period
		  return;
		}
	}

	if( millis() - tick3 > 1500 ) // Switch back to mask or selected patchname after showing preselected patchname for a while
	{		
		if( preNameActive ) // NOT implemented yet
		{
			preNameActive = false; // Reset showing pre-selected name
			
			// Patch is not active => local Settings mask should be activated again
			maskActive = true;
			// drawStatusMask(0,85); // TODO: Check is we need this
			maskUpdate = true;
			
			// tick3 = millis(); // Start new waiting not necessary, because it is a "1-shot"-Timer
			return;	
		}
	}

//	 if(millis()-tick4>3500)  //force to get all actual settings by dump request
//	 {
//	    if(maskActive)
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

// elapsedMillis tickpedals;

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

    // Should be called every 4-5ms or faster, for the default debouncing time of ~20ms.
    //for (uint8_t i = 0; i < NUM_BUTTONS; i++) { buttons[i].check(); } // TODO: Do we need it here as well?

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

				maskActive=true;  // Tell GUI that it must show the settings mask
				// drawStatusMask(0,85); //y-position results from height of the bar-diagram, that is drawn bound to lowest display line
				maskUpdate=true;  // Tell GUI to update settings mask one time because of changed settings		
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
		drawConnIcon(midi_connected);
  } // of: if(midi_connected)
		
	timing(); // GUI timing must be called regularly enough. Locate the call in the working loop	

  // Should be called every 4-5ms or faster, for the default debouncing time of ~20ms.
  //for (uint8_t i = 0; i < NUM_BUTTONS; i++) { buttons[i].check(); } // TODO: Do we need it here as well?

	WorkingTimer_Tick(); // Timing for MIDI-message send/receive is shortest loop

  // Should be called every 4-5ms or faster, for the default debouncing time of ~20ms.
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) { buttons[i].check(); }
	
	//if (tickpedals >= 50)
	//{
	//	pollpedalinputs();
	//	tickpedals = 0;
	//}
	
	/////////////////////////////////////////////////////////////////////////////////////////
	// STATE MACHINE
	/////////////////////////////////////////////////////////////////////////////////////////

  std::array<byte, 29> ask_preset_buf = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
            

	//                0        1            2              3        4        5        6                7
	// enum UIStates {UI_idle, UI_home_amp, UI_home_patch, UI_edit, UI_save, UI_name, UI_init_act_set, UI_act_vol_sol, UI_patch, UI_ded_sol, UI_pat_vol_sol};

	if(button_state!=0) // A foot switch was pressed
	{
		TRACE_V_THR30IIPEDAL(Serial.println();)
		TRACE_V_THR30IIPEDAL(Serial.println("button_state: " + String(button_state));)
		TRACE_V_THR30IIPEDAL(Serial.println("Old UI_state: " + String(_uistate));)
		// TRACE_V_THR30IIPEDAL(Serial.println("Old THR_Values.sendChangestoTHR: " + String(THR_Values.sendChangestoTHR));)
		// TRACE_V_THR30IIPEDAL(Serial.println("Old stored_THR_Values.sendChangestoTHR: " + String(stored_THR_Values.sendChangestoTHR));)
		switch (_uistate)
		{
			case UI_idle:
			case UI_home_amp:
			case UI_home_patch:
				switch (button_state)
				{
					case 0:

					break;

					case 1: // Submit pre-selected patch
						switch (_uistate)
						{
							case UI_home_amp: //no patch is active  (but patch key was pressed)
								if(npatches>0) //if at least one patch-settings file is available
								{ //store actual settings and activate presel patch
									TRACE_V_THR30IIPEDAL(Serial.println(F("Storing local settings..."));)
									stored_THR_Values = THR_Values;  //THR30II_Settings class is deep copyable
									patch_activate(presel_patch_id);
								}
							break;

							case UI_home_patch: //a patch is active already
								if(presel_patch_id != active_patch_id)  //a patch is active, but it is a different patch than pre-selected
								{
									//send presel patch
									Serial.printf("\n\rActivating pre-selected patch %d instead of the activated %d...\n\r",presel_patch_id, active_patch_id );
									patch_activate(presel_patch_id);
								}
								else  // return to local settings
								{
									patch_deactivate();	//restore actual settings
								}						
							break;
				
							default:	//UI_idle
								//do nothing
							break;
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 2: // Rotate col/amp/cab, depending on which amp_select_mode is active
						switch(amp_select_mode)
						{
							case COL:
								switch(THR_Values.col)
								{
									case CLASSIC:	 THR_Values.col = BOUTIQUE;	break;
									case BOUTIQUE: THR_Values.col = MODERN; 	break;
									case MODERN:	 THR_Values.col = CLASSIC; 	break;
								}
                // TODO
                //THR_Values.sendChangestoTHR = true;
                //THR_Values.SetColAmp(THR_Values.col, THR_Values.amp);
                //THR_Values.SendColAmp();
                //THR_Values.sendChangestoTHR = false;
                Serial.println("Amp collection switched to: " + String(THR_Values.col));
							break;

							case AMP:
								switch(THR_Values.amp)
								{
									case CLEAN:		THR_Values.amp = CRUNCH;	break;
									case CRUNCH:	THR_Values.amp = LEAD;		break;
									case LEAD:		THR_Values.amp = HI_GAIN;	break;
									case HI_GAIN:	THR_Values.amp = SPECIAL;	break;
									case SPECIAL:	THR_Values.amp = BASS;		break;
									case BASS:		THR_Values.amp = ACO;	  	break;
									case ACO:		  THR_Values.amp = FLAT;		break;
									case FLAT:		THR_Values.amp = CLEAN;		break;
								}
                // TODO
                //THR_Values.sendChangestoTHR = true;
                //THR_Values.SetColAmp(THR_Values.col, THR_Values.amp);
                //THR_Values.sendChangestoTHR = false;
                Serial.println("Amp type switched to: " + String(THR_Values.amp));
							break;

							case CAB:
								switch(THR_Values.cab)
								{
									case British_4x12:		THR_Values.cab = American_4x12;		break;
									case American_4x12:		THR_Values.cab = Brown_4x12;	  	break;
									case Brown_4x12:	  	THR_Values.cab = Vintage_4x12;		break;
									case Vintage_4x12:		THR_Values.cab = Fuel_4x12;		  	break;
									case Fuel_4x12:		  	THR_Values.cab = Juicy_4x12;	  	break;
									case Juicy_4x12:	  	THR_Values.cab = Mods_4x12;		  	break;
									case Mods_4x12:		  	THR_Values.cab = American_2x12;		break;
									case American_2x12:		THR_Values.cab = British_2x12;		break;
									case British_2x12:		THR_Values.cab = British_Blues;		break;
									case British_Blues:		THR_Values.cab = Boutique_2x12;		break;
									case Boutique_2x12:		THR_Values.cab = Yamaha_2x12;		  break;
									case Yamaha_2x12:	  	THR_Values.cab = California_1x12;	break;
									case California_1x12:	THR_Values.cab = American_1x12;		break;
									case American_1x12:		THR_Values.cab = American_4x10;		break;
									case American_4x10:		THR_Values.cab = Boutique_1x12;		break;
									case Boutique_1x12:		THR_Values.cab = Bypass;			    break;
									case Bypass:			    THR_Values.cab = British_4x12;	  break;
								}
                // TODO: To be removed if THR_Values.createPatch() to be used or do not call THR_Values.createPatch() when changing cabinet
                //THR_Values.sendChangestoTHR = true;
                //THR_Values.SendCab(); // This works
                //THR_Values.sendChangestoTHR = false;
								Serial.println("Cabinet switched to: " + String(THR_Values.cab));
							break;
						}
						THR_Values.createPatch();
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 3: // Tap tempo
						THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
						maskUpdate = true;         // Request display update to show new states quickly
						button_state = 0;          // Remove flag, because it is handled
					break;

					case 4: // Decrement patch
						presel_patch_id--;	//decrement pre-selected patch-ID
						if (presel_patch_id < 1)	//detect wraparound
						{
							presel_patch_id = npatches;	//wrap back round to last patch in library
						}
						Serial.printf("Patch #%d pre-selected\n\r", presel_patch_id);
						//if immediate mode selected & new patch selected, send immediately
						if ((send_patch_now) && (presel_patch_id != active_patch_id))	
						{
							patch_activate(presel_patch_id);
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;
						
					case 5: // Increment patch
						presel_patch_id++;	//increment pre-selected patch-ID
						if (presel_patch_id > npatches)	//detect wraparound
						{
							presel_patch_id = 1;	//wrap back round to first patch in library
						}
						Serial.printf("Patch #%d pre-selected\n\r", presel_patch_id);
						//if immediate mode selected & new patch selected, send immediately
						if ((send_patch_now) && (presel_patch_id != active_patch_id))
						{
							patch_activate(presel_patch_id);
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 6:
            nUserPreset++;
            if( nUserPreset > 4 ) { nUserPreset = 0; }
            
            THR_Values.setActiveUserSetting(nUserPreset);
            THR_Values.setUserSettingsHaveChanged(false);
            THR_Values.thrSettings = false;

            if(thr_user_presets[nUserPreset].getActiveUserSetting() == -1)
            {
              isUsrPreset = true;

              Serial.println("Requesting data for User preset #" + String(nUserPreset));

              ask_preset_buf[22] = (byte)nUserPreset;
              outqueue.enqueue(Outmessage(SysExMessage(ask_preset_buf.data(), ask_preset_buf.size()), 88, false, true));
            }
            else
            {
              stored_THR_Values = thr_user_presets[nUserPreset];
              patch_deactivate();
            }
						maskUpdate = true; // Request display update to show new states quickly
						button_state = 0;  // Remove flag, because it is handled
					break;

					case 7: // Toggle dynamics
						switch(dyn_mode)
						{
							case Comp:
								if(THR_Values.unit[COMPRESSOR])	//check compressor unit status
								{
									THR_Values.Switch_On_Off_Compressor_Unit(false); //if on, switch off
									Serial.println("Compressor unit switched off");
								}
								else
								{
									THR_Values.Switch_On_Off_Compressor_Unit(true);	//if off, switch on
									Serial.println("Compressor unit switched on");
								}
							break;

							case Boost:
								if(boost_activated)
								{
									undo_gain_boost();
									Serial.println("Gain boost deactivated");
								}
								else
								{
									do_gain_boost();
									Serial.println("Gain boost activated");

								}
							break;

							case Gate:
								if(THR_Values.unit[GATE])	//check gate unit status
								{
									THR_Values.Switch_On_Off_Gate_Unit(false); //if on, switch off
									Serial.println("Gate unit switched off");
								}
								else
								{
									THR_Values.Switch_On_Off_Gate_Unit(true);	//if off, switch on
									Serial.println("Gate unit switched on");
								}
							break;
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 8: // Toggle Effect
						if(THR_Values.unit[EFFECT])	//check effect unit status
						{
							THR_Values.Switch_On_Off_Effect_Unit(false); //if on, switch off
							Serial.println("Effect unit switched off");
						}
						else
						{
							THR_Values.Switch_On_Off_Effect_Unit(true);	//if off, switch on
							Serial.println("Effect unit switched on");
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;
					
					case 9: // Toggle Echo
						if(THR_Values.unit[ECHO])
						{
							THR_Values.Switch_On_Off_Echo_Unit(false);
							Serial.println("Echo unit switched off");
						}
						else
						{
							THR_Values.Switch_On_Off_Echo_Unit(true);
							Serial.println("Echo unit switched on");
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 10: // Toggle Reverb
						if(THR_Values.unit[REVERB])
						{
							THR_Values.Switch_On_Off_Reverb_Unit(false);
							Serial.println("Reverb unit switched off");
						}
						else
						{
							THR_Values.Switch_On_Off_Reverb_Unit(true);
							Serial.println("Reverb unit switched on");
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 11:
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 12: // Rotate amp select mode (COL -> AMP -> CAB -> )
						switch(amp_select_mode)
						{
							case COL:	amp_select_mode = AMP;	break;
							case AMP:	amp_select_mode = CAB; 	break;
							case CAB:	amp_select_mode = COL; 	break;
						}
						Serial.println("Amp Select Mode: "+String(amp_select_mode));
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 13:
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 14: // Toggle patch selection mode (pre-select or immediate)
						send_patch_now = !send_patch_now;
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 15: // Increment patch to first entry in next bank of 5
						presel_patch_id = ((presel_patch_id-1)/5 + 1) * 5 + 1;	//increment pre-selected patch-ID to first of next bank
						if (presel_patch_id > npatches)	//detect if last patch selected
						{
							presel_patch_id = 1;	//wrap back round to first patch in library
						}
						Serial.printf("Patch #%d pre-selected\n\r", presel_patch_id);
						//if immediate mode selected & new patch selected, send immediately
						if ((send_patch_now) && (presel_patch_id != active_patch_id))
						{
							patch_activate(presel_patch_id);
						}
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 16: // Enter FX edit mode (DISABLED)
//						_uistate = UI_edit;
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 17: // Rotate Dynamics Mode (Boost -> Comp -> Gate -> )
						switch(dyn_mode)
						{
							case Boost:	dyn_mode = Comp; 	break;
							case Comp:	dyn_mode = Gate;	break;
							case Gate:	dyn_mode = Boost; break;
						}
						Serial.println("Dynamics Mode: "+String(dyn_mode));
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 18: // Rotate Effect Mode (Chorus > Flanger > Phaser > Tremolo > )
						switch(THR_Values.effecttype)
						{
							case CHORUS:
								THR_Values.EffectSelect(FLANGER);
								Serial.println("Effect unit switched from Chorus to Flanger");
							break;

							case FLANGER:
								THR_Values.EffectSelect(PHASER);
								Serial.println("Effect unit switched from Flanger to Phaser");
							break;

							case PHASER:
								THR_Values.EffectSelect(TREMOLO);
								Serial.println("Effect unit switched from Phaser to Tremolo");
							break;

							case TREMOLO:
								THR_Values.EffectSelect(CHORUS);
								Serial.println("Effect unit switched from Tremolo to Chorus");
							break;
						}
						THR_Values.createPatch();
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;
					
					case 19: // Rotate Echo Mode (Tape Echo > Digital Delay > )
						switch(THR_Values.echotype)
						{
							case TAPE_ECHO:
								THR_Values.EchoSelect(DIGITAL_DELAY);
								Serial.println("Echo unit switched from Tape Echo to Digital Delay");
							break;

							case DIGITAL_DELAY:
								THR_Values.EchoSelect(TAPE_ECHO);
								Serial.println("Effect unit switched from Digital Delay to Tape Echo");
							break;
						}
						THR_Values.createPatch();
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;

					case 20: // Rotate Reverb Mode (Spring > Room > Plate > Hall > )
						switch(THR_Values.reverbtype)
						{
							case SPRING:
								THR_Values.ReverbSelect(ROOM);
								Serial.println("Reverb unit switched from Spring to Room");
							break;

							case ROOM:
								THR_Values.ReverbSelect(PLATE);
								Serial.println("Reverb unit switched from Room to Plate");
							break;

							case PLATE:
								THR_Values.ReverbSelect(HALL);
								Serial.println("Reverb unit switched from Plate to Hall");
							break;

							case HALL:
								THR_Values.ReverbSelect(SPRING);
								Serial.println("Reverb unit switched from Hall to Spring");
							break;
						}
						THR_Values.createPatch();
						maskUpdate=true;  //request display update to show new states quickly
						button_state=0;  //remove flag, because it is handled
					break;
					
					default:
					break;
				}
			break;

			case UI_edit:
			break;

			case UI_save:
			break;

			case UI_name:
			break;

			default:
			break;
		}
		TRACE_V_THR30IIPEDAL(Serial.println("New UI_state: " + String(_uistate));)
		// TRACE_V_THR30IIPEDAL(Serial.println("New THR_Values.sendChangestoTHR: " + String(THR_Values.sendChangestoTHR));)
		// TRACE_V_THR30IIPEDAL(Serial.println("New stored_THR_Values.sendChangestoTHR: " + String(stored_THR_Values.sendChangestoTHR));)

	} //button_state!=0

}//end of loop()



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

//void pollpedalinputs()	// Poll pedal inputs
//{
//	pedal_1_sense = !digitalRead(PEDAL_1_SENSE_PIN);  //sense whether pedal 1 present
//	if (pedal_1_sense) {
//		pedal_1_old_val = pedal_1_val;
//		pedal_1_sum = 0;
//		//for loop to read pedal_buf_size pedal inputs into pedal_1_buf, then average.
//		for (int i=0; i<pedal_buf_size; i++) {
//			pedal_1_buf[i] = 1023-analogRead(PEDAL_1_PIN);
//			pedal_1_sum += pedal_1_buf[i];
//		}
//		pedal_1_val = pedal_1_sum/pedal_buf_size;
//		// pedal_1_val = 1023-analogRead(PEDAL_1_PIN);   //read in pedal 1 input
//	}
//	else {
//		pedal_1_val = 0;
//	}
//	if ((pedal_1_val > (pedal_1_old_val + pedal_margin)) || (pedal_1_val < (pedal_1_old_val - pedal_margin))) {
//		maskUpdate=true;  //request display update to show new states quickly
//	}
//
//  	pedal_2_sense = !digitalRead(PEDAL_2_SENSE_PIN);  //sense whether pedal 2 present
//  	if (pedal_2_sense) {
//		pedal_2_old_val = pedal_2_val;
//		pedal_2_sum = 0;
//		//for loop to read pedal_buf_size pedal inputs into pedal_2_buf, then average.
//		for (int i=0; i<pedal_buf_size; i++) {
//			pedal_2_buf[i] = 1023-analogRead(PEDAL_2_PIN);
//			pedal_2_sum += pedal_2_buf[i];
//		}
//		pedal_2_val = pedal_2_sum/pedal_buf_size;
//		// pedal_2_val = 1023-analogRead(PEDAL_2_PIN);   //read in pedal 2 input
//	}
//	else {
//		pedal_2_val = 0;
//	}
//	if ((pedal_2_val > (pedal_2_old_val + pedal_margin)) || (pedal_2_val < (pedal_2_old_val - pedal_margin))) {
//		updatemastervolume(pedal_2_val);
//		// Serial.println(pedal_2_val);
//		maskUpdate=true;  //request display update to show new states quickly
//	}
//}

void updatemastervolume(int mastervolume)
{
	Serial.print("updatemastervolume(" + String(mastervolume) + ") = ");
	scaledvolume = min(max((static_cast<double>(mastervolume)+1) * 100 / 1024, 0), 100);
	Serial.println(scaledvolume);
	THR_Values.sendChangestoTHR=true;
	THR_Values.SetControl(CTRL_MASTER, scaledvolume);
	THR_Values.sendChangestoTHR=false;
}

void do_gain_boost()
{
  do_volume_patch();
	//std::copy(THR_Values.control.begin(),THR_Values.control.end(),THR_Values.control_store.begin());  //save volume and tone related settings

	//THR_Values.sendChangestoTHR=true;
	//THR_Values.SetControl(CTRL_GAIN, min(THR_Values.GetControl(CTRL_GAIN)+40, 100));
	//THR_Values.sendChangestoTHR=false;
	boost_activated = true;
}

void undo_gain_boost()
{
  undo_volume_patch();
	//THR_Values.sendChangestoTHR=true;
	//THR_Values.SetControl(CTRL_GAIN, THR_Values.control_store[CTRL_GAIN]);
	//THR_Values.sendChangestoTHR=false;
	boost_activated = false;
}

void do_volume_patch() // Increases Volume and/or Tone settings for SOLO to be louder than actual patch
{
	std::copy(THR_Values.control.begin(), THR_Values.control.end(), THR_Values.control_store.begin());  //save volume and tone related settings

	if( THR_Values.GetControl(CTRL_MASTER) < 100 / 1.333333 ) // Is there enough headroom to increase Master Volume by 33%?
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
		double tone_margin=(100.0-max_tone)/max_tone; // Maximum equal TONE-Settings multiplier? (e.g. 0.28)
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
	THR_Values.SetControl( CTRL_MASTER, THR_Values.control_store[CTRL_MASTER] );
	// THR_Values.SetControl( CTRL_GAIN, THR_Values.control_store[CTRL_GAIN] ); // Gain is not involved in Volume-Patch
	THR_Values.SetControl( CTRL_BASS,   THR_Values.control_store[CTRL_BASS] );
	THR_Values.SetControl( CTRL_MID,    THR_Values.control_store[CTRL_MID] );
	THR_Values.SetControl( CTRL_TREBLE, THR_Values.control_store[CTRL_TREBLE] );
	THR_Values.sendChangestoTHR = false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// TEMPO TAP TIMING
/////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t tempotaptime0 = 0;
uint32_t tempotaptime1 = 0;
uint32_t tempotapint = 1000;
uint32_t tempotaptimeout = 1500;
uint8_t tempotapbpm = 0;
uint8_t tempotapsetting = 50;

void THR30II_Settings::EchoTempoTap()
{
	// Get button press interval (discarding spurious results from long intervals, i.e. only do stuff on multiple presses)
	tempotaptime0 = tempotaptime1; // Time of last press
	tempotaptime1 = millis();			 // Time of most recent press
	if( tempotaptime1 - tempotaptime0 < tempotaptimeout )	// Ignore long intervals > tempotaptimeout
	{
    // TODO Check timing to be similar to using the tap: time THR button 
		tempotapint = tempotaptime1 - tempotaptime0; // Time interval between presses
		tempotapsetting = tempotapint / 10;	         // Convert tapped interval to setting value
		Serial.println(tempotapsetting);
		tempotapbpm = 60000 / tempotapint;
		Serial.println(tempotapbpm);

		echo_setting[TAPE_ECHO][TA_TIME] = tempotapsetting;
		echo_setting[DIGITAL_DELAY][DD_TIME] = tempotapsetting;
		createPatch();
    
    // Set only the time parameter(s) iso sending a complete patch
    //std::map<String,uint16_t> &glob = Constants::glo;
    //EchoSetting(TAPE_ECHO,     glob["Time"], tempotapsetting);
    //EchoSetting(DIGITAL_DELAY, glob["Time"], tempotapsetting);
	}
}

//////////////////////////////////////////////
// Manage patches activation
//////////////////////////////////////////////
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

void patch_activate(uint16_t pnr) // Check patchnumber and send patch as a SysEx message to THR30II
{
	if( pnr <= npatches ) // This patch-id is available
	{
		// Activate the patch (overwrite either the actual patch or the local settings)
		TRACE_THR30IIPEDAL(Serial.printf("Patch_activate(): Activating patch #%d \n\r", pnr);)
		send_patch( pnr ); // Now send this patch as a SysEx message to THR30II 
		active_patch_id = pnr;
		boost_activated = false;
		_uistate = UI_home_patch; // State "patch" reached
	} 
	else
	{
		TRACE_THR30IIPEDAL(Serial.printf("Patch_activate(): invalid pnr");)
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

int THR30II_Settings::SetLoadedPatch(const DynamicJsonDocument &djd ) // Invoke all settings from a loaded patch
{
	if (!MIDI_Activated)
	{
		TRACE_THR30IIPEDAL(Serial.println("Midi Sync is not ready!");)
		// return -1; // TODO: Shall we return?
	}

	sendChangestoTHR = false; // We apply all the settings in one shot with a MIDI-Patch-Upload to THRII via "createPatch()" 
                            // and not each setting separately. So we use the setters only for our local fields!

	TRACE_THR30IIPEDAL(Serial.println(F("SetLoadedPatch(): Setting loaded patch..."));)

	std::map<String, uint16_t> &glob = Constants::glo;
	
	// Patch name FIXME: This overwrites the user preset patch names!!!!!!!!!
  // This set may not be needed
	//SetPatchName(djd["data"]["meta"]["name"].as<const char*>(), presel_patch_id);
	
	// Unknown global parameter
	Tnid = djd["data"]["meta"]["tnid"].as<uint32_t>();

	// Unit states -----------------------------------------------------------------
	Switch_On_Off_Compressor_Unit(djd["data"]["tone"]["THRGroupFX1Compressor"]["@enabled"].as<bool>());
	Switch_On_Off_Gate_Unit(djd["data"]["tone"]["THRGroupGate"]["@enabled"].as<bool>());
	Switch_On_Off_Effect_Unit(djd["data"]["tone"]["THRGroupFX2Effect"]["@enabled"].as<bool>());
	Switch_On_Off_Echo_Unit(djd["data"]["tone"]["THRGroupFX3EffectEcho"]["@enabled"].as<bool>());
	Switch_On_Off_Reverb_Unit(djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@enabled"].as<bool>());
	TRACE_V_THR30IIPEDAL(Serial.printf("Comp on/off: %d\n\r", unit[THR30II_UNITS::COMPRESSOR] );)
	TRACE_V_THR30IIPEDAL(Serial.printf("Gate on/off: %d\n\r", unit[THR30II_UNITS::GATE] );)
	TRACE_V_THR30IIPEDAL(Serial.printf("Eff on/off: %d\n\r",  unit[THR30II_UNITS::EFFECT] );)
	TRACE_V_THR30IIPEDAL(Serial.printf("Echo on/off: %d\n\r", unit[THR30II_UNITS::ECHO] );)
	TRACE_V_THR30IIPEDAL(Serial.printf("Rev on/off: %d\n\r",  unit[THR30II_UNITS::REVERB] );)

	// Global parameter tempo -----------------------------------------------------
	ParTempo = djd["data"]["tone"]["global"]["THRPresetParamTempo"].as<uint32_t>();

	// FX1 Compressor (no different types) ----------------------------------------
	CompressorSetting(CO_SUSTAIN, djd["data"]["tone"]["THRGroupFX1Compressor"]["Sustain"].as<double>() * 100);
	CompressorSetting(CO_LEVEL,   djd["data"]["tone"]["THRGroupFX1Compressor"]["Level"].as<double>()   * 100);
	// CompressorSetting(THR30II_COMP_SET.MIX, pat.data.tone.THRGroupFX1Compressor. * 100); // No Mix-Parameter in Patch-Files

	// Amp/Collection --------------------------------------------------------------
	uint16_t key = glob[djd["data"]["tone"]["THRGroupAmp"]["@asset"].as<const char*>() ];  // e.g. "THR10C_DC30" => 0x88
	setColAmp(key);
	TRACE_V_THR30IIPEDAL(Serial.printf("Amp: %x\n\r", key);)

	// Main Amp Controls -----------------------------------------------------------
	SetControl(CTRL_GAIN,   djd["data"]["tone"]["THRGroupAmp"]["Drive"].as<double>()  * 100);
	SetControl(CTRL_MASTER, djd["data"]["tone"]["THRGroupAmp"]["Master"].as<double>() * 100);
	SetControl(CTRL_BASS,   djd["data"]["tone"]["THRGroupAmp"]["Bass"].as<double>()   * 100);
	SetControl(CTRL_MID,    djd["data"]["tone"]["THRGroupAmp"]["Mid"].as<double>()    * 100);
	SetControl(CTRL_TREBLE, djd["data"]["tone"]["THRGroupAmp"]["Treble"].as<double>() * 100);
	TRACE_V_THR30IIPEDAL(Serial.println( "Controls: G, M , B , Mi , T" );)
	TRACE_V_THR30IIPEDAL(Serial.println(control[0]);)
	TRACE_V_THR30IIPEDAL(Serial.println(control[1]);)
	TRACE_V_THR30IIPEDAL(Serial.println(control[2]);)
	TRACE_V_THR30IIPEDAL(Serial.println(control[3]);)
	TRACE_V_THR30IIPEDAL(Serial.println(control[4]);)

	// Cab Simulation --------------------------------------------------------------
	uint16_t val = djd["data"]["tone"]["THRGroupCab"]["SpkSimType"].as<uint16_t>(); // e.g. 10 
	SetCab((THR30II_CAB)val);                                                       // e.g. 10 => Boutique_2x12
	TRACE_V_THR30IIPEDAL(Serial.printf("Cab: %x\n\r", val);)

	// Noise Gate ------------------------------------------------------------------
	double v = 100.0 - 100.0 / 96 * -  djd["data"]["tone"]["THRGroupGate"]["Thresh"].as<double>() ; // Change range from [-96dB ... 0dB] to [0...100]
	GateSetting(GA_THRESHOLD, v);
	GateSetting(GA_DECAY, djd["data"]["tone"]["THRGroupGate"]["Decay"].as<double>() * 100);
	TRACE_V_THR30IIPEDAL(Serial.println("Gate Thresh:");)
	TRACE_V_THR30IIPEDAL(Serial.println(v);)

	// FX2 Effect Settings ---------------------------------------------------------
	String type = djd["data"]["tone"]["THRGroupFX2Effect"]["@asset"].as<const char*>();
	TRACE_V_THR30IIPEDAL(Serial.printf("EffType: %s\n\r", type.c_str() );)

	if( type == "BiasTremolo" )
	{
		EffectSelect(TREMOLO);
		EffectSetting(TREMOLO, glob["Depth"],  djd["data"]["tone"]["THRGroupFX2Effect"]["Depth"].as<double>()   * 100);
		EffectSetting(TREMOLO, glob["Speed"],  djd["data"]["tone"]["THRGroupFX2Effect"]["Speed"].as<double>()   * 100);
		EffectSetting(TREMOLO, glob["FX2Mix"], djd["data"]["tone"]["THRGroupFX2Effect"]["@wetDry"].as<double>() * 100);
	}
	else if( type == "StereoSquareChorus" )
	{
		EffectSelect(CHORUS);
		EffectSetting(CHORUS, glob["Depth"],    djd["data"]["tone"]["THRGroupFX2Effect"]["Depth"].as<double>()    * 100);
		EffectSetting(CHORUS, glob["Freq"],     djd["data"]["tone"]["THRGroupFX2Effect"]["Freq"].as<double>()     * 100);
		EffectSetting(CHORUS, glob["Pre"],      djd["data"]["tone"]["THRGroupFX2Effect"]["Pre"].as<double>()      * 100);
		EffectSetting(CHORUS, glob["Feedback"], djd["data"]["tone"]["THRGroupFX2Effect"]["Feedback"].as<double>() * 100);
		EffectSetting(CHORUS, glob["FX2Mix"],   djd["data"]["tone"]["THRGroupFX2Effect"]["@wetDry"].as<double>()  * 100);
	}
	else if( type == "L6Flanger" )
	{
		EffectSelect(FLANGER );
		EffectSetting(FLANGER, glob["Depth"],  djd["data"]["tone"]["THRGroupFX2Effect"]["Depth"].as<double>()   * 100);
		EffectSetting(FLANGER, glob["Freq"],   djd["data"]["tone"]["THRGroupFX2Effect"]["Freq"].as<double>()    * 100);
		EffectSetting(FLANGER, glob["FX2Mix"], djd["data"]["tone"]["THRGroupFX2Effect"]["@wetDry"].as<double>() * 100);
	}
	else if( type == "Phaser" )
	{
		EffectSelect(PHASER);
		EffectSetting(PHASER, glob["Feedback"], djd["data"]["tone"]["THRGroupFX2Effect"]["Feedback"].as<double>() * 100);
		EffectSetting(PHASER, glob["Speed"],    djd["data"]["tone"]["THRGroupFX2Effect"]["Speed"].as<double>()    * 100);
		EffectSetting(PHASER, glob["FX2Mix"],   djd["data"]["tone"]["THRGroupFX2Effect"]["@wetDry"].as<double>()  * 100);
	}

	// FX3 Echo Settings ----------------------------------------------------------
	type = djd["data"]["tone"]["THRGroupFX3EffectEcho"]["@asset"].as<const char*>();
	TRACE_V_THR30IIPEDAL(Serial.printf("EchoType: %s\n\r", type.c_str() );)

	if( type == "TapeEcho" )
	{
		EchoSelect(TAPE_ECHO);
		EchoSetting(TAPE_ECHO, glob["Bass"],     djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Bass"].as<double>()     * 100);
		EchoSetting(TAPE_ECHO, glob["Feedback"], djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Feedback"].as<double>() * 100);
		EchoSetting(TAPE_ECHO, glob["FX3Mix"],   djd["data"]["tone"]["THRGroupFX3EffectEcho"]["@wetDry"].as<double>()  * 100);
		EchoSetting(TAPE_ECHO, glob["Time"],     djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Time"].as<double>()     * 100);
		EchoSetting(TAPE_ECHO, glob["Treble"],   djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Treble"].as<double>()   * 100);
	}
	else if( type == "L6DigitalDelay" )
	{
		EchoSelect(DIGITAL_DELAY);
		EchoSetting(DIGITAL_DELAY, glob["Bass"],     djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Bass"].as<double>()     * 100);
		EchoSetting(DIGITAL_DELAY, glob["Feedback"], djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Feedback"].as<double>() * 100);
		EchoSetting(DIGITAL_DELAY, glob["FX3Mix"],   djd["data"]["tone"]["THRGroupFX3EffectEcho"]["@wetDry"].as<double>()  * 100);
		EchoSetting(DIGITAL_DELAY, glob["Time"],     djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Time"].as<double>()     * 100);
		EchoSetting(DIGITAL_DELAY, glob["Treble"],   djd["data"]["tone"]["THRGroupFX3EffectEcho"]["Treble"].as<double>()   * 100);
	}

	// FX4 Reverb Settings ----------------------------------------------------------
	type = (djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@asset"].as<const char*>());
	TRACE_V_THR30IIPEDAL(Serial.printf("RevType: %s\n\r", type.c_str() );)

	if( type == "StandardSpring" )
	{
		ReverbSelect(SPRING);
		ReverbSetting(SPRING, glob["Time"],       djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Time"].as<double>()    * 100);
		ReverbSetting(SPRING, glob["Tone"],       djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Tone"].as<double>()    * 100);
		ReverbSetting(SPRING, glob["FX4WetSend"], djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@wetDry"].as<double>() * 100);
	}
	else if( type == "LargePlate1" )
	{
		ReverbSelect(PLATE);
		ReverbSetting(PLATE, glob["Decay"],      djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Decay"].as<double>()    * 100);
		ReverbSetting(PLATE, glob["Tone"],       djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Tone"].as<double>()     * 100);
		ReverbSetting(PLATE, glob["PreDelay"],   djd["data"]["tone"]["THRGroupFX4EffectReverb"]["PreDelay"].as<double>() * 100);
		ReverbSetting(PLATE, glob["FX4WetSend"], djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@wetDry"].as<double>()  * 100);
	}
	else if( type == "ReallyLargeHall" )
	{
		ReverbSelect(HALL);
		ReverbSetting(HALL, glob["Decay"],      djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Decay"].as<double>()    * 100);
		ReverbSetting(HALL, glob["Tone"],       djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Tone"].as<double>()     * 100);
		ReverbSetting(HALL, glob["PreDelay"],   djd["data"]["tone"]["THRGroupFX4EffectReverb"]["PreDelay"].as<double>() * 100);
		ReverbSetting(HALL, glob["FX4WetSend"], djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@wetDry"].as<double>()  * 100);
	}
	else if( type == "SmallRoom1" )
	{
		ReverbSelect(ROOM);
		ReverbSetting(ROOM, glob["Decay"],      djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Decay"].as<double>()    * 100);
		ReverbSetting(ROOM, glob["Tone"],       djd["data"]["tone"]["THRGroupFX4EffectReverb"]["Tone"].as<double>()     * 100);
		ReverbSetting(ROOM, glob["PreDelay"],   djd["data"]["tone"]["THRGroupFX4EffectReverb"]["PreDelay"].as<double>() * 100);
		ReverbSetting(ROOM, glob["FX4WetSend"], djd["data"]["tone"]["THRGroupFX4EffectReverb"]["@wetDry"].as<double>()  * 100);
	}

  createPatch();  // Send all settings as a patch dump SysEx to THRII
	
	TRACE_THR30IIPEDAL(Serial.println(F("SetLoadedPatch(): Done setting."));)
	return 0;
}

THR30II_Settings::States THR30II_Settings::_state = THR30II_Settings::States::St_idle;  // The actual state of the state engine for creating a patch

void THR30II_Settings::createPatch() // Fill send buffer with actual settings, creating a valid SysEx for sending to THR30II
{
	//1.) Make the data buffer (structure and values) store the length.
	//    Add 12 to get the 2nd length-field for the header. Add 8 to get the 1st length field for the header.
	//2.) Cut it to slices with 0x0d02 (210 dez.) bytes (gets the payload of the frames)
	//    These frames will thus not have incomplete 8/7 groups after bitbucketing but total frame length is 253 (just below the possible 255 bytes).
	//    The remaining bytes will build the last frame (perhaps with last 8/7 group inclomplete)
	//    Store the length of the last slice (all others are fixed 210 bytes long.)
	//3.) Create the header:
	//    SysExStart:  f0 00 01 0c 22 02 4d (always the same)        (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 Get actual counter for memory frames and append it here
	//                 00 01 0b  ("same frame counter" and payload size for the header)
	//                 hang following 4-Byte values together:
	//
	//                 0d 00 00 00   (Opcode for memory write/send)
	//                 (len +8+12)   (1st length field = total length = data length + 3 values + type field/netto length )
	//                 FF FF FF FF   (user patch number to overwrite/ 0xFFFFFFFF for actual patch)
	//                 (len   +12)   (2nd length field = netto length= data length + the following 3 values)
	//                 00 00 00 00   (always the same, kind of opcode?)
	//                 01 00 00 00   (always the same, kind of opcode?)
	//                 00 00 00 00   (always the same, kind of opcode?)
	//
	//                 Bitbucket this group (gives no incomplete 8/7 group!)
	//                 and append it to the header
	//                 finish header with 0xF7
	//                 Header frame may already be sent out!
	//4.)              For each slice:
	//                 Bitbucket encode the slice
	//                 Build frame:
	//                 f0 00 01 0c 22 02 4d(always the same)        (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 Get actual counter for memory frames and append it here
	//                 append same frame counter (is the slice counter) as a byte
	//                 0d 01  (except for last frame, where we use it's stored length before bitbucketing)
	//                 bitbucketed data for this slice
	//                 0xF7
	//                 Send it out.
	TRACE_V_THR30IIPEDAL(Serial.println(F("Create_patch(): "));)

	std::array<byte, 2000> dat;
	auto datlast = dat.begin(); // Iterator to last element (starts on begin!)
	std::array<byte, 300> sb;
	auto sblast = sb.begin();   // Iterator to last element (starts on begin!)
	std::array<byte, 300> sb2;
	auto sb2last = sb2.begin(); // Iterator to last element (starts on begin!)

	std::map<String, uint16_t> &glob = Constants::glo;
	
	std::array<byte, 4> toInsert4;
	std::array<byte, 6> toInsert6;

	#define datback(x) for(const byte &b : x ) { *datlast++ = b; };
	
	// Macro for converting a 32-Bit value to a 4-byte array<byte, 4> and append it to "dat"
	#define conbyt4(x) toInsert4={ (byte)(x), (byte)((x)>>8), (byte)((x)>>16), (byte)((x)>>24) }; datlast = std::copy( std::begin(toInsert4), std::end(toInsert4), datlast ); 
	
  // Macro for appending a 16-Bit value to "dat"
	#define conbyt2(x) *datlast ++= (byte)(x); *datlast ++= (byte)((x)>>8); 

	// 1.) Make the data buffer (structure and values) -----------------------------------------
	datback(tokens["StructOpen"]);
	// Meta
	datback(tokens["Meta"] ) ;          
	datback(tokens["TokenMeta"] );
	toInsert6 = { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00 };
	datlast = std::copy(std::begin(toInsert6), std::end(toInsert6), datlast); // Number 0x0000, type 0x00040000 (String)
	
	conbyt4( patchNames[0].length() + 1 ) // Length of patchname (incl. '\0')
	// 32Bit-Value (little endian; low byte first) 
	
	std::string nam = patchNames[0u].c_str();
	datback( nam );  // Copy the patchName  //!!ZWEZWE!! mind UTF-8 ?
	*datlast++='\0'; // Append '\0' to the patchname
	
	toInsert6 = { 0x01, 0x00, 0x00, 0x00, 0x02, 0x00 };
	datlast = std::copy(std::begin(toInsert6), std::end(toInsert6), datlast); // Number 0x0001, type 0x00020000 (int)
	
	conbyt4(Tnid); // 32Bit-Value (little endian; low byte first) Tnid

	toInsert6 = { 0x02, 0x00, 0x00, 0x00, 0x02, 0x00 }; // Number 0x0002, type 0x00020000 (int)
	datlast = std::copy(std::begin(toInsert6), std::end(toInsert6), datlast);

	conbyt4(UnknownGlobal);	// 32Bit-Value (little endian; low byte first) Unknown Global

	toInsert6 = { 0x03, 0x00, 0x00, 0x00, 0x03, 0x00 }; // Number 0x0003, type 0x00030000 (int)
	datlast=std::copy(std::begin(toInsert6), std::end(toInsert6), datlast);

	conbyt4(ParTempo); // 32Bit-Value (little endian; low byte first) ParTempo (min = 110 = 0x00000000)
	
	datback(tokens["StructClose"]);
	// Data           
	datback(tokens["StructOpen"]);
	datback(tokens["Data"]);
	datback(tokens["TokenData"]);

	// Unit GuitarProc
	datback(tokens["UnitOpen"]);
	conbyt2(glob["GuitarProc"]);
	datback(tokens["UnitType"]);
	datback(tokens["PseudoVal"]);
	conbyt2(glob["Y2GuitarFlow"]);
	datback(tokens["ParCount"]);
	datback(tokens["PseudoType"]);
	conbyt4(11u); // 32-Bit value  number of parameters (here: 11)
	// 1601 = FX1EnableState  (CompOn)
	conbyt2(glob["FX1EnableState"]);
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[COMPRESSOR] ? 0x01u : 0x00u);
	// 1901 = FX2EnableState  (EffectOn)
	conbyt2(glob["FX2EnableState"]);
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[EFFECT] ? 0x01u : 0x00u);
	// 1801 = FX2MixState     (Eff.Mix)
	conbyt2(glob["FX2MixState"]);				
	conbyt4(0x00030000u); // Type int
	
	conbyt4(ValToNumber(effect_setting[PHASER][PH_MIX])); // MIX is independent from the effect type!
	//1C01 = FX3EnableState  (EchoOn)
	conbyt2(glob["FX3EnableState"]);
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[ECHO] ? 0x01u : 0x00u);         
	// 1B01 = FX3MixState     (EchoMix)
	conbyt2(glob["FX3MixState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(echo_setting[TAPE_ECHO][TA_MIX])); // MIX is independent from the echo type!
	
	// 1F01 = FX4EnableState  (RevOn)
	conbyt2(glob["FX4EnableState"]);		
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[REVERB] ? 0x01u : 0x00u);
	// 2601 = FX4WetSendState (RevMix)
	conbyt2(glob["FX4WetSendState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(reverb_setting[SPRING][SP_MIX]) ); // MIX is independent from the effect type!
	
  // 2101 = GateEnableState (GateOn)
	conbyt2(glob["GateEnableState"]);
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[GATE] ? 0x01u : 0x00u);

	// 2401 = SpkSimTypeState (Cabinet)
	conbyt2(glob["SpkSimTypeState"]);
	conbyt4(0x00020000u); // Type enum
	conbyt4((uint32_t)cab); // Cabinet is a value from 0x00 to 0x10
	
	// F800 = DecayState      (GateDecay)
	conbyt2(glob["DecayState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(gate_setting[GA_DECAY]));
	// 2501 = ThreshState     (GateThreshold)
	conbyt2(glob["ThreshState"]);			     
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber_Threshold(gate_setting[GA_THRESHOLD]) );
	
	// Unit Compressor
	datback(tokens["UnitOpen"]);
	conbyt2(glob["FX1"]);						
	datback(tokens["UnitType"]);		
	datback(tokens["PseudoVal"]);		
	conbyt2(glob["RedComp"]);				
	datback(tokens["ParCount"]);			
	datback(tokens["PseudoType"]);			
	conbyt4(2u); // 32-Bit value  number of parameters (here: 2)
	
	// BF00 = Compressor Level(LevelState)
	conbyt2(glob["LevelState"]);	
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(compressor_setting[CO_LEVEL]) );	
	// BE00 = Compressor Sustain(SustainState)
	conbyt2(glob["SustainState"]);						
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(compressor_setting[CO_SUSTAIN]) );
	datback(tokens["UnitClose"]); // Close Compressor Unit
	
	// Unit AMP (0x0A01)
	datback(tokens["UnitOpen"]);	
	conbyt2(glob["Amp"]);
	datback(tokens["UnitType"]);	
	datback(tokens["PseudoVal"]);
	conbyt2(THR30IIAmpKeys[col_amp(col,amp)]);
	datback(tokens["ParCount"]);	
	datback(tokens["PseudoType"]);			
	conbyt4(5u); // 32-Bit value  number of parameters (here: 5)

	// 4f 00 CTRL BASS (BassState)
	conbyt2(glob["BassState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(control[CTRL_BASS]));
	// 52 00 GAIN (DriveState)
	conbyt2(glob["DriveState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(control[CTRL_GAIN]));
	// 53 00 MASTER (MasterState)
	conbyt2(glob["MasterState"]);
	conbyt4(0x00030000u);//Type int
	conbyt4(ValToNumber(control[CTRL_MASTER]) );
	//50 00 MID (MidState)
	conbyt2(glob["MidState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(control[CTRL_MID]));
	// 51 00 CTRL TREBLE (TrebleState)
	conbyt2(glob["TrebleState"]);
	conbyt4(0x00030000u); // Type int
	conbyt4(ValToNumber(control[CTRL_TREBLE]));
	datback(tokens["UnitClose"] ); // Close AMP Unit
	
  // Unit EFFECT (FX2) (0x0E01)
	datback(tokens["UnitOpen"]);
	conbyt2(glob["FX2"]);
	datback(tokens["UnitType"]);	
	datback(tokens["PseudoVal"]);	
	conbyt2(THR30II_EFF_TYPES_VALS[effecttype].key); // EffectType as a key value
	
	datback(tokens["ParCount"]);    
	datback(tokens["PseudoType"]);
	
  // Number and kind of parameters depend on the selected effect type            
	switch( effecttype )
	{
		case PHASER:
			conbyt4(2u); // 32-Bit value  number of parameters (here: 2)
			conbyt2(glob["FeedbackState"]);      
			conbyt4(0x00030000u); // Type int
			
			conbyt4(ValToNumber(effect_setting[effecttype][PH_FEEDBACK]));
			conbyt2(glob["FreqState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][PH_SPEED]));
			break;

		case TREMOLO:
			conbyt4(2u); // 32-Bit value  number of parameters (here: 2)
			conbyt2(glob["DepthState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][TR_DEPTH]));
			conbyt2(glob["SpeedState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][TR_SPEED]));
			break;

		case FLANGER:
			conbyt4(2u); // 32-Bit value  number of parameters (here: 2)
			conbyt2(glob["DepthState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][FL_DEPTH]));
			conbyt2(glob["FreqState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][FL_SPEED]));
			break;

		case CHORUS:
			conbyt4(4u); // 32-Bit value  number of parameters (here: 4)
			conbyt2(glob["DepthState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][CH_DEPTH]));
			conbyt2(glob["FeedbackState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][CH_FEEDBACK]));
			conbyt2(glob["FreqState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][CH_SPEED]));
			conbyt2(glob["PreState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(effect_setting[effecttype][CH_PREDELAY]));
			break;
	}
	datback(tokens["UnitClose"]); // Close EFFECT Unit

	// 0f 01 Unit ECHO (FX3)
	datback(tokens["UnitOpen"]);
	conbyt2(glob["FX3"]);
	datback(tokens["UnitType"]);
	datback(tokens["PseudoVal"]);
	conbyt2(THR30II_ECHO_TYPES_VALS[echotype].key); // EchoType as a key value (variable since 1.40.0a)
	//dat.AddRange(BitConverter.GetBytes(glob["TapeEcho"])); // Before 1.40.0a "TapeEcho" was fixed type for Unit "Echo"
	datback(tokens["ParCount"]);
	datback(tokens["PseudoType"]);
	
	// Number and kind of parameters could depend on the selected echo type (in fact it does not)           
	switch( echotype )
	{
		case TAPE_ECHO:
			conbyt4(4u); // 32-Bit value  number of parameters (here: 4)
			conbyt2(glob["BassState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][TA_BASS]));
			conbyt2(glob["FeedbackState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][TA_FEEDBACK]));
			conbyt2(glob["TimeState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][TA_TIME]));
			conbyt2(glob["TrebleState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][TA_TREBLE]));
			break;

		case DIGITAL_DELAY:
			conbyt4(4u); // 32-Bit value  number of parameters (here: 4)
			conbyt2(glob["BassState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][DD_BASS]));
			conbyt2(glob["FeedbackState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][DD_FEEDBACK]));
			conbyt2(glob["TimeState"]);			
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][DD_TIME]));
			conbyt2(glob["TrebleState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(echo_setting[echotype][DD_TREBLE]));
			break;
	}
	datback(tokens["UnitClose"]);	// Close ECHO Unit
	
	// 12 01 Unit REVERB
	datback(tokens["UnitOpen"]);
	conbyt2(glob["FX4"]);
	datback(tokens["UnitType"]);
	datback(tokens["PseudoVal"]);
	conbyt2(THR30II_REV_TYPES_VALS[reverbtype].key); 	
	datback(tokens["ParCount"]);
	datback(tokens["PseudoType"]);
	
  // Number and kind of parameters depend on the selected reverb type            
	switch( reverbtype )
	{
		case SPRING:
			conbyt4(2u); // 32-Bit value  number of parameters (here: 2)
			conbyt2(glob["TimeState"]);	
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][SP_REVERB]));
			conbyt2(glob["ToneState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][SP_TONE]));
			break;

		case PLATE:
			conbyt4(3u); // 32-Bit value  number of parameters (here: 3)
			conbyt2(glob["DecayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][PL_DECAY]));
			conbyt2(glob["PreDelayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][PL_PREDELAY]));
			conbyt2(glob["ToneState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][PL_TONE]));
			break;

		case HALL:
			conbyt4(3u); // 32-Bit value  number of parameters (here: 3)
			conbyt2(glob["DecayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][HA_DECAY]));
			conbyt2(glob["PreDelayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][HA_PREDELAY]));
			conbyt2(glob["ToneState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][HA_TONE]));
			break;

		case ROOM:
			conbyt4(3u); // 32-Bit value  number of parameters (here: 3)
			conbyt2(glob["DecayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][RO_DECAY]));
			conbyt2(glob["PreDelayState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][RO_PREDELAY]));
			conbyt2(glob["ToneState"]);
			conbyt4(0x00030000u); // Type int
			conbyt4(ValToNumber(reverb_setting[reverbtype][RO_TONE]));
			break;
	}
	datback(tokens["UnitClose"]);   // Close REVERB Unit
	
	datback(tokens["UnitClose"]);   // Close Unit GuitarProcessor
	
	datback(tokens["StructClose"]);  // Close Structure Data
	
	// Print whole buffer as a chain of HEX
	// TRACE_V_THR30IIPEDAL( Serial.printf("\n\rRaw frame before slicing/enbucketing:\n\r"); hexdump(dat,datlast-dat.begin());)

	// Now store the length.
	// Add 12 to get the 2nd length-field for the header. Add 8 to get the 1st length field for the header.
	uint32_t datalen = (uint32_t) (datlast-dat.begin());
	
	// 2.) cut to slices -----------------------------------------------------------------
	uint32_t lengthfield2 = datalen + 12;
	uint32_t lengthfield1 = datalen + 20;

	int numslices = (int)datalen / 210; // Number of 210-byte slices
	int lastlen   = (int)datalen % 210; // Data left for last frame containing the rest
	TRACE_V_THR30IIPEDAL(Serial.print("Datalen: "); Serial.println(datalen);)
	TRACE_V_THR30IIPEDAL(Serial.print("Number of slices: "); Serial.println(numslices);)
	TRACE_V_THR30IIPEDAL(Serial.print("Length of last slice: "); Serial.println(lastlen);)

	// 3.) Create the header --------------------------------------------------------------
	//    SysExStart:  f0 00 01 0c 22 02 4d (always the same)        (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 put following 4-Byte values together:
	//
	//                 0d 00 00 00   (Opcode for memory write/send)
	
	// Macro for converting a 32-Bit value to a 4-byte vector<byte, 4> and append it to "sb"
	#define conbyt4s(x) toInsert4 = { (byte)(x), (byte)((x)>>8), (byte)((x)>>16), (byte)((x)>>24) }; sblast = std::copy(std::begin(toInsert4), std::end(toInsert4), sblast);

	// sblast starts at sb.begin()
	conbyt4s(0x0du);
	//                 (len +8+12)   (1st length field = total length = data length + 3 values + type field/netto length )
	conbyt4s(lengthfield1);
	//                 FF FF FF FF   (number of user patch to overwrite / 0xFFFFFFFF for actual patch)
  // TODO: Write to user presets (0-4) when deactivating patches...
	conbyt4s(0xFFFFFFFFu);
	//                 (len   +12)   (2nd length field = netto length = data length + the following 3 values)
	conbyt4s(lengthfield2);
	//                 00 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x0u);
	//                 01 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x1u);
	//                 00 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x0u);
	//
	// Bitbucket this group (gives no incomplete 8/7 group!)
	sb2last = Enbucket(sb2, sb, sblast); 
	//                 And append it to the header
	//                 Finish header with 0xF7
	//                 Header frame may already be sent out!
	byte memframecnt = 0x06;
	//                 Get actual counter for memory frames and append it here

	//                 00 01 0b  ("same frame counter" and payload size  for the header)
	sblast = sb.begin(); // Start the buffer to send
	
	std::array<byte, 12> toInsert;
	toInsert = std::array<byte,12>({ 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt++, 0x00, 0x01, 0x0b });
	//toInsert=std::array<byte,12>({ 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt++, 0x00, 0x01, 0x0b });
	sblast = std::copy(toInsert.begin(), toInsert.end(), sb.begin());
	sblast = std::copy(sb2.begin(), sb2last, sblast);

	*sblast ++= 0xF7; // SysEx end demarkation

	SysExMessage m(sb.data(), sblast - sb.begin());
	Outmessage om(m, 100, false, false); // no Ack, no answer for the header 
	outqueue.enqueue(om); // Send header to THRxxII

	// 4.) For each slice: ------------------------------------------------------------
	//     Bitbucket encode the slice
	
	for( int i = 0; i < numslices; i++ )
	{
		std::array<byte, 210> slice;
		std::copy(dat.begin() + i*210, dat.begin() + i*210+210, slice.begin());
		sb2last = Enbucket(sb2, slice, slice.end());
		sblast = sb.begin(); // sStart new buffer to send
		toInsert =  { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt, (byte)(i % 128), 0x0d, 0x01 }; // memframecount is not incremented inside sent patches
		//toInsert=  { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt, (byte)(i % 128), 0x0d, 0x01 };  //memframecount is not incremented inside sent patches
		sblast = std::copy(toInsert.begin(), toInsert.end(), sblast);
		sblast = std::copy( sb2.begin(), sb2last, sblast);
		*sblast ++= 0xF7;

		// Print whole buffer as a chain of HEX
		//TRACE_V_THR30IIPEDAL(Serial.printf("\n\rFrame %d to send in \"createpatch\":\n\r",i);
		// 		hexdump(sb,sblast-sb.begin());
		//)
    // NOTE: Run-time crash if m or om are reused!
    //m = SysExMessage(sb.data(), sblast - sb.begin() );
    //om = Outmessage(m, (uint16_t)(101 + i), false, false);  //no Ack, for all the other slices
    SysExMessage m(sb.data(), sblast - sb.begin());
    Outmessage om(m, (uint16_t)(101 + i), false, false); // no Ack, for all the other slices
    outqueue.enqueue(om); // Send slice to THRxxII
	}

	if( lastlen > 0)  // Last slice (could be the first, if it is the only one)
	{
		std::array<byte, 210> slice; // Here 210 is a maximum size, not the true element count
		byte *slicelast = slice.begin();
		slicelast = std::copy(dat.begin() + numslices * 210, dat.begin() + numslices * 210 + lastlen, slicelast);
		sb2last = Enbucket(sb2, slice, slicelast);
		sblast = sb.begin(); // Start new buffer to send
		toInsert = { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt++, (byte)((numslices) % 128), (byte)((lastlen - 1) / 16), (byte)((lastlen - 1) % 16) }; 
		//toInsert={ 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt++, (byte)((numslices) % 128), (byte)((lastlen - 1) / 16), (byte)((lastlen - 1) % 16) }; 
		sblast = std::copy(toInsert.begin(),toInsert.end(), sblast);
		sblast = std::copy(sb2.begin(), sb2last, sblast);
		*sblast ++= 0xF7;
		
    // Print whole buffer as a chain of HEX
		// TRACE_V_THR30IIPEDAL( Serial.println(F("\n\rLast frame to send in \"createpatch\":"));
		//  		                  hexdump(sb,sblast-sb.begin());
	  //                   	)
    // NOTE: Run-time crash if m or om are reused!
		//m =  SysExMessage(sb.data(),sblast-sb.begin());
		//om = Outmessage(m, (uint16_t)(101 + numslices), true, false);  //Ack, but no answer for the header 
    SysExMessage m(sb.data(), sblast - sb.begin());
    Outmessage om(m, (uint16_t)(101 + numslices), true, false);  //Ack, but no answer for the header     
		outqueue.enqueue(om);  //send last slice to THRxxII
	}
	
	TRACE_THR30IIPEDAL(Serial.println(F("\n\rCreate_patch(): Ready outsending."));)

	userSettingsHaveChanged = false;
	//activeUserSetting = -1; // Commented to fix the issue of not showing the user preset number when in UI_HOME_AMP

	// Now we have to await acknowledgment for the last frame
	// on_ack_queue.enqueue(std::make_tuple(101+numslices,Outmessage(SysExMessage((const byte[29]) { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x3c, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0xf7 },29), 
	//                      101+numslices+1, false, true),nullptr,false)); //answer will be the settings dump for actual settings

  // Summary of what we did just now:
	//                 Building the SysEx frame:
	//                 f0 00 01 0c 22 02 4d(always the same)       (24 01 for THR10ii-W)
	//                 01   (memory command, not a settings command)
	//                 Get actual counter for memory frames and append it here
	//                 append same frame counter (is the slice counter) as a byte
	//                 set length field to  0d 01  (except for last frame, where we use it's stored length before bitbucketing)
	//                 append "bitbucketed" data for this slice
	//                 finish with 0xF7
	//                 Send it out.

} //End of THR30II_Settings::createPatch()

byte THR30II_Settings::UseSysExSendCounter() // Returns the actual counter value and increments it afterwards
{
  return (SysExSendCounter++) % 0x80;  // Cycles to 0, if more than 0x7f
}

void THR30II_Settings::setActiveUserSetting(int8_t val) // Setter for number of the active user setting
{
	activeUserSetting = val;
}

int8_t THR30II_Settings::getActiveUserSetting() // Getter for number of the active user setting
{
	return activeUserSetting;
}

bool THR30II_Settings::getUserSettingsHaveChanged() // Getter for state of user Settings
{
	return userSettingsHaveChanged;
}

void THR30II_Settings::setUserSettingsHaveChanged(bool changed) // Setter for state of user Settings
{
	userSettingsHaveChanged = changed;
}

// Tokens to find inside the patch data
std::map<String, std::vector<byte>> THR30II_Settings::tokens 
{
	{{"StructOpen"},  { 0x00, 0x00, 0x00, 0x80, 0x02, 0x00 }},
	{{"StructClose"}, { 0x02, 0x00, 0x00, 0x80, 0x00, 0x00 }},
	{{"UnitOpen"},    { 0x03, 0x00, 0x00, 0x80, 0x07, 0x00 }},
	{{"UnitClose"},   { 0x04, 0x00, 0x00, 0x80, 0x00, 0x00 }},
	{{"Data"},        { 0x01, 0x00, 0x00, 0x00, 0x01, 0x00 }},
	{{"Meta"}, 		    { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 }},
	{{"TokenMeta"},	  { 0x00, 0x80, 0x02, 0x00, 0x50, 0x53, 0x52, 0x50 }},
	{{"TokenData"},   { 0x00, 0x80, 0x02, 0x00, 0x54, 0x52, 0x54, 0x47 }},
	{{"UnitType"},	  { 0x00, 0x00, 0x05, 0x00 }},
	{{"ParCount"},	  { 0x00, 0x00, 0x06, 0x00 }},
	{{"PseudoVal"},	  { 0x00, 0x80, 0x07, 0x00 }},   
	{{"PseudoType"},  { 0x00, 0x80, 0x02, 0x00 }}
};

static std::map<uint16_t, string_or_ulong> globals; // Global settings from a patch-dump
std::map<uint16_t, Dumpunit> units; // Ushort value for UnitKey
uint16_t actualUnitKey = 0, actualSubunitKey = 0;
static String logg; // The log while analyzing dump

/////////////////////////////////////////////////////////////////////////////////////
// patchSetAll()
/////////////////////////////////////////////////////////////////////////////////////

// Helper for "patchSetAll()"
// Check key and following token, if in state "Structure"
static THR30II_Settings::States checkKeyStructure(byte key[], byte *buf, int buf_len, uint16_t &pt)
{
	if( pt + 6 > buf_len - 8 ) // If no following octet fits in the buffer
	{
		logg += "Error: Unexpected end of buffer while in Structure context!\n"; 
		return THR30II_Settings::States::St_error; // Stay in state Structure
	}

	byte next_octet[8];

	memcpy(next_octet, buf+pt+6, 8); // Fetch following octet from buffer

	if( memcmp(key, THR30II_Settings::tokens["Data"].data(), 6) == 0 )
	{
		logg += "Token Data found. ";
		if( memcmp(next_octet, THR30II_Settings::tokens["TokenData"].data(), 8) == 0 )
		{
			logg += "Token Data complete. Data:\n\r";
			pt += 8;
			return THR30II_Settings::States::St_data;
		}
	}
	else if( memcmp(key, THR30II_Settings::tokens["Meta"].data(), 6) == 0 )
	{
		logg += "Token Meta found. ";
		if( memcmp(next_octet, THR30II_Settings::tokens["TokenMeta"].data(), 8) == 0 )
		{
			logg += "Token Meta complete. Global:\n\r";
			pt += 8;
			return THR30II_Settings::States::St_global;
		}
	}
	logg += " ---> Structure:\n";
	return THR30II_Settings::States::St_structure;
}

// Helper for "patchSetAll()"
// check key, if in state "Data"
THR30II_Settings::States checkKeyData(byte key[], byte *buf, int buf_len, uint16_t &pt)
{
	if( pt + 6 > buf_len ) // If no token follows (end of buffer)
	{
	  logg += "Error: Unexpected end of buffer while in Data-Context!\n\r";
		return THR30II_Settings::States::St_error;
	}

	if( memcmp(key, THR30II_Settings::tokens["UnitOpen"].data(), 6) == 0 )
	{
		logg += "token UnitOpen found.  Unit:\n\r";
		return THR30II_Settings::States::St_unit;
	}
	else if( memcmp(key, THR30II_Settings::tokens["StructClose"].data(), 6) == 0 ) // This will perhaps never happen (Empty Data-Section)
	{
		logg += "Token StructClose found.  Idle:\n\r";
		return THR30II_Settings::States::St_idle;
	}
	logg += "Data:\n\r";
	return THR30II_Settings::States::St_data;
}

// Helper for "patchSetAll()"
// Check key and eventually following token, if in state "Unit"
THR30II_Settings::States checkKeyUnit(byte key[], byte *buf,int buf_len, uint16_t &pt)
{
	if( pt + 6 > buf_len ) // if buffer ends after this buffer
	{
		logg += "Error: Unexpected end of buffer while in a Unit!\n\r";
		return THR30II_Settings::States::St_error;
	}

	if( memcmp(key, THR30II_Settings::tokens["UnitOpen"].data(), 6) == 0 )
	{
		logg += "token UnitOpen found. State Subunit:\n\r";
		return THR30II_Settings::States::St_subunit;
	}
	else if( memcmp(key+2, THR30II_Settings::tokens["UnitType"].data(), 4) == 0 )
	{
		logg += "token UnitType found. ";
		uint16_t unitKey = (uint16_t)(key[0] + 256 * key[1]);
		if( pt + 6 > buf_len - 8 ) // if no following octet fits in the buffer
		{
			logg += "Error: Unexpected end of buffer while in Unit " + String(unitKey) + " !\n\r";
			return THR30II_Settings::States::St_error;
		}
		pt += 10;
		uint16_t unitType = (uint16_t)(buf[pt]      + 256 * buf[pt +  1]);
		uint16_t parCount = (uint16_t)(buf[pt + 10] + 256 * buf[pt + 11]);

		std::map<uint16_t, key_longval> emptyvals;
		std::map<uint16_t, Dumpunit> emptysubUnits;

		Dumpunit actualUnit =
		{
			unitType,
			parCount,
			emptyvals,      // Values
			emptysubUnits   // SubUninits
		};

		units.emplace(std::pair<uint16_t, Dumpunit>(unitKey, actualUnit)); // Store actual unit as a top level unit
		pt += 8;

		actualUnitKey = unitKey;
		logg += "State ValuesUnit:\n\r";
		return THR30II_Settings::States::St_valuesUnit;
	}
	else if( memcmp(key, THR30II_Settings::tokens["UnitClose"].data(), 6) == 0 )
	{
		logg += "token UnitClose found. State Data:\n\r";
		return THR30II_Settings::States::St_data; // Should only happen at the end of last unit
	}
	logg += "State Error:\n\r";
	return THR30II_Settings::States::St_error;
}

// Helper for "patchSetAll()"
// Check key and eventually following token, if in state "SubUnit"
THR30II_Settings::States checkKeySubunit(byte key[], byte *buf, int buf_len, uint16_t &pt)
{
	if( pt + 6 > buf_len ) // if buffer ends after this unit
	{
		logg += "Error: Unexpected end of buffer while in a SubUnit!\n\r";
		return THR30II_Settings::States::St_error;
	}

	if( memcmp(key, THR30II_Settings::tokens["UnitOpen"].data(), 6) == 0 )
	{
		logg += "token UnitOpen found.  State Error:\n\r";
		return THR30II_Settings::States::St_error;  // No Sub-SubUnits possible
	}
	else if( memcmp(key+2, THR30II_Settings::tokens["UnitType"].data(), 4) == 0 )
	{
		logg += "token UnitType found. ";
		uint16_t unitKey = (int16_t)(key[0] + 256 * key[1]);
		if( pt + 6 > buf_len - 8 ) // if no following octet fits in buffer
		{
			logg += "Error: Unexpected end of buffer while in SubUnit " + String(unitKey) + " !\n\r";
			return THR30II_Settings::States::St_error;
		}
		pt += 10;
		uint16_t unitType = (uint16_t)(buf[pt]      + 256 * buf[pt +  1]);
		uint16_t parCount = (uint16_t)(buf[pt + 10] + 256 * buf[pt + 11]);

		std::map<uint16_t, key_longval> emptyVals;
		std::map<uint16_t, Dumpunit> emptysubUnits;

		Dumpunit actualSubunit 
		{
			unitType,
			parCount,
		  emptyVals,    // Values 
			emptysubUnits // SubUnits
		};

		units[actualUnitKey].subUnits.emplace(unitKey, actualSubunit);

		pt += 8;
		actualSubunitKey = unitKey;
		logg += "State ValuesSubunit:\n\r";
		return THR30II_Settings::States::St_valuesSubunit;
	}
	else if( memcmp(key, THR30II_Settings::tokens["UnitClose"].data(), 6) == 0 )
	{
		logg += "token UnitClose found.  Unit:\n\r";
		actualSubunitKey = 0xFFFF; // Not in a subunit any more
		return THR30II_Settings::States::St_unit; // Should only happen at the end of last unit
	}
	logg += "Error: No allowed trigger in State SubUnit:\n\r";
	return THR30II_Settings::States::St_error;
}

// Get data, when in state "Global" - Helper for "patchSetAll()"
// Fetch one global setting from the patch dump (called several times, if there are several global values)
static void getGlobal(byte *buf, int buf_len, uint16_t &pt)  
{
	uint16_t lfdNr = (uint16_t)buf[pt + 0] + 256 * (uint16_t)buf[pt + 1];

	byte type = buf[pt + 4]; // Get type code for the global value

	if( type == 0x04 ) // Type string (the patch name)
	{
		byte len = buf[pt + 6]; // Ignore 3 High Bytes, because string is always shorter than 255 
		logg.append(String("found string of len ")+String((int)len)+String(" : "));

		String name;

		if( len > 0 )
		{
			if( len > 64 )
			{
			  char tmp[len];
				memcpy(tmp, buf+pt+10, len-1);
				name = String(tmp).substring(0, 64);
			}
			else
			{
				char tmp[len];
				memcpy(tmp, buf+pt+10, len-1);
				name = String(tmp);
			}
      logg.append(name + String("\n\r") );
		}

		globals.emplace(std::pair< uint16_t, string_or_ulong>(lfdNr, (string_or_ulong){true, type, name, 0}));

		pt += (uint16_t)len + 4;
	}
	else if( type == 0x02 || type == 0x03 )
	{
		globals.emplace(std::pair<uint16_t,string_or_ulong> ( lfdNr, 
		                                                      (string_or_ulong) {false, type,
														                            	(uint32_t)buf[pt + 6]        + ((uint32_t)buf[pt + 7] <<  8)
		                                                   + ((uint32_t)buf[pt + 8] << 16) + ((uint32_t)buf[pt + 9] << 24)} ));
		pt += 4;
	}
	else
	{
		logg.append("unknown type code " + String(type) + " for global value!\n\r");
		pt += 4;
	}
}

// Get data, when in state "Unit" - Helper for "patchSetAll()"
// Get one of the units' settings from the patch dump (called several times, if there are several values)
THR30II_Settings::States getValueUnit(byte key[], byte *buf, int buf_len, uint16_t &pt) 
{
	if( memcmp(key, THR30II_Settings::tokens["UnitOpen"].data(), 6) == 0 )
	{
		logg += "token UnitOpen found.  SubUnit:\n\r";
		return THR30II_Settings::States::St_subunit;
	}
	else if( memcmp(key, THR30II_Settings::tokens["UnitClose"].data(), 6) == 0 )
	{
		logg += "token UnitClose found. Data:\n\r";
		return THR30II_Settings::States::St_data;
	}
	// No Open or Close => should be a value
	
	if( pt + 6 > buf_len - 4 ) // if no following quartet fits in the buffer
	{
		logg += "Error: Unexpected end of buffer while in ValueUnit context!\n\r"; 
		return THR30II_Settings::States::St_error;  //Stay in state Structure
	}

	uint16_t parKey = (uint16_t)(key[0] + 256 * key[1]); // Get the key for this value

	byte type = buf[pt + 4]; // Get the type key for the following 4-Byte-value
	// Get the 4-byte-value itself
	uint32_t val = (uint32_t)(((uint32_t)buf[pt + 6]) + ((uint32_t)buf[pt + 7] << 8) + ((uint32_t)buf[pt + 8] << 16) + ((uint32_t)buf[pt + 9] << 24));
	units[actualUnitKey].values.emplace(parKey, (key_longval) {type, val} );
	pt += 4;
	// Stay in context ValueUint to read further value(s)
	logg += "ValuesUnit:\n\r";
	return THR30II_Settings::States::St_valuesUnit;
}

// Helper for "patchSetAll()"
// Checks, if there is a value in the current subUnti and extract this value (4-Byte)
THR30II_Settings::States getValueSubunit(byte key[], byte *buf, int buf_len, uint16_t &pt)
{
	if( memcmp(key, THR30II_Settings::tokens["UnitOpen"].data(), 6) == 0 )
	{
		logg += "token UnitOpen found.  Unit:\n\r";
		return THR30II_Settings::States::St_error; // No Sub-SubUnits possible
	}
	else if( memcmp(key, THR30II_Settings::tokens["UnitClose"].data(), 6) == 0 )
	{
		logg += "token UnitClose in Subunit found.  Unit:\n\r";
		actualSubunitKey = 0xFFFF; // No Subunit any more
		return THR30II_Settings::States::St_unit;
	}
	
	// No Open or Close => should be a value
	if( pt + 6 > buf_len - 4 ) // If no following quartet fits in the buffer
	{
		logg += "Error: Unexpected end of buffer while in ValueSubUnit context!\n\r"; 
		return THR30II_Settings::States::St_error;  
	}

	uint16_t parKey = (uint16_t)(key[0] + 256 * key[1]); // Get the key for this value
	
	byte type = buf[pt + 4]; // Get the type key for the following 4-Byte-value
	// Get the 4-byte-value itself
	uint32_t val = (uint32_t)(((uint32_t)buf[pt + 6]) + ((uint32_t)buf[pt + 7] << 8) + ((uint32_t)buf[pt + 8] << 16) + ((uint32_t)buf[pt + 9] << 24));
	units[actualUnitKey].subUnits[actualSubunitKey].values.emplace(parKey, (key_longval) {type, val});
	pt += 4;
	// Stay in context ValueSubUnit to read further value(s)
	logg += "ValuesSubunit:\n\r";
	return THR30II_Settings::States::St_valuesSubunit;
}

// Extract all settings from a received dump. returns error code
int THR30II_Settings::patch_setAll(uint8_t * buf, uint16_t buf_len) 
{
	uint16_t pt = 0; // Index
	_state = States::St_idle;
	globals.clear();
	units.clear();
	actualUnitKey = 0; 
  actualSubunitKey = 0;

	// Setting up a data structure to keep the retrieved dump data
	logg = String();
	
	// Overwrite locally stored patch values with received buffer's data
	
	// State "idle"
	logg.append("Idle:\n\r");

	while( (pt <= buf_len - 6) && _state != St_error ) // Walk through the patch data and look for token-sextetts
	{
		byte sextet[6];
		memcpy(sextet, buf+pt, 6); // Fetch token sextet from buffer
		char tmp[36];
		sprintf(tmp, "Byte %d of %d : %02X%02X%02X%02X%02X%02X : ", pt, buf_len, sextet[0], sextet[1], sextet[2], sextet[3], sextet[4], sextet[5]);
		logg.append(tmp);
		
		if( _state == States::St_idle )
		{
			// From Idle-state we can reach the Structure-state 
			// if we send the corresponding key as a trigger.
			if( memcmp(sextet, THR30II_Settings::tokens["StructOpen"].data(), 6) == 0 )
			{ 
				// Reaches state "Structure"
				logg.append("Structure:\n\r");					
				_state = States::St_structure;
			}
		}
		else if( _state == States::St_structure )
		{
			// Switching options from this state on
			_state = checkKeyStructure(sextet, buf, buf_len, pt);
		}
		else if( _state == States::St_data )
		{
			// Switching options from this state on
			_state = checkKeyData(sextet, buf, buf_len, pt);
		}
		else if( _state == States::St_meta )
		{
			// Switching options from this state on
			if( memcmp(sextet, THR30II_Settings::tokens["StructClose"].data(), 6) == 0 )
			{ 
				// Reaches state "Idle"
				logg.append("Idle:\n\r");
				_state = St_idle;
			}
			else
			{
				// Reaches state "Global"
				logg.append("Global:\n\r");
				_state = St_global;
			}
		}
		else if( _state == States::St_global )
		{
			// Switching options from this state on
			if( memcmp(sextet, THR30II_Settings::tokens["StructClose"].data(), 6) == 0 )
			{
				// Reaches state"Idle"
				logg.append("Idle:\n\r");
					_state = St_idle;
			}
			else
			{ 
				getGlobal(buf, buf_len, pt);
				
				// Reaches state "Global"
				logg.append("Global:\n\r");
				_state = St_global;
			}
		}
		else if( _state == States::St_unit )
		{
			// Switching options from this state on
			_state = checkKeyUnit(sextet, buf, buf_len, pt);
		}
		else if( _state == States::St_valuesUnit )
		{
			// Switching options from this state on
			_state = getValueUnit(sextet, buf, buf_len, pt);
		}
		else if( _state == States::St_subunit )
		{
			// Switching options from this state on
			_state = checkKeySubunit(sextet, buf, buf_len, pt);
		}
		else if (_state == States::St_valuesSubunit)
		{
			// Switching options from this state on
			_state = getValueSubunit(sextet, buf, buf_len, pt);
		}
		pt += 6; // Advance in buffer
	}

	TRACE_THR30IIPEDAL(Serial.println("patch_setAll parsing ready - results: ");)
	TRACE_V_THR30IIPEDAL(Serial.println(logg);)     
	TRACE_THR30IIPEDAL(Serial.println("Setting the "+ String(globals.size())+" globals: ");)

	// Walk through the globals (Structur Meta)
	for( std::pair<uint16_t, string_or_ulong> kvp: globals )
	{
		if( kvp.first == 0x0000 )
		{
			if( kvp.second.isString )
			{
				SetPatchName(kvp.second.nam, -1);
			}
		}
		else if( kvp.first == 0x0001 )
		{
			if( !kvp.second.isString ) // int
			{
				Tnid = kvp.second.val;
			}
		}
		if( kvp.first== 0x0002 )
		{
			if( !kvp.second.isString )
			{
				UnknownGlobal = kvp.second.val;
			}
		}
		if( kvp.first==  0x0003 )
		{
			if( !kvp.second.isString )
			{
				ParTempo = kvp.second.val;
			}
		}
	}

  // Switch GUI to THR amp mode
  _uistate = UI_home_amp;

	TRACE_THR30IIPEDAL(Serial.println("... setting unit vals: ");)
	// Recurse through the whole data structure created while parsing the dump
	
	for( std::pair<uint16_t, Dumpunit> du: units ) // for each!
	{
		if( du.first == THR30II_UNITS_VALS[GATE].key ) // Unit Gate also hosts MIX and Subunits COMP...REV
		{
			TRACE_V_THR30IIPEDAL(Serial.println("In dumpunit GATE/AMP");)

			if( du.second.parCount != 0 )
			{
				std::map<uint16_t, Dumpunit> &dict2 = du.second.subUnits; // Get SubUnits of GATE/MIX

				for(std::pair<uint16_t, Dumpunit> kvp: dict2) // for each
				{
					if( kvp.first == THR30II_UNITS_VALS[ECHO].key ) // If SubUnit "Echo"
					{
						TRACE_V_THR30IIPEDAL(Serial.println("In dumpSubUnit ECHO");)

						// Before we set Parameters we have to select the Echo-Type
						uint16_t t = kvp.second.type;

						auto result = std::find_if( THR30II_ECHO_TYPES_VALS.begin()
							                        , THR30II_ECHO_TYPES_VALS.end()
							                        , [t](const auto& mapobj) 
                                      { return mapobj.second.key == t; }
						);

						if( result != THR30II_ECHO_TYPES_VALS.end() )
						{
							EchoSelect(result->first);
						}
						else // Defaults to TAPE_ECHO
						{
							EchoSelect(THR30II_ECHO_TYPES::TAPE_ECHO);
						}

						for( std::pair<uint16_t, key_longval> p: kvp.second.values) // Values contained in SubUnit "Echo"
						{
							uint16_t key = EchoMap(p.first); // Map the dump-keys to the MIDI-Keys 
							EchoSetting(key, NumberToVal(p.second.val));
						}
					}
					else if( kvp.first== THR30II_UNITS_VALS[EFFECT].key ) // If SubUnit "Effect"
					{
						TRACE_V_THR30IIPEDAL(Serial.println("In dumpSubUnit EFFECT");)

						// Before we set Parameters we have to select the Effect-Type
						uint16_t t = kvp.second.type;

						auto result = std::find_if( THR30II_EFF_TYPES_VALS.begin()
							                        , THR30II_EFF_TYPES_VALS.end()
							                        , [t](const auto& mapobj)
                                      { return mapobj.second.key == t; }
						);

						if( result != THR30II_EFF_TYPES_VALS.end() )
						{
							EffectSelect(result->first);
						}
						else // Defaults to PHASER
						{
							EffectSelect(THR30II_EFF_TYPES::PHASER);
						}

						for( std::pair<uint16_t, key_longval> p: kvp.second.values ) // Values contained in SubUnit "Effect"
						{
							uint16_t key = EffectMap(p.first); // Map the dump-keys to the MIDI-Keys 
							EffectSetting(key, NumberToVal(p.second.val));
						}
					}
					else if( kvp.first == THR30II_UNITS_VALS[COMPRESSOR].key ) // If SubUnit "Compressor"
					{
						TRACE_V_THR30IIPEDAL(Serial.println("In dumpSubUnit COMPRESSOR");)

						for( std::pair<uint16_t, key_longval> p: kvp.second.values ) // Values contained in SubUnit "Compressor"
						{
							uint16_t key = CompressorMap(p.first); // Map the dump-keys to the MIDI-Keys

							// Find Setting for this key
							auto result = std::find_if( THR30II_COMP_VALS.begin()
								                        , THR30II_COMP_VALS.end()
								                        , [key](const auto& mapobj)
                                        { return mapobj.second == key; }
							);

							if( result != THR30II_COMP_VALS.end() )
							{
								CompressorSetting(result->first, NumberToVal(p.second.val));
							}
							else // Defaults to CO_SUSTAIN
							{
								CompressorSetting(CO_SUSTAIN, NumberToVal(p.second.val));
							}
						}
					}
					else if( kvp.first == THR30II_UNITS_VALS[THR30II_UNITS::REVERB].key ) // If SubUnit "Reverb"
					{
						TRACE_V_THR30IIPEDAL(Serial.println("In dumpSubUnit REVERB");)

						// Before we set Parameters we have to select the Reverb-Type
						uint16_t t = kvp.second.type;

						auto result = std::find_if( THR30II_REV_TYPES_VALS.begin()
							                        , THR30II_REV_TYPES_VALS.end()
							                        , [t](const auto& mapobj) 
                                      { return mapobj.second.key == t; }
						);

						if( result != THR30II_REV_TYPES_VALS.end() )
						{
							ReverbSelect(result->first);
						}
						else // Defaults to SPRING
						{
							ReverbSelect(SPRING);
						}

						for( std::pair<uint16_t, key_longval> p: kvp.second.values ) // Values contained in SubUnit "Reverb"
						{
							uint16_t key = ReverbMap(p.first); // Map the dump-keys to the MIDI-Keys 
							ReverbSetting(key, NumberToVal(p.second.val));
						}
					}
					else if( kvp.first == THR30II_UNITS_VALS[THR30II_UNITS::CONTROL].key ) // If SubUnit "Control/Amp"
					{
						TRACE_V_THR30IIPEDAL(Serial.println("In dumpSubUnit CTRL/AMP");)
						setColAmp(kvp.second.type);
						
						for( std::pair<uint16_t, key_longval> p: kvp.second.values ) // Values contained in SubUnit "Amp"
						{
							uint16_t key = controlMap[p.first]; // Map the dump-keys to the MIDI-Keys
							
							// Find Setting for this key
							auto result = std::find_if( THR30II_CTRL_VALS.begin()
                                        ,	THR30II_CTRL_VALS.end()
                                        ,	[key](const auto& mapobj)
                                        { return mapobj.second == key; }
							);

							double val=0.0;
							if( result != THR30II_CTRL_VALS.end() )
							{   
								val = NumberToVal(p.second.val);
							  TRACE_THR30IIPEDAL(Serial.printf("Setting main control %d = %.1f\n\r", ((std::_Rb_tree_iterator<std::pair<const THR30II_CTRL_SET,uint16_t>>) result)->first, val);)
								SetControl(result->first, val);
							}
							else // Defaults to CTRL_GAIN
							{
								TRACE_THR30IIPEDAL(Serial.printf("Setting main control \"Gain\" (default) = %.1f\n\r", val);)
								SetControl(THR30II_CTRL_SET::CTRL_GAIN, NumberToVal(p.second.val));
							}
						}
					}
				} // End of foreach kvp in dict2 - Subunits of GuitarProc (Gate) an their params
				
				std::map<uint16_t, key_longval> &dict = du.second.values; // Values -directly- contained in Unit GATE/MIX

				for( std::pair<uint16_t, key_longval> kvp: dict )
				{
					if     ( unitOnMap[kvp.first] == THR30II_UNIT_ON_OFF_COMMANDS[EFFECT]      ) { Switch_On_Off_Effect_Unit(kvp.second.val     != 0); }
					else if( unitOnMap[kvp.first] == THR30II_UNIT_ON_OFF_COMMANDS[ECHO]        ) { Switch_On_Off_Echo_Unit(kvp.second.val       != 0); }
					else if( unitOnMap[kvp.first] == THR30II_UNIT_ON_OFF_COMMANDS[REVERB]      ) { Switch_On_Off_Reverb_Unit(kvp.second.val     != 0); }
					else if( unitOnMap[kvp.first] == THR30II_UNIT_ON_OFF_COMMANDS[COMPRESSOR]  ) { Switch_On_Off_Compressor_Unit(kvp.second.val != 0); }
					else if( unitOnMap[kvp.first] == THR30II_UNIT_ON_OFF_COMMANDS[GATE]        ) { Switch_On_Off_Gate_Unit(kvp.second.val       != 0); }

					else if( gateMap[kvp.first]   == THR30II_GATE_VALS[GA_DECAY]               ) { gate_setting[GA_DECAY]     = NumberToVal(kvp.second.val); }
					else if( gateMap[kvp.first]   == THR30II_GATE_VALS[GA_THRESHOLD]           ) { gate_setting[GA_THRESHOLD] = NumberToVal_Threshold(kvp.second.val); }

					else if( reverbMap[kvp.first] == THR30II_INFO_REVERB[reverbtype]["MIX"].sk ) { ReverbSetting(reverbtype, reverbMap[kvp.first], NumberToVal(kvp.second.val)); }
					else if( effectMap[kvp.first] == THR30II_INFO_EFFECT[effecttype]["MIX"].sk ) { EffectSetting(effecttype, effectMap[kvp.first], NumberToVal(kvp.second.val)); }
					else if( echoMap[kvp.first]   == THR30II_INFO_ECHO[echotype]["MIX"].sk     ) { EchoSetting(echotype,     echoMap[kvp.first],   NumberToVal(kvp.second.val)); }

					else if( compressorMap[kvp.first] == THR30II_COMP_VALS[CO_MIX]             ) { CompressorSetting(CO_MIX, NumberToVal(kvp.second.val) ); }

					else if( kvp.first == THR30II_CAB_COMMAND_DUMP                             ) { SetCab((THR30II_CAB)kvp.second.val); }
					// Note 0x0120: AMP_EnableState (not used in patches to THRII) but occurs in dumps from THRII to PC
					else if( kvp.first == Constants::glo["AmpEnableState"]                     ) { TRACE_THR30IIPEDAL(Serial.printf("\"AmpEnableState\" %d.\n\r",kvp.second.val);) }
				} // of for (each) kvp in dict

			} // end of if count params !=0
		} // end of if "Unit GATE" (contains COMP...REV as subunits)
	} // end of foreach dumpunit

	return 0; // Success
} // end of THR30II_settings::patch_setAll

///////////////////////////////////////////////////////////////////////
// The setters for locally stored THR30II-Settings class
///////////////////////////////////////////////////////////////////////

// void THR30II_Settings::SetAmp(uint8_t _amp) // No separate setter necessary at the moment
// {
// }

void THR30II_Settings::SetColAmp(THR30II_COL _col, THR30II_AMP _amp) // Setter for the Simulation Collection / Amp
{
//	bool changed = false;
	if (_col != col)
	{
		col = _col;
//		changed = true;
	}
	if (_amp != amp)
	{
		amp = _amp;
//		changed = true;
	}

//	if (changed)
//	{
//	  if (sendChangestoTHR)
//	 	{
//	 		SendColAmp();
//	 	}
//	}
}

void THR30II_Settings::setColAmp(uint16_t ca) // Setter by key
{	
  // Lookup Key for this value
	auto result = std::find_if( THR30IIAmpKeys.begin()
                            , THR30IIAmpKeys.end()
                            , [ca](const auto& mapobj)
                            { return mapobj.second == ca; }
  );

	// RETURN VARIABLE IF FOUND
	if( result != THR30IIAmpKeys.end() )
	{
		col_amp foundkey = result->first;	
		SetColAmp(foundkey.c, foundkey.a);
	}
}

void THR30II_Settings::SetCab(THR30II_CAB _cab) // Setter for the Cabinet Simulation
{
	cab = _cab;

	if( sendChangestoTHR )
	{
		SendCab();
	}
}

void THR30II_Settings::SetGuitarVol(uint16_t _g_vol)
{
    pedal_1_val = _g_vol;
}

void THR30II_Settings::SetAudioVol(uint16_t _a_vol)
{
    pedal_2_val = _a_vol;
}

void THR30II_Settings::EchoSetting(uint16_t ctrl, double value) // Setter for the Echo Parameters
{
	 EchoSetting(echotype, ctrl, value); // Using setter with actual selected echo type
}

void THR30II_Settings::EchoSetting(THR30II_ECHO_TYPES type, uint16_t ctrl, double value) // Setter for the Echo Parameters
{
	switch( type )
	{
		case TAPE_ECHO:
			THR30II_ECHO_SET_TAPE stt; // Find setting by it's key
			for(auto it = THR30II_INFO_TAPE.begin(); it != THR30II_INFO_TAPE.end(); it++)
			{
				if( it->second.sk==ctrl )
				{
					stt = it->first;
				}
			} 
			if( value >= THR30II_INFO_TAPE[stt].ll && value <= THR30II_INFO_TAPE[stt].ul )
			{
				echo_setting[TAPE_ECHO][stt] = value; // All double values
				// if( stt == TA_MIX ) // Mix is identical for both types!
				// {
				// 	 echo_setting[DIGITAL_DELAY][DD_MIX] = value;
				// }

				if( sendChangestoTHR )
				{
					SendParameterSetting(un_cmd {THR30II_INFO_TAPE[stt].uk, THR30II_INFO_TAPE[stt].sk} ,type_val<double> {0x04, value}); // Send settings change to THR
				}
			}
			break;
		case DIGITAL_DELAY:
			THR30II_ECHO_SET_DIGI std; // Find setting by it's key
			for(auto it = THR30II_INFO_DIGI.begin(); it != THR30II_INFO_DIGI.end(); it++)
			{
				if( it->second.sk == ctrl )
				{
					std = it->first;
				}
			}

			if( value >= THR30II_INFO_DIGI[std].ll && value <= THR30II_INFO_DIGI[std].ul )
			{
				echo_setting[DIGITAL_DELAY][std] = value; // All double values
				// if (std == DD_MIX) // Mix is identical for both types!
				// {
				// 	 echo_setting[TAPE_ECHO][TA_MIX] = value;
				// }

				if( sendChangestoTHR )
				{
					SendParameterSetting(un_cmd {THR30II_INFO_DIGI[std].uk, THR30II_INFO_DIGI[std].sk }, type_val<double> {0x04, value}); // Send settings change to THR
				}
			}
			break;
	}
}

void THR30II_Settings::GateSetting(THR30II_GATE ctrl, double value) // Setter for gate effect parameters
{
	gate_setting[ctrl] = constrain(value, 0, 100);

	if( sendChangestoTHR )
	{
		SendParameterSetting((un_cmd){THR30II_UNITS_VALS[GATE].key, THR30II_GATE_VALS[ctrl]},(type_val<double>) {0x04, value}); // Send reverb type change to THR
	}
}

void THR30II_Settings::CompressorSetting(THR30II_COMP ctrl, double value) // Setter for compressor effect parameters
{
	compressor_setting[ctrl] = constrain(value, 0, 100);

	if( sendChangestoTHR )
	{
		if( ctrl == THR30II_COMP::CO_SUSTAIN )
		{
			SendParameterSetting((un_cmd){THR30II_UNITS_VALS[COMPRESSOR].key, THR30II_COMP_VALS[CO_SUSTAIN]}, (type_val<double>){0x04, value}); // Send settings change to THR
		}
		else if( ctrl == THR30II_COMP::CO_LEVEL )
		{
			SendParameterSetting((un_cmd){THR30II_UNITS_VALS[COMPRESSOR].key, THR30II_COMP_VALS[CO_LEVEL]}, (type_val<double>){0x04, value}); // Send settings change to THR
		}
		else if( ctrl == THR30II_COMP::CO_MIX )
		{
			SendParameterSetting((un_cmd){THR30II_UNITS_VALS[GATE].key, THR30II_COMP_VALS[CO_MIX]}, (type_val<double>){0x04, value}); // Send settings change to THR
		}
	}
}

void THR30II_Settings::EffectSetting(uint16_t ctrl, double value) // Using the internally selected actual effect type
{
		EffectSetting(effecttype, ctrl, value);
}

void THR30II_Settings::EffectSetting(THR30II_EFF_TYPES type, uint16_t ctrl, double value) // Setter for Effect parameters by type and key
{
	switch( type )
	{
		case THR30II_EFF_TYPES::CHORUS:
			
				THR30II_EFF_SET_CHOR stc; // Find setting by it's key
				for(auto it = THR30II_INFO_CHOR.begin(); it != THR30II_INFO_CHOR.end(); it++)
				{
          if( it->second.sk == ctrl )
					{
					  stc = it->first;
					}
				}

				if( value >= THR30II_INFO_CHOR[stc].ll && value <= THR30II_INFO_CHOR[stc].ul )
				{
					effect_setting[CHORUS][stc] = value; // All double values
					// if(stc == CH_MIX) // MIX is identical for all types!
					// {
					// 	effect_setting[FLANGER][FL_MIX] = value;
					// 	effect_setting[PHASER][PH_MIX] = value;
					// 	effect_setting[TREMOLO][TR_MIX] = value;
					// }
					if( sendChangestoTHR )  // Do not send back if change results from THR itself
					{
						SendParameterSetting((un_cmd) { THR30II_INFO_CHOR[stc].uk, THR30II_INFO_CHOR[stc].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
					}
				}
			break;

		case THR30II_EFF_TYPES::FLANGER:

				THR30II_EFF_SET_FLAN stf; // Find setting by its key
				for(auto it = THR30II_INFO_FLAN.begin(); it != THR30II_INFO_FLAN.end(); it++)
				{
          if( it->second.sk == ctrl )
					{
				    stf = it->first;
				  }
				}

				if( value >= THR30II_INFO_FLAN[stf].ll && value <= THR30II_INFO_FLAN[stf].ul )
				{
					effect_setting[FLANGER][stf] = value; // All double values
					// if (stf == FL_MIX) // MIX is identical for all types!
					// {
					// 	effect_setting[CHORUS][CH_MIX] = value;
					// 	effect_setting[PHASER][PH_MIX] = value;
					// 	effect_setting[TREMOLO][TR_MIX] = value;
					// }					
					if( sendChangestoTHR ) // Do not send back, if change results from THR itself
					{
						SendParameterSetting((un_cmd){THR30II_INFO_FLAN[stf].uk, THR30II_INFO_FLAN[stf].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
					}
				}
			break;

		case THR30II_EFF_TYPES::TREMOLO:

			THR30II_EFF_SET_TREM stt; // Find setting by its key
			for(auto it = THR30II_INFO_TREM.begin(); it != THR30II_INFO_TREM.end(); it++)
			{
				if(it->second.sk==ctrl)
				{
					stt= it->first;
				}
			}
			
      if( value >= THR30II_INFO_TREM[stt].ll && value <= THR30II_INFO_TREM[stt].ul )
			{
				effect_setting[TREMOLO][stt] = value; // All double values
				//  if (stt == TR_MIX)   // MIX is identical for all types!
        //         {
        //             effect_setting[FLANGER][FL_MIX] = value;
        //             effect_setting[PHASER][PH_MIX] = value;
        //             effect_setting[CHORUS][CH_MIX] = value;
        //         }
				if( sendChangestoTHR ) // Do not send back, if change results from THR itself
				{
					SendParameterSetting((un_cmd){THR30II_INFO_TREM[stt].uk, THR30II_INFO_TREM[stt].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
				}
			}
		break;

		case THR30II_EFF_TYPES::PHASER:

			THR30II_EFF_SET_PHAS stp; // Find setting by its key
			for(auto it = THR30II_INFO_PHAS.begin(); it != THR30II_INFO_PHAS.end(); it++)
			{
        if( it->second.sk == ctrl )
        {
          stp = it->first;
        }
			}

			if( value >= THR30II_INFO_PHAS[stp].ll && value <= THR30II_INFO_PHAS[stp].ul )
			{
				effect_setting[PHASER][stp] = value; // All double values
				// if (stp == PH_MIX)   //MIX is identical for all types!
        //             {
        //                 effect_setting[FLANGER][FL_MIX] = value;
        //                 effect_setting[CHORUS][CH_MIX] = value;
        //                 effect_setting[TREMOLO][TR_MIX] = value;
        //             }
				if( sendChangestoTHR ) // Do not send back, if changes result from THR itself
				{
					SendParameterSetting((un_cmd){THR30II_INFO_PHAS[stp].uk, THR30II_INFO_PHAS[stp].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
				}
			}
		break;
	}
}

void THR30II_Settings::ReverbSetting(THR30II_REV_TYPES type, uint16_t ctrl, double value)
{
	switch( type )
	{
		case THR30II_REV_TYPES::SPRING:

			THR30II_REV_SET_SPRI sts; // Find setting by its key
			for(auto it = THR30II_INFO_SPRI.begin(); it != THR30II_INFO_SPRI.end(); it++)
			{
        if( it->second.sk == ctrl )
        {
          sts = it->first;
        }
			}

			if( value >= THR30II_INFO_SPRI[sts].ll && value <= THR30II_INFO_SPRI[sts].ul )
			{
				reverb_setting[SPRING][sts] = value; // All double values
				//  if(sts== SP_MIX)   //MIX is identical for all types!
        //             {
        //                 reverb_setting[PLATE][PL_MIX] = value;
        //                 reverb_setting[HALL][HA_MIX] = value;
        //                 reverb_setting[ROOM][RO_MIX] = value;
        //             }
				if( sendChangestoTHR )
				{
					SendParameterSetting((un_cmd){THR30II_INFO_SPRI[sts].uk, THR30II_INFO_SPRI[sts].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
				}
			}
			break;

		case THR30II_REV_TYPES::PLATE:

			THR30II_REV_SET_PLAT stp; // Find setting by its key
			for(auto it = THR30II_INFO_PLAT.begin(); it != THR30II_INFO_PLAT.end(); it++)
			{
					if(it->second.sk==ctrl)
					{
						stp= it->first;
					}
			}

			if( value >= THR30II_INFO_PLAT[stp].ll && value <= THR30II_INFO_PLAT[stp].ul )
			{
				reverb_setting[PLATE][stp] = value; // All double values
				//  if (stp == PL_MIX) // MIX is identical for all types!
        //             {
        //                 reverb_setting[SPRING][SP_MIX] = value;
        //                 reverb_setting[HALL][HA_MIX] = value;
        //                 reverb_setting[ROOM][RO_MIX] = value;
        //             }
				if( sendChangestoTHR )
				{
					SendParameterSetting((un_cmd){THR30II_INFO_PLAT[stp].uk, THR30II_INFO_PLAT[stp].sk}, (type_val<double>){0x04, value}); // Send settings change to THR
				}
			}
			break;

		case THR30II_REV_TYPES::HALL:
			
      THR30II_REV_SET_HALL sth; // Find setting by its key
			for(auto it = THR30II_INFO_HALL.begin(); it != THR30II_INFO_HALL.end(); it++)
			{
        if( it->second.sk == ctrl )
        {
          sth = it->first;
        }
			}

			if( value >= THR30II_INFO_HALL[sth].ll && value <= THR30II_INFO_HALL[sth].ul )
			{
				reverb_setting[HALL][sth] = value; // All double values
				// if (sth == HA_MIX) // MIX is identical for all types!
				// {
				// 	reverb_setting[SPRING][SP_MIX] = value;
				// 	reverb_setting[PLATE][PL_MIX] = value;
				// 	reverb_setting[ROOM][RO_MIX] = value;
				// }
				if( sendChangestoTHR )
				{
					SendParameterSetting((un_cmd){THR30II_INFO_HALL[sth].uk, THR30II_INFO_HALL[sth].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
				}
			}
			break;

		case THR30II_REV_TYPES::ROOM:
			
      THR30II_REV_SET_ROOM str; // Find setting by its key
			for(auto it = THR30II_INFO_ROOM.begin(); it != THR30II_INFO_ROOM.end(); it++)
			{
        if( it->second.sk == ctrl )
        {
          str = it->first;
        }
			}
			if( value >= THR30II_INFO_ROOM[str].ll && value <= THR30II_INFO_ROOM[str].ul )
			{
				reverb_setting[ROOM][str] = value; // All double values
				// if (str == RO_MIX) // MIX is identical for all types!
        //             {
        //                 reverb_setting[SPRING][SP_MIX] = value;
        //                 reverb_setting[PLATE][PL_MIX] = value;
        //                 reverb_setting[HALL][HA_MIX] = value;
        //             }
				if( sendChangestoTHR )
				{
					SendParameterSetting((un_cmd){THR30II_INFO_ROOM[str].uk, THR30II_INFO_ROOM[str].sk}, (type_val<double>) {0x04, value}); // Send settings change to THR
				}
			}
			break;
	}
}

void THR30II_Settings::ReverbSetting(uint16_t ctrl, double value)  // Using internally selected actual reverb type
{
	ReverbSetting(reverbtype, ctrl, value); // Use actual selected reverbtype
}

void THR30II_Settings::Switch_On_Off_Gate_Unit(bool state) // Setter for switching on / off the effect unit
{
	unit[GATE] = state;

	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendUnitState(GATE);
	}
}

void THR30II_Settings::Switch_On_Off_Echo_Unit(bool state) // Setter for switching on / off the Echo unit
{
	unit[ECHO] = state;

	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendUnitState(ECHO);
	}
}

void THR30II_Settings::Switch_On_Off_Effect_Unit(bool state) // Setter for switching on / off the effect unit
{
	unit[EFFECT] = state;

	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendUnitState(EFFECT);
	}
}

void THR30II_Settings::Switch_On_Off_Compressor_Unit(bool state) // Setter for switching on /off the compressor unit
{
	unit[COMPRESSOR] = state;

	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendUnitState(COMPRESSOR);
	}
}

void THR30II_Settings::Switch_On_Off_Reverb_Unit(bool state) // Setter for switching on / off the reverb unit
{
	unit[REVERB] = state;
	
	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendUnitState(REVERB);
	}
}

void THR30II_Settings::SetControl(uint8_t ctrl, double value)
{
//  Serial.println("SetControl()");
  control[ctrl] = value;
  if( sendChangestoTHR )
  {
    SendParameterSetting(un_cmd {THR30II_UNITS_VALS[CONTROL].key, THR30II_CTRL_VALS[(THR30II_CTRL_SET)ctrl] }, type_val<double> {0x04,value});
  }
}

double THR30II_Settings::GetControl(uint8_t ctrl)
{
	return control[ctrl];
}

void THR30II_Settings::ReverbSelect(THR30II_REV_TYPES type) // Setter for selection of the reverb type
{
	reverbtype = type;
	
	if( sendChangestoTHR )
	{
		SendTypeSetting(REVERB, THR30II_REV_TYPES_VALS[type].key); // Send reverb type change to THR
	}
	// TODO: Await Acknowledge?
}

void THR30II_Settings::EffectSelect(THR30II_EFF_TYPES type) // Setter for selection of the Effect type
{
	effecttype = type;
	
	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendTypeSetting(EFFECT, THR30II_EFF_TYPES_VALS[type].key); // Send effect type change to THR
	}
	// TODO: Await Acknowledge?
}

void THR30II_Settings::EchoSelect(THR30II_ECHO_TYPES type) // Setter for selection of the Effect type
{
	echotype = type;
	
	if( sendChangestoTHR ) // Do not send back if change results from THR itself
	{
		SendTypeSetting(ECHO, THR30II_ECHO_TYPES_VALS[type].key); // Send echo type change to THR
	}
	// TODO: Await Acknowledge?
}

THR30II_Settings::THR30II_Settings():patchNames( { "Actual", "UserSetting1", "UserSetting2", "UserSetting3", "UserSetting4", "UserSetting5" }) // Constructor
{
	dumpFrameNumber = 0; 
	dumpByteCount = 0; // Received bytes
	dumplen = 0;       // Expected length in bytes
  Firmware = 0x00000000;
  ConnectedModel = 0x00000000; // FamilyID (2 Byte) + ModelNr. (2 Byte): 0x0024_0002=THR30II
	unit[EFFECT]=false; unit[ECHO]=false; unit[REVERB]=false; unit[COMPRESSOR]=false; unit[GATE]=false;
}

void THR30II_Settings::SetPatchName(String nam, int nr) // nr = -1 as default for actual patchname
{														                            // corresponds to field "activeUserSetting"
	TRACE_THR30IIPEDAL(Serial.println(String("SetPatchName(): Patchnname: ") + nam + String(" nr: ") + String(nr) );)
	
	nr = constrain(nr, 0, 5);  // Make 0 based index for array patchNames[]

	patchNames[nr] = nam.length() > 64 ? nam.substring(0,64) : nam; // Cut if necessary

	if( sendChangestoTHR )
	{
		CreateNamePatch();
	}
}

//--------- METHOD FOR UPLOADING PATCHNAME TO THR30II -----------------

void THR30II_Settings::CreateNamePatch() // Fill send buffer with just setting for actual patchname, creating a valid SysEx for sending to THR30II  
{                                        // uses same algorithm as CreatePatch() - but will only be  o n e  frame!
	//1.) Make the data buffer (structure and values) store the length.
	//    Add 12 to get the 2nd length-field for the header. Add 8 to get the 1st length field for the header.
	//2.) Cut it to slices with 0x0d02 (210 dez.) bytes (gets the payload of the frames)
	//    These frames will thus not have incomplete 8/7 groups after bitbucketing but total frame length is 253 (just below the possible 255 bytes).
	//    The remaining bytes will build the last frame (perhaps with last 8/7 group incomplete)
	//    Store the length of the last slice (all others are fixed 210 bytes  long.)
	//3.) Create the header:
	//    SysExStart:  f0 00 01 0c 24 02 4d (always the same)       (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 Get actual counter for memory frames and append it here
	//                 00 01 0b  ("same frame counter" and payload size  for the header)
	//                 hang following 4-Byte values together:
	//
	//                 0d 00 00 00   (Opcode for memory write/send)
	//                 (len +8+12)   (1st length field = total length= data length + 3 values + type field/netto length )
	//                 FF FF FF FF   (user patch number to overwrite/ 0xFFFFFFFF for actual patch)
	//                 (len   +12)   (2nd length field = netto length= data length + the following 3 values)
	//                 00 00 00 00   (always the same, kind of opcode?)
	//                 01 00 00 00   (always the same, kind of opcode?)
	//                 00 00 00 00   (always the same, kind of opcode?)
	//
	//                 Bitbucket this group (gives no incomplete 8/7 group!)
	//                 and append it to the header
	//                 finish header with 0xF7
	//                 Header frame may already be sent out!
	//4.)              For each slice:
	//                 Bitbucket encode the slice
	//                 Build frame:
	//                 f0 00 01 0c 24 02 4d(always the same)       (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 Get actual counter for memory frames and append it here
	//                 append same frame counter (is the slice counter) as a byte
	//                 0d 01  (except for last frame, where we use it's stored length before bitbucketing)
	//                 bitbucketed data for this slice
	//                 0xF7
	//                 Send it out.

	TRACE_V_THR30IIPEDAL(Serial.println(F("Create_Name_Patch(): "));)
	
	std::array<byte, 300> dat;  // TODO: 2000 in createPatch()
	auto datlast = dat.begin(); // Iterator to last element (starts on begin!)
	std::array<byte, 300> sb;
	auto sblast = sb.begin();   // Iterator to last element (starts on begin!)
	std::array<byte, 300> sb2;
	auto sb2last = sb2.begin(); // Iterator to last element (starts on begin!)

	std::array<byte, 4> toInsert4;
	std::array<byte, 6> toInsert6;

	#define datback(x) for(const byte &b: x) { *datlast++ = b; };
	
	// Macro for converting a 32-Bit value to a 4-byte array<byte,4> and append it to "dat"
	#define conbyt4(x) toInsert4 = { (byte)(x), (byte)((x)>>8), (byte)((x)>>16), (byte)((x)>>24) }; datlast = std::copy(std::begin(toInsert4), std::end(toInsert4), datlast); 
	
  // Macro for appending a 16-Bit value to "dat"
	#define conbyt2(x) *datlast ++= (byte)(x); *datlast ++= (byte)((x)>>8); 
	
	// 1.) Make the data buffer (structure and values) ---------------------------------------------------------------
	datback(tokens["StructOpen"]);
	// Meta
	datback(tokens["Meta"]);          
	datback(tokens["TokenMeta"]);
	toInsert6 = { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00 };
	datlast = std::copy(std::begin(toInsert6), std::end(toInsert6),datlast); // Number 0x0000, type 0x00040000 (String)
	
	conbyt4( patchNames[0].length() + 1 ) // Length of patchname (incl. '\0')
	                                      // 32Bit-Value (little endian; low byte first) 
	
	std::string nam = patchNames[0u].c_str();
	datback( nam ); // Copy the patchName  //!!ZWEZWE!! mind UTF-8 ?
	*datlast++='\0'; // Append '\0' to the patchname
	
	datback(tokens["StructClose"]); // close Structure Meta
	
	// print whole buffer as a chain of HEX
	TRACE_V_THR30IIPEDAL(Serial.printf("\n\rRaw frame before slicing/enbucketing:\n\r"); hexdump(dat,datlast-dat.begin());)

	// Now store the length.
	// Add 12 to get the 2nd length-field for the header. Add 8 to get the 1st length field for the header.
	uint32_t datalen = (uint32_t) (datlast-dat.begin());
	
	// 2.) cut to slices ----------------------------------------------------------------------------
	uint32_t lengthfield2 = datalen + 12;
	uint32_t lengthfield1 = datalen + 20;

	int numslices = (int)datalen / 210; // Number of 210-byte slices
	int lastlen   = (int)datalen % 210; // Data left for last frame containing the rest
	TRACE_V_THR30IIPEDAL(Serial.print("Datalen: "); Serial.println(datalen);)
	TRACE_V_THR30IIPEDAL(Serial.print("Number of slices: "); Serial.println(numslices);)
	TRACE_V_THR30IIPEDAL(Serial.print("Length of last slice: "); Serial.println(lastlen);)

	// 3.) Create the header: -----------------------------------------------------------------------
	//    SysExStart:  f0 00 01 0c 22 02 4d (always the same)       (24 01 for THR10ii-W)
	//                 01   (memory command, not settings command)
	//                 put following 4-Byte values together:
	//
	//                 0d 00 00 00   (Opcode for memory write/send)
	
	// Macro for converting a 32-Bit value to a 4-byte vector<byte,4> and append it to "sb"
	#define conbyt4s(x) toInsert4 = { (byte)(x), (byte)((x)>>8), (byte)((x)>>16), (byte)((x)>>24) }; sblast = std::copy( std::begin(toInsert4), std::end(toInsert4), sblast ); 
	//sblast starts at sb.begin()
	conbyt4s(0x0du);
	//                 (len +8+12)   (1st length field = total length= data length + 3 values + type field/netto length )
	conbyt4s(lengthfield1);
	//                 FF FF FF FF   (number of user patch to overwrite / 0xFFFFFFFF for actual patch)
	conbyt4s(0xFFFFFFFFu);
	//                 (len   +12)   (2nd length field = netto length= data length + the following 3 values)
	conbyt4s(lengthfield2);
	//                 00 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x0u);
	//                 01 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x1u);
	//                 00 00 00 00   (always the same, kind of opcode?)
	conbyt4s(0x0u);
	//
	// Bitbucket this group (gives no incomplete 8/7 group!)
	sb2last = Enbucket(sb2, sb, sblast); 
	//                 And append it to the header
	//                 Finish header with 0xF7
	//                 Header frame may already be sent out!
	byte memframecnt = 0x06;
	//                 Get actual counter for memory frames and append it here

	//                 00 01 0b  ("same frame counter" and payload size  for the header)
	sblast = sb.begin(); // Start the buffer to send
	
	std::array<byte, 12> toInsert;
	toInsert = std::array<byte,12>({ 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt++, 0x00, 0x01, 0x0b });
	//toInsert=std::array<byte,12>({ 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt++, 0x00, 0x01, 0x0b });
	sblast = std::copy(toInsert.begin(), toInsert.end(), sb.begin());
	sblast = std::copy(sb2.begin(), sb2last, sblast);

	*sblast ++= 0xF7; // SysEx end demarkation

	SysExMessage m(sb.data(), sblast - sb.begin());
	Outmessage om(m, 100, false, false); // no Ack, no answer for the header 
	outqueue.enqueue(om); // Send header to THRxxII

	// 4.) For each slice: --------------------------------------------------------------------------------
	//    Bitbucket encode the slice
	
	for( int i = 0; i < numslices; i++ )
	{
		std::array<byte, 210> slice;
		std::copy(dat.begin() + i*210, dat.begin() + i*210 + 210, slice.begin());
		sb2last = Enbucket(sb2, slice, slice.end());
		sblast = sb.begin(); // Start new buffer to send
		//toInsert=  { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt++, (byte)(i % 128), 0x0d, 0x01 };
		toInsert = { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt, (byte)(i % 128), 0x0d, 0x01 }; // memframecount is not incremented inside sent patches
		//toInsert=  { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt, (byte)(i % 128), 0x0d, 0x01 };  //memframecount is not incremented inside sent patches
		sblast = std::copy(toInsert.begin(), toInsert.end(), sblast);
		sblast = std::copy(sb2.begin(), sb2last, sblast);
		*sblast ++= 0xF7;
		// Print whole buffer as a chain of HEX
		TRACE_V_THR30IIPEDAL(Serial.printf("\n\rFrame %d to send in \"Create_Name_Patch\":\n\r", i); hexdump(sb, sblast - sb.begin());)
		//m = SysExMessage(sb.data(), sblast -sb.begin() );
		//om = Outmessage(m, (uint16_t)(101 + i), false, false);  //no Ack, for all the other slices 
  	SysExMessage m(sb.data(), sblast - sb.begin());
	  Outmessage om(m, (uint16_t)(101 + i), false, false); // no Ack, for all the other slices  
    outqueue.enqueue(om); // Send slice to THRxxII
	}

	if( lastlen > 0 ) // Last slice (could be the first, if it is the only one)
	{
		std::array<byte, 210> slice;  // Here 210 is a maximum size, not the true element count
		byte *slicelast = slice.begin();
		slicelast = std::copy(dat.begin() + numslices * 210, dat.begin() + numslices * 210 + lastlen, slicelast);
		sb2last = Enbucket(sb2, slice, slicelast);
		sblast = sb.begin(); // Start new buffer to send
		toInsert = { 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, memframecnt++, (byte)((numslices) % 128), (byte)((lastlen - 1) / 16), (byte)((lastlen - 1) % 16) }; 
		//toInsert={ 0xF0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, memframecnt++, (byte)((numslices) % 128), (byte)((lastlen - 1) / 16), (byte)((lastlen - 1) % 16) }; 
		sblast = std::copy(toInsert.begin(), toInsert.end(), sblast);
		sblast = std::copy(sb2.begin(), sb2last,sblast);
		*sblast ++= 0xF7;
		// Print whole buffer as a chain of HEX
		TRACE_V_THR30IIPEDAL(Serial.println(F("\n\rLast frame to send in \"Create_Name_Patch\":")); hexdump(sb, sblast - sb.begin());)
		//m =  SysExMessage(sb.data(),sblast-sb.begin());
		//om = Outmessage(m, (uint16_t)(101 + numslices), true, false);  //Ack, but no answer for the header 
  	SysExMessage m(sb.data(), sblast - sb.begin());
	  Outmessage om(m, (uint16_t)(101 + numslices), true, false); // no Ack, but no answer for the header   
    outqueue.enqueue(om); // Send last slice to THRxxII
	}
	
	TRACE_THR30IIPEDAL(Serial.println(F("\n\rCreate_Name_patch(): Ready outsending."));)

	// Now we have to await acknowledgment for the last frame

	// on_ack_queue.enqueue(std::make_tuple(101+numslices,Outmessage(SysExMessage((const byte[29]) { 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x02, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x3c, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0xf7 },29), 
	//                      101+numslices+1, false, true),nullptr,false)); //answer will be the settings dump for actual settings

    // Summary of what we did just now:
	//                 Building the SysEx frame:
	//                 f0 00 01 0c 22 02 4d(always the same)       (24 01 for THR10ii-W)
	//                 01   (memory command, not a settings command)
	//                 Get actual counter for memory frames and append it here
	//                 append same frame counter (is the slice counter) as a byte
	//                 set length field to  0d 01  (except for last frame, where we use it's stored length before bitbucketing)
	//                 append "bitbucketed" data for this slice
	//                 finish with 0xF7
	//                 Send it out.
}

String THR30II_Settings::getPatchName()
{
   return patchNames[constrain(activeUserSetting, 0, 5)];
}

// Find the correct (col, amp)-struct object for this ampkey
col_amp THR30IIAmpKey_ToColAmp(uint16_t ampkey)
{ 
	for(auto it = THR30IIAmpKeys.begin(); it != THR30IIAmpKeys.end(); ++it)
	{
		if( it->second == ampkey )
		{
			return (col_amp)(it->first);
		}
	}
	return col_amp{CLASSIC, CLEAN};
}

//---------FUNCTION FOR SENDING COL/AMP SETTING TO THR30II -----------------
void THR30II_Settings::SendColAmp() // Send COLLLECTION/AMP setting to THR
{
		//uint16_t ak = THR30IIAmpKeys[(col_amp){col, amp}];
		//SendTypeSetting(THR30II_UNITS::CONTROL, ak);
    
    //Serial.println("Swithching Amp/Col...");
    // TODO: Insert real amp/col data

    //std::array<byte, 7 + 2 + 3 + 16 + 1> sendbuf_head = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};  //29 Bytes
    //std::array<byte, 29>                 sendbuf_body = {0xf0, 0x00, 0x01, 0x00, 0x24, 0x00, 0x4d, 0x00, 0x04, 0x00, 0x00, 0x07, 0x04, 0x0c, 0x01, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
    //SysExMessage m = SysExMessage(sendbuf_head.data(), sendbuf_head.size());
    //hexdump(sendbuf_head, sendbuf_head.size());

    // FIXME: Buffer content is OK but setting amp/col is not successful 
    // Header to set amp/collection
    //outqueue.enqueue(Outmessage(SysExMessage((const byte[29]){ 0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 },29), 1111, false, false)); // no ack/answ for the header  
    // Classic Lead
    ////delay(1000);
    //outqueue.enqueue(Outmessage(SysExMessage((const byte[29]){ 0xf0, 0x00, 0x01, 0x0, 0x24, 0x0, 0x4d, 0x00, 0x04, 0x00, 0x00, 0x07, 0x04, 0x0c, 0x01, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7 },29), 1112, true, true)); // ack/answ  
}

//--------- FUNCTION FOR SENDING CAB SETTING TO THR30II -----------------
void THR30II_Settings::SendCab() // Send cabinet setting to THR
{
  Serial.println("Swithching Cabinet...");
	SendParameterSetting((un_cmd) {THR30II_UNITS_VALS[THR30II_UNITS::GATE].key, THR30II_CAB_COMMAND}, (type_val<uint16_t>) {0x02, (uint16_t)cab});
}

//--------- FUNCTION FOR SENDING UNIT STATE TO THR30II -----------------
void THR30II_Settings::SendUnitState(THR30II_UNITS un) //Send unit state setting to THR30II
{
	type_val<uint16_t> tmp = {(byte){0x03}, unit[un]};
	SendParameterSetting((un_cmd) {THR30II_UNITS_VALS[THR30II_UNITS::GATE].key, THR30II_UNIT_ON_OFF_COMMANDS[un]}, tmp);
}

//--------- FUNCTIONS FOR SENDING SETTINGS CHANGES TO THR30II -----------------
void THR30II_Settings::SendTypeSetting(THR30II_UNITS unit, uint16_t val) //Send setting to THR (Col/Amp or Reverbtype or Effecttype)
{
	//This method is not really necessary for the THR30II-Pedal
	//We need it on the PC where we can change settings separately
	//
	//Because with the the pedal settings are changed all together with complete MIDI-patches 
	//Perhaps for future use: If knobs for switching on/off the units are added?
 /*
	if (!MIDI_Activated)
		return;

	byte[] raw_msg_body = new byte[8]; //2 Ints:  Unit + Val
	byte[] raw_msg_head = new byte[8]; //2 Ints:  Opcode + Len(Body)
	raw_msg_head[0] = 0x08;   //Opcode for unit type change
	raw_msg_head[4] = (byte)raw_msg_body.Length;  //Length of the body

	ushort key = THR30II_UNITS_VALS[unit].key;

	raw_msg_body[0] = (byte)(key % 256);
	raw_msg_body[1] = (byte)(key / 256);
	raw_msg_body[4] = (byte)(val % 256);
	raw_msg_body[5] = (byte)(val / 256);

	//Prepare Message-Body
	byte[] msg_body = Helpers.Enbucket(raw_msg_body);
	byte[] sendbuf_body = new byte[PC_SYSEX_BEGIN.Length + 2 + 3 + msg_body.Length + 1];  //for "00" and frame-counter; 3 for Length-Field, 1 for "F7"

	Buffer.BlockCopy(PC_SYSEX_BEGIN, 0, sendbuf_body, 0, PC_SYSEX_BEGIN.Length);
	sendbuf_body[PC_SYSEX_BEGIN.Length] = 0x00;
	sendbuf_body[PC_SYSEX_BEGIN.Length + 2] = 0x00;  //only one frame in this message
	sendbuf_body[PC_SYSEX_BEGIN.Length + 3] = (byte)((raw_msg_body.Length - 1) / 16);  //Length Field (Hi)
	sendbuf_body[PC_SYSEX_BEGIN.Length + 4] = (byte)((raw_msg_body.Length - 1) % 16); //Length Field (Low)
	Buffer.BlockCopy(msg_body, 0, sendbuf_body, PC_SYSEX_BEGIN.Length + 5, msg_body.Length);
	sendbuf_body[sendbuf_body.GetUpperBound(0)] = SYSEX_STOP;

	//Prepare Message-Header
	byte[] msg_head = Helpers.Enbucket(raw_msg_head);
	byte[] sendbuf_head = new byte[PC_SYSEX_BEGIN.Length + 2 + 3 + msg_head.Length + 1];

	Buffer.BlockCopy(PC_SYSEX_BEGIN, 0, sendbuf_head, 0, PC_SYSEX_BEGIN.Length);
	sendbuf_head[PC_SYSEX_BEGIN.Length] = 0x00;
	sendbuf_head[PC_SYSEX_BEGIN.Length + 2] = 0x00;  //only one frame in this message
	sendbuf_head[PC_SYSEX_BEGIN.Length + 3] = (byte)((raw_msg_head.Length - 1) / 16);  //Length Field (Hi)
	sendbuf_head[PC_SYSEX_BEGIN.Length + 4] = (byte)((raw_msg_head.Length - 1) % 16); //Length Field (Low)
	Buffer.BlockCopy(msg_head, 0, sendbuf_head, PC_SYSEX_BEGIN.Length + 5, msg_head.Length);
	sendbuf_head[sendbuf_head.GetUpperBound(0)] = SYSEX_STOP;

	if (THR10_Editor.THR10_Settings.O_device == null)
	{
		return;
	}
	if (!THR10_Editor.THR10_Settings.O_device.IsOpen)
	{
		THR10_Editor.THR10_Settings.O_device?.Open();
	}
	sendbuf_head[PC_SYSEX_BEGIN.Length + 1] = THR30II_Window.SysExSendCounter++;
	THR10_Editor.THR10_Settings.O_device?.Send(new SysExMessage(sendbuf_head));
	sendbuf_body[PC_SYSEX_BEGIN.Length + 1] = THR30II_Window.SysExSendCounter++;
	THR10_Editor.THR10_Settings.O_device?.Send(new SysExMessage(sendbuf_body));

	if (THR10_Editor.THR10_Settings.O_device.IsOpen)
	{
		THR10_Editor.THR10_Settings.O_device.Close();
	}

	//ToDO:  Set Wait for ACK
 */
}

// THRII Model:
// 00 24 00 00: THR10II = 235929610 (for "device" in thrl6p-file)
// 00 24 00 01: THR10IIWireless = 235929710 ("device" in thrl6p-file)
// 00 24 00 02: THR30IIWireless = 235929810 ("device" in thrl6p-file)
// 00 24 00 03: THR30IIAcousticWireless = 235929910 ("device" in thral6p-file)
String THR30II_Settings::THRII_MODEL_NAME()
{ 
	switch(ConnectedModel) 
	{
		case 0x00240000:
			return String("THR10II");
			break;

		case 0x00240001:
			return String("THR10II-W");
			break;

		case 0x00240002:
			return String("THR30II-W");
			break;
		
		case 0x00240003:
			return String("THR30II-WA");
			break;

		default:
			return String( "None");
		  break;
	}
}

ArduinoQueue<Outmessage> outqueue(30); // FIFO Queue for outgoing SysEx-Messages to THRII

// Messages, that have to be sent out,
// and flags to change, if an awaited acknowledge comes in (adressed by their ID)
ArduinoQueue <std::tuple<uint16_t, Outmessage, bool *, bool>> on_ack_queue;

ArduinoQueue<SysExMessage> inqueue(40);


/////////////////////////////////////////////////////////////////////////////////////////////////
// UI DRAWING FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////

void drawPatchID(uint16_t fgcolour, int patchID)
{
 	int x = 0, y = 0, w = 80, h = 80;
	uint16_t bgcolour = TFT_THRVDARKGREY;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);
	spr.setTextFont(7);
	spr.setTextSize(1);
	spr.setTextDatum(MR_DATUM);
	spr.setTextColor(TFT_BLACK);
	spr.drawNumber(88, w-7, h/2);
	spr.setTextColor(fgcolour);
	spr.drawNumber(patchID, w-7, h/2);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawPatchIconBank(int presel_patch_id, int active_patch_id)
{
	uint16_t iconcolour = 0;
	int banksize = 5;
	int first = ((presel_patch_id-1)/banksize)*banksize+1; // Find number for 1st icon in row
	
	switch( _uistate ) 
  {
		case UI_home_amp:
			for(int i = 1; i <= 5; i++)
			{
				if( i == THR_Values.getActiveUserSetting() + 1 )
				{
          // Note: if user preset is selected and parameter is changed via THRII, then getActiveUserSetting() returns -1
					iconcolour = TFT_SKYBLUE; // Highlight active user preset patch icon
				}
				else if( i == presel_patch_id )
				{
					iconcolour = ST7789_ORANGE; // Highlight pre-selected patch icon
				}
				else
				{
					iconcolour = TFT_THRBROWN; // Colour for unselected patch icons
				}
				if( i > npatches )
				{
					iconcolour = TFT_BLACK;
				}
				drawPatchIcon(60 + 20*i, 0, 20, 20, iconcolour, i);
			}
		break;
		
		case UI_home_patch:
			for(int i = first; i < first+banksize; i++)
			{
				if( i == active_patch_id )
				{
					iconcolour = TFT_THRCREAM; // Highlight active patch icon
				}
				else if( i == presel_patch_id )
				{
					iconcolour = ST7789_ORANGE; // Highlight pre-selected patch icon
				}
				else
				{
					iconcolour = TFT_THRBROWN; // Colour for unselected patch icons
				}
				if( i > npatches )
				{
					iconcolour = TFT_BLACK;
				}
				drawPatchIcon(60 + 20*(i-first+1), 0, 20, 20, iconcolour, i);
			}
		break;

		default:
		break;
	}
}

void drawPatchIcon(int x, int y, int w, int h, uint16_t colour, int patchID)
{
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawNumber(patchID, 10, 11);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawPatchSelMode(uint16_t colour)
{
  int x = 180, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawString("NOW", w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawAmpSelMode(uint16_t colour, String string)
{
  int x = 230, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawString(string, w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawConnIcon(bool THRconnected)
{
  spr.createSprite(40, 20);
  if( THRconnected ) // Connected: green
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_GREEN, TFT_BLACK); // Bg
    spr.drawWideLine(1, 9, 14, 9, 3, TFT_WHITE, TFT_GREEN);         // Left wire
    spr.drawWideLine(26, 9, 38, 9, 3, TFT_WHITE, TFT_GREEN);        // Right wire
    spr.fillSmoothRoundRect(12, 2, 8, 15, 3, TFT_WHITE, TFT_GREEN); // Plug
    spr.fillRect(16, 2, 4, 15, TFT_WHITE);                          // Plug
    spr.fillSmoothRoundRect(21, 2, 8, 15, 3, TFT_WHITE, TFT_GREEN); // Socket
    spr.fillRect(21, 2, 4, 15, TFT_WHITE);                          // Socket
    spr.fillRect(16, 4, 5, 3, TFT_WHITE);                           // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_WHITE);                          // Lower prong
  }
  else // Disconnected: red 
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_RED, TFT_BLACK); // Bg
    spr.drawWideLine(1, 9, 10, 9, 3, TFT_WHITE, TFT_RED);         // Left wire
    spr.drawWideLine(30, 9, 38, 9, 3, TFT_WHITE, TFT_RED);        // Right wire
    spr.fillSmoothRoundRect(8, 2, 8, 15, 3, TFT_WHITE, TFT_RED);  // Plug
    spr.fillRect(12, 2, 4, 15, TFT_WHITE);                        // Plug
    spr.fillSmoothRoundRect(24, 2, 8, 15, 3, TFT_WHITE, TFT_RED); // Socket
    spr.fillRect(24, 2, 4, 15, TFT_WHITE);                        // Socket
    spr.fillRect(16, 4, 5, 3, TFT_WHITE);                         // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_WHITE);                        // Lower prong
  }
  spr.pushSprite(280, 0);
  spr.deleteSprite();
}

void drawPatchName(uint16_t fgcolour, String patchname, bool inverted = false)
{
  int x = 80, y = 20, w = 240, h = 60;
  uint16_t fg_colour = fgcolour;
  uint16_t bg_colour = TFT_THRVDARKGREY;
  if( inverted )
  {
    fg_colour = TFT_THRVDARKGREY;
    bg_colour = fgcolour;
  }
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w, h-1, 3, bg_colour, TFT_BLACK);
  spr.loadFont(AA_FONT_LARGE);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fg_colour, bg_colour);
  spr.drawString(patchname, w/2, h/2);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawBarChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int barh)
{
  int tpad = 20; int pad = 0;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour);
  spr.drawString(label, w/2-1, 11);
  spr.fillRect(pad, h-(h-tpad-pad)*barh/100, w-1-2*pad, (h-tpad-pad)*barh/100, fgcolour);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawEQChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int b, int m, int t)
{
  int tpad = 20; int pad = 0;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour);
  spr.drawString(label, w/2, 11);
  spr.fillRect(pad, h-(h-tpad-pad)*b/100, w/3-1-2*pad, (h-tpad-pad)*b/100, fgcolour);
  spr.fillRect(w/3+pad, h-(h-tpad-pad)*m/100, w/3-1-2*pad, (h-tpad-pad)*m/100, fgcolour);
  spr.fillRect(2*w/3+pad, h-(h-tpad-pad)*t/100, w/3-1-2*pad, (h-tpad-pad)*t/100, fgcolour);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawAmpUnit(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, THR30II_COL col, THR30II_AMP amp, THR30II_CAB cab)
{
	int tpad = 2; int pad = 2;
	uint16_t ampcolour1 [3] = {TFT_THRRED, TFT_THRBLUE, TFT_THRGREEN};
	uint16_t ampcolour2 [3] = {TFT_THRLIGHTRED, TFT_THRLIGHTBLUE, TFT_THRLIGHTGREEN};
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);	// Unit body
	spr.fillSmoothRoundRect(pad, tpad, w-1-2*pad, h-1-tpad-pad, 1, fgcolour, bgcolour);	// Draw area
	spr.drawSpot(16, 16, 7.5, ampcolour2[col], fgcolour);	// Col status light
	spr.drawSpot(16, 16, 6, ampcolour1[col], ampcolour2[col]);
	spr.drawSpot(19, 13, 1.5, ampcolour2[col], ampcolour1[col]);	// Col status light
	spr.loadFont(AA_FONT_MONO);
	spr.setTextDatum(ML_DATUM);
	spr.setTextColor(bgcolour, fgcolour);
	spr.drawString(THR30II_AMP_NAMES[amp], 28, 18);	// Unit label
	spr.drawString(THR30II_CAB_NAMES[cab], 12, 43);	// Unit label
	
	int cabcentrex = 210;
	int cabcentrey = 30;
	int cabw = 50;
	int cabh = 50;

	switch( cab )	// Draw cabinet icon depending on type
	{
		case 0 ... 6:
		case 14:
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-cabh/4,   9, bgcolour, fgcolour); // Top left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-cabh/4,   7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-cabh/4,   9, bgcolour, fgcolour); // Top right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-cabh/4,   7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey+cabh/4-2, 9, bgcolour, fgcolour); // Bottom left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey+cabh/4-2, 7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey+cabh/4-2, 9, bgcolour, fgcolour); // Bottom right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey+cabh/4-2, 7, fgcolour, bgcolour);
		break;

		case 7 ... 11:
			cabh = 30;
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-1, 9, bgcolour, fgcolour);	// Top left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-1, 7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-1, 9, bgcolour, fgcolour);	// Top right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-1, 7, fgcolour, bgcolour);
		break;

		case 12 ... 13:
		case 15:
			cabw = 40;
			cabh = 40;
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-1, cabcentrey-1, 15, bgcolour, fgcolour);	// Top left
			spr.drawSpot(cabcentrex-1, cabcentrey-1, 13, fgcolour, bgcolour);
		break;

		case 16:
		break;
	}

	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawUtilUnit(int x, int y, int w, int h, int bpad, uint16_t bgcolour, uint16_t fgcolour, String label, double utilparams[2])
{
	int tpad = 30; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = 0; int barh = grh/2;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-bpad, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2, 16);
	spr.fillRect(grx, gry, grw, grh, fgcolour);	// Draw graph area
	barw = grw * utilparams[0]/100;
	spr.fillRect(grx, gry, barw, barh, bgcolour);
	barw = grw * utilparams[1]/100;
	spr.fillRect(grx, gry+barh, barw, barh+1, bgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawFXUnit(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int nbars, double FXparams[5], int selectedFXparam)
{
	int tpad = 30; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = grw / nbars;	// Calculate bar width
	int barh = 0;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);	// Draw FX unit
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2, 16);	// Write label
	spr.fillRect(grx, gry, grw, grh, fgcolour);	// Draw graph area
	for(int i = 0; i < nbars; i++)
	{
		barh = grh * FXparams[i] / 100;	// calculate bar height
		if( nbars == 4 ) 
    {
			barw = grw / nbars + 1;
		}
		if( i == nbars - 1 ) { // Widen last bar by 1px
			spr.fillRect(grx + i * barw, gry + grh - barh, barw+1, barh, bgcolour);
		}
    else 
    {
			spr.fillRect(grx + i * barw, gry + grh - barh, barw, barh, bgcolour);
		}
	}
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawPPChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int ped1, int ped2)
{
  // Used to represent Guitar Volume and Audio Volume
	int tpad = 20; int pad = 0;
//	int ped1height = ped1*(h-tpad-pad)/1023;
//	int ped2height = ped2*(h-tpad-pad)/1023;
	int ped1height = ped1*(h-tpad-pad)/100; // For guitar and audio volume
	int ped2height = ped2*(h-tpad-pad)/100; // 
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2-1, 11);
	spr.fillRect(pad, h-ped1height, w/2-1-2*pad, ped1height, fgcolour);
	//spr.fillRect(pad, h-(h-tpad-pad)*ex/100, w/2-1-2*pad, (h-tpad-pad)*ex/100, fgcolour);
	spr.fillRect(w/2+pad, h-ped2height, w/2-1-2*pad, ped2height, fgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawFXUnitEditMode()
{
}

void THR30II_Settings::updateConnectedBanner() // Show the connected Model 
{
	// //Display "THRxxII" in blue (as long as connection is established)

	// s1=THRxxII_MODEL_NAME();
	// if(ConnectedModel != 0)
	// {
	// 	tft.loadFont(AA_FONT_LARGE); // Set current font    
	// 	tft.setTextSize(1);
	// 	tft.setTextColor(TFT_BLUE, TFT_BLACK);
	// 	tft.setTextDatum(TL_DATUM);
	// 	tft.setTextPadding(160);
	// 	tft.drawString(s1, 0, 0);//printAt(tft,0,0, s1 ); // Print e.g. "THR30II" in blue in header position
	// 	tft.setTextPadding(0);
	// 	tft.unloadFont();
	// }
}

/////////////////////////////////////////////////////////////////
// Redraw all the bars and values on the TFT
// x-position (0) where to place top left corner of status mask
// y-position     where to place top left corner of status mask
/////////////////////////////////////////////////////////////////
void THR30II_Settings::updateStatusMask(uint8_t x, uint8_t y)
{
	// Patch number
  drawPatchID(TFT_THRCREAM, active_patch_id);
	
	// Patch icon bank
	drawPatchIconBank(presel_patch_id, active_patch_id);

	// Patch select mode (pre-select/immediate) & UI mode
	switch( _uistate )
  {
		case UI_home_amp:
			if( send_patch_now ) { drawPatchSelMode(TFT_THRCREAM); }
			else                 { drawPatchSelMode(TFT_THRBROWN); }
		  break;

		case UI_home_patch:
			if( send_patch_now ) { drawPatchSelMode(TFT_THRCREAM); }
			else                 { drawPatchSelMode(TFT_THRBROWN); }
		  break;

		case UI_edit:
		  break;
		default:
		  break;
	}

	// Amp select mode (COL/AMP/CAB)
	switch( amp_select_mode )
	{
		case COL:
			drawAmpSelMode(TFT_THRCREAM, "COL");
		  break;

		case AMP:
			drawAmpSelMode(TFT_THRCREAM, "AMP");
		  break;

		case CAB:
			drawAmpSelMode(TFT_THRCREAM, "CAB");
		  break;
	}

	String FXtitle;
	uint16_t FXbgcolour  =  0;
	uint16_t FXfgcolour  =  0;
	double FXparams[5]   = {0};
	double utilparams[2] = {0};
	uint8_t FXx;
	uint8_t FXy;
	uint8_t FXw =  60;
	uint8_t FXh = 100;
	uint8_t nFXbars = 5;
	uint8_t selectedFXparam = 0;

	// Gain, Master, and EQ (B/M/T) ---------------------------------------------------------
  drawBarChart( 0, 80, 15, 160, TFT_THRBROWN, TFT_THRCREAM,  "G", control[CTRL_GAIN]);
  if( boost_activated )
  {
    drawBarChart(15, 80, 15, 160, TFT_THRDIMORANGE, TFT_THRORANGE,  "M", control[CTRL_MASTER]);
   	drawEQChart( 30, 80, 30, 160, TFT_THRDIMORANGE, TFT_THRORANGE, "EQ", control[CTRL_BASS], control[CTRL_MID], control[CTRL_TREBLE]);
  }
  else
  {
    drawBarChart(15, 80, 15, 160, TFT_THRBROWN, TFT_THRCREAM,  "M", control[CTRL_MASTER]);
   	drawEQChart( 30, 80, 30, 160, TFT_THRBROWN, TFT_THRCREAM, "EQ", control[CTRL_BASS], control[CTRL_MID], control[CTRL_TREBLE]);
  }

	// Amp/Cabinet ---------------------------------------------------------------
	switch( col )
	{
		case BOUTIQUE:
			tft.setTextColor(TFT_BLUE, TFT_BLACK);
		break;

		case MODERN:
			tft.setTextColor(TFT_GREEN, TFT_BLACK);
		break;

		case CLASSIC:
			tft.setTextColor(TFT_RED, TFT_BLACK);
		break;
	}
  drawAmpUnit(60, 80, 240, 60, TFT_THRCREAM, TFT_THRBROWN, "Amp", col, amp, cab);
	
	// FX1 Compressor -------------------------------------------------------------
	utilparams[0] = compressor_setting[CO_SUSTAIN];
	utilparams[1] = compressor_setting[CO_LEVEL];
 	if(THR_Values.unit[COMPRESSOR]) { drawUtilUnit(60, 140, 60, 50, 1, TFT_THRWHITE,    TFT_THRDARKGREY,  "Comp", utilparams); }
  else                            { drawUtilUnit(60, 140, 60, 50, 1, TFT_THRDARKGREY, TFT_THRVDARKGREY, "Comp", utilparams); }
	
 	// Gate -----------------------------------------------------------------------
	utilparams[0] = gate_setting[GA_THRESHOLD];
	utilparams[1] = gate_setting[GA_DECAY];
 	if(THR_Values.unit[GATE]) {	drawUtilUnit(60, 190, 60, 50, 0, TFT_THRYELLOW,    TFT_THRDIMYELLOW, "Gate", utilparams); }
  else                      { drawUtilUnit(60, 190, 60, 50, 0, TFT_THRDIMYELLOW, TFT_THRVDARKGREY, "Gate", utilparams); }

	// FX2 Effect (Chorus/Flanger/Phaser/Tremolo) ----------------------------------
	switch( effecttype )
	{
		case CHORUS:
			FXtitle = "Chor";	 // Set FX unit title
			if( unit[EFFECT] ) // FX2 activated
			{
				FXbgcolour = TFT_THRFORESTGREEN;
				FXfgcolour = TFT_THRDIMFORESTGREEN;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMFORESTGREEN;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = effect_setting[CHORUS][CH_SPEED];
			FXparams[1] = effect_setting[CHORUS][CH_DEPTH];
			FXparams[2] = effect_setting[CHORUS][CH_PREDELAY];
			FXparams[3] = effect_setting[CHORUS][CH_FEEDBACK];
			FXparams[4] = effect_setting[CHORUS][CH_MIX];
			nFXbars = 5;  
		break;

		case FLANGER: 
			FXtitle = "Flan";	 // Set FX unit title
			if( unit[EFFECT] ) // FX2 activated
			{
				FXbgcolour = TFT_THRLIME;
				FXfgcolour = TFT_THRDIMLIME;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMLIME;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = effect_setting[FLANGER][FL_SPEED];
			FXparams[1] = effect_setting[FLANGER][FL_DEPTH];
			FXparams[2] = effect_setting[FLANGER][FL_MIX];
			FXparams[3] = 0;
			FXparams[4] = 0;
			nFXbars = 3;
		break;

		case PHASER:
			FXtitle = "Phas";	 // Set FX unit title
			if( unit[EFFECT] ) // FX2 activated
			{
				FXbgcolour = TFT_THRLEMON;
				FXfgcolour = TFT_THRDIMLEMON;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMLEMON;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = effect_setting[PHASER][PH_SPEED];
			FXparams[1] = effect_setting[PHASER][PH_FEEDBACK];
			FXparams[2] = effect_setting[PHASER][PH_MIX];
			FXparams[3] = 0;
			FXparams[4] = 0;
			nFXbars = 3;		  
		break;		

		case TREMOLO:
			FXtitle = "Trem";	 // Set FX unit title
			if( unit[EFFECT] ) // FX2 activated
			{
				FXbgcolour = TFT_THRMANGO;
				FXfgcolour = TFT_THRDIMMANGO;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMMANGO;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = effect_setting[TREMOLO][TR_SPEED];
			FXparams[1] = effect_setting[TREMOLO][TR_DEPTH];
			FXparams[2] = effect_setting[TREMOLO][TR_MIX];
			FXparams[3] = 0;
			FXparams[4] = 0;
			nFXbars = 3;
		break;
	} // of switch(effecttype)
		
	FXx = 120; // Set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

  // FX3 Echo (Tape Echo/Digital Delay)
  switch( echotype )
	{
		case TAPE_ECHO:
			FXtitle = "Tape";	// Set FX unit title
			if( unit[ECHO] )  // FX3 activated
			{
				FXbgcolour = TFT_THRROYALBLUE;
				FXfgcolour = TFT_THRDIMROYALBLUE;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMROYALBLUE;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = echo_setting[TAPE_ECHO][TA_TIME];
			FXparams[1] = echo_setting[TAPE_ECHO][TA_FEEDBACK];
			FXparams[2] = echo_setting[TAPE_ECHO][TA_BASS];
			FXparams[3] = echo_setting[TAPE_ECHO][TA_TREBLE];
			FXparams[4] = echo_setting[TAPE_ECHO][TA_MIX];
			nFXbars = 5;
		break;

		case DIGITAL_DELAY:
			FXtitle = "D.D.";	// Set FX unit title
			if( unit[ECHO] )  // FX3 activated
			{
				FXbgcolour = TFT_THRSKYBLUE;
				FXfgcolour = TFT_THRDIMSKYBLUE;
			}
			else // FX3 deactivated
			{
				FXbgcolour = TFT_THRDIMSKYBLUE;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = echo_setting[DIGITAL_DELAY][DD_TIME];
			FXparams[1] = echo_setting[DIGITAL_DELAY][DD_FEEDBACK];
			FXparams[2] = echo_setting[DIGITAL_DELAY][DD_BASS];
			FXparams[3] = echo_setting[DIGITAL_DELAY][DD_TREBLE];
			FXparams[4] = echo_setting[DIGITAL_DELAY][DD_MIX];
			nFXbars = 5;  
		break;
	}	// of switch(effecttype)
	
	FXx = 180;	// set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

 	// FX4 Reverb (Spring/Room/Plate/Hall)
	switch( reverbtype )
	{
		case SPRING:
			FXtitle = "Spr";   // Set FX unit title
			if( unit[REVERB] ) // FX4 activated
			{
				FXbgcolour = TFT_THRRED;
				FXfgcolour = TFT_THRDIMRED;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMRED;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = reverb_setting[SPRING][SP_REVERB];
			FXparams[1] = reverb_setting[SPRING][SP_TONE];
			FXparams[2] = reverb_setting[SPRING][SP_MIX];
			FXparams[3] = 0;
			FXparams[4] = 0;
			nFXbars = 3;  
		break;

		case ROOM:
			FXtitle = "Room";  // Set FX unit title
			if( unit[REVERB] ) // FX4 activated
			{
				FXbgcolour = TFT_THRMAGENTA;
				FXfgcolour = TFT_THRDIMMAGENTA;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMMAGENTA;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = reverb_setting[ROOM][RO_DECAY];
			FXparams[1] = reverb_setting[ROOM][RO_PREDELAY];
			FXparams[2] = reverb_setting[ROOM][RO_TONE];
			FXparams[3] = reverb_setting[ROOM][RO_MIX];
			FXparams[4] = 0;
			nFXbars = 4;  
		break;

		case PLATE:
			FXtitle = "Plate"; // Set FX unit title
			if( unit[REVERB] ) // FX4 activated
			{
				FXbgcolour = TFT_THRPURPLE;
				FXfgcolour = TFT_THRDIMPURPLE;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMPURPLE;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = reverb_setting[PLATE][PL_DECAY];
			FXparams[1] = reverb_setting[PLATE][PL_PREDELAY];
			FXparams[2] = reverb_setting[PLATE][PL_TONE];
			FXparams[3] = reverb_setting[PLATE][PL_MIX];
			FXparams[4] = 0;
			nFXbars = 4;  
		break;

		case HALL:
			FXtitle = "Hall";	 // Set FX unit title
			if( unit[REVERB] ) // FX4 activated
			{
				FXbgcolour = TFT_THRVIOLET;
				FXfgcolour = TFT_THRDIMVIOLET;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMVIOLET;
				FXfgcolour = TFT_THRVDARKGREY;
			}
			FXparams[0] = reverb_setting[HALL][HA_DECAY];
			FXparams[1] = reverb_setting[HALL][HA_PREDELAY];
			FXparams[2] = reverb_setting[HALL][HA_TONE];
			FXparams[3] = reverb_setting[HALL][HA_MIX];
			FXparams[4] = 0;
			nFXbars = 4;  
		break;
	}	// of switch(reverbtype)
		
	FXx = 240; // Set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

  // Exp/vol pedal positions
  // NOTE: Used to show Guitar and Audio volume values
  drawPPChart(300, 80, 20, 160, TFT_THRBROWN, TFT_THRCREAM, "VA", pedal_1_val, pedal_2_val);

	String s2,s3;
	
	switch( _uistate )
	{
	  case UI_home_amp: // !patchActive
    	if( THR_Values.thrSettings )
			{
				//s2 = "THR Panel";
        s2 = THRII_MODEL_NAME();
        if( s2 != "None") { s2 += " PANEL";       }
        else              { s2 = "NOT CONNECTED"; }
				drawPatchName(TFT_SKYBLUE, s2, boost_activated);
			}
			else if( THR_Values.getUserSettingsHaveChanged() )
			{
				s2 = THR_Values.getPatchName(); // "(*)" removed to save space
				drawPatchName(ST7789_ORANGERED, s2, boost_activated);
			}
			else
			{
				// Update GUI status line
				s2 = THR_Values.getPatchName();
				drawPatchName(TFT_SKYBLUE, s2, boost_activated);
			}
	 	break;

		case UI_home_patch:	// Patch is active
			// If unchanged User Memory setting is active:
			if( !THR_Values.getUserSettingsHaveChanged() )
			{
				if( presel_patch_id != active_patch_id )
				{
					s2 = libraryPatchNames[presel_patch_id - 1]; // libraryPatchNames is 0-indexed
					drawPatchName(ST7789_ORANGE, s2, boost_activated);
				}
				else
				{
					s2 = libraryPatchNames[active_patch_id - 1];
					drawPatchName(TFT_THRCREAM, s2, boost_activated);
				}
			}
			else
			{
				s2 = libraryPatchNames[active_patch_id - 1]; // libraryPatchNames is 0-indexed; "(*)" removed to save space
				drawPatchName(ST7789_ORANGERED, s2, boost_activated);
			}
		break;

		default:
		break;
	}
	maskUpdate = false; // Tell local GUI that mask has been updated
}

void WorkingTimer_Tick() // Latest martinzw version + BJW debug msgs
{
	// Care about queued incoming messages
	if (inqueue.item_count() > 0)
	{
		SysExMessage msg (inqueue.dequeue());
    // CHECK: If ParseSysEx() is called without printing the result, some timeout occurs, Are we too quick?
		Serial.println("Work. Tmr: " + THR_Values.ParseSysEx(msg.getData(), msg.getSize()));
		//THR_Values.ParseSysEx(msg.getData(),msg.getSize());
    if( THR_Values.userPresetDownloaded )
    {
      THR_Values.userPresetDownloaded = false;
      int idx = THR_Values.getActiveUserSetting();
      Serial.println("Work. Tmr: User preset #" + String(idx) + " " + THR_Values.getPatchName() + " to be copied");
      if( idx >= 0 && idx <= 5 )
      {
        // Store latest user preset settings localy
        thr_user_presets[idx] = THR_Values; // THR30II_Settings class is deep copyable
        nUserPreset = idx;
        Serial.println("Done");

        // Activate the patch
        if( isUsrPreset )
        {
          isUsrPreset = false;
          stored_THR_Values = THR_Values;
          patch_deactivate();
        }
      }
    }

		if( !maskActive )
		{
			maskActive = true; // Tell GUI to show Settings mask
			// drawStatusMask(0, 85);
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

uint16_t UnitOnMap(uint16_t u)
{
	std::map<uint16_t, uint16_t>::iterator p = unitOnMap.find(u);
	if( p != unitOnMap.end() ) { return p->second; } // The value for this key
	else                       { return 0xFFFF;    } // Map does not contain key u
}

uint16_t ReverbMap(uint16_t u)
{
	std::map<uint16_t, uint16_t>::iterator p = reverbMap.find(u);
	if( p != reverbMap.end() ) { return p->second; } // The value for this key
	else                       { return 0xFFFF;    } // Map does not contain key u
}

uint16_t EchoMap(uint16_t u)
{
	std::map<uint16_t, uint16_t>::iterator p = echoMap.find(u);
	if( p != echoMap.end() ) { return p->second; } // The value for this key
	else                     { return 0xFFFF;    } // Map does not contain key u
}

uint16_t EffectMap(uint16_t u)
{
	std::map<uint16_t, uint16_t>::iterator p = effectMap.find(u);
	if( p != effectMap.end() ) { return p->second; } // The value for this key
	else                       { return 0xFFFF;    } // Map does not contain key u
}

uint16_t CompressorMap(uint16_t u)
{
	std::map<uint16_t, uint16_t>::iterator p = compressorMap.find(u);
	if( p != compressorMap.end() ) { return p->second; } // The value for this key
	else                           { return 0xFFFF;    } // Map does not contain key u
}

void OnSysEx(const uint8_t *data, uint16_t length, bool complete)
{
	//Serial.println("SysEx Received "+String(length));
	//complete?Serial.println("- completed "):Serial.println("- continued ");
	
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
