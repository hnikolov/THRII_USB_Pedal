#ifndef _BUTTONS_H_
#define _BUTTONS_H_

#include <AceButton.h>
using namespace ace_button;

const int NUM_BUTTONS = 9;

// Define input button pins
#define BUTTON_1_PIN  14
#define BUTTON_2_PIN  15
#define BUTTON_3_PIN  16
#define BUTTON_4_PIN  17
#define BUTTON_5_PIN  18
#define BUTTON_6_PIN  19
#define BUTTON_7_PIN  20
#define BUTTON_8_PIN  21
#define BUTTON_9_PIN  22

std::map<int, int> bmap =
{
//   Pin number, Button number
  {BUTTON_1_PIN, 1},
  {BUTTON_2_PIN, 2},
  {BUTTON_3_PIN, 3},
  {BUTTON_4_PIN, 4},
  {BUTTON_5_PIN, 5},
  {BUTTON_6_PIN, 6},
  {BUTTON_7_PIN, 7},
  {BUTTON_8_PIN, 8},
  {BUTTON_9_PIN, 9}
};

uint8_t pins[NUM_BUTTONS] =
{
  BUTTON_1_PIN,
  BUTTON_2_PIN,
  BUTTON_3_PIN,
  BUTTON_4_PIN,
  BUTTON_5_PIN,
  BUTTON_6_PIN,
  BUTTON_7_PIN,
  BUTTON_8_PIN,
  BUTTON_9_PIN
};

// Contains 1-9 for pressed, 11-19 for hold buttons 
uint8_t button_state = 0;

/////////////////////////////////////////////////////////////////////////////////////////
// BUTTONS EVENT HANDLER 
/////////////////////////////////////////////////////////////////////////////////////////
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  uint8_t button_pressed =  bmap[button->getPin()];

  switch (eventType)
  {
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

// Use an array
AceButton buttons[NUM_BUTTONS];

// ----------------------
// Initialise the buttons
// ----------------------
void initialize_buttons()
{  
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
}

#endif // _BUTTONS_H_