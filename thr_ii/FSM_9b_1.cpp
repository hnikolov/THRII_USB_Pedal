#include "THR30II_Pedal.h"

/////////////////////////////////////////////////////////////////

// Normal TRACE/DEBUG
#define TRACE_THR30IIPEDAL(x) x	  // trace on
// #define TRACE_THR30IIPEDAL(x)	// trace off

// Verbose TRACE/DEBUG
#define TRACE_V_THR30IIPEDAL(x) x	 // trace on
// #define TRACE_V_THR30IIPEDAL(x) // trace off

extern THR30II_Settings THR_Values, stored_THR_Values;

extern ampSelectModes amp_select_mode;
dynModes dyn_mode = Comp; // Used only in the FSM

extern std::array< THR30II_Settings, 5 > thr_user_presets;

extern uint16_t npatches;  // Counts the patches stored on SD-card or in PROGMEN
extern int8_t nUserPreset; // Used to cycle the THRII User presets

int16_t bank_first_patch = 1; // The first patch in a bank
int16_t presel_patch_id  = 1; // ID of actually pre-selected patch (absolute number)
int16_t active_patch_id  = 1; // ID of actually selected patch     (absolute number)
// Test FSM_10b_v1 that still works with current updateStatusMask(), note button 3 behavior in the button handler
//bool send_patch_now = false; // pre-select patch to send (false) or send immediately (true) - Used in updateStatusMask (TODO))
bool show_patch_num = false;

extern uint32_t maskCUpdate;

extern uint32_t maskPatchID;
extern uint32_t maskPatchIconBank;
extern uint32_t maskPatchSelMode;
extern uint32_t maskPatchSet;
extern uint32_t maskAmpSelMode;
extern uint32_t maskConnIcon; // Not used
extern uint32_t maskPatchName;
extern uint32_t maskGainMaster;
extern uint32_t maskVolumeAudio;
extern uint32_t maskEQChart;
extern uint32_t maskAmpUnit;
extern uint32_t maskCompressor;
extern uint32_t maskNoiseGate;
extern uint32_t maskFxUnit;
extern uint32_t maskEcho;
extern uint32_t maskReverb;
extern uint32_t maskAll;

UIStates _uistate_prev = UI_home_patch; // To remember amp vs custom patches state
/////////////////////////////////////////////////////////////////

std::array<byte, 29> ask_preset_buf = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// Switch to preset #4 (__0x03__)
std::array<byte, 29> switch_preset_buf = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, 0x08, 0x00, 0x0e, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// TODO: Switch col/amp:  Classic Lead
// std::array<byte, 29> sendbuf_head = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// std::array<byte, 29> sendbuf_body = {0xf0, 0x00, 0x01, 0x00, 0x24, 0x00, 0x4d, 0x00, 0x04, 0x00, 0x00, 0x07, 0x04, 0x0c, 0x01, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};

void toggle_boost()
{
  if(THR_Values.boost_activated)
  {
    undo_gain_boost();
    Serial.println("Gain boost deactivated");
  }
  else
  {
    do_gain_boost();
    Serial.println("Gain boost activated");
  }
}

void select_thrii_preset(byte nThriiPreset)
{
  nUserPreset = nThriiPreset;
  THR_Values.setActiveUserSetting(nThriiPreset);
  THR_Values.setUserSettingsHaveChanged(false);
  THR_Values.thrSettings = false;
  THR_Values.boost_activated = false;

  // Switch to User preset ASAP, TODO: Check for timeouts...
  switch_preset_buf[22] = nThriiPreset;
  outqueue.enqueue(Outmessage(SysExMessage(switch_preset_buf.data(), switch_preset_buf.size()), 88, false, true));

  // Request User preset data if not already done
  if(thr_user_presets[nThriiPreset].getActiveUserSetting() == -1)
  {
    Serial.println("Requesting data for User preset #" + String(nThriiPreset));

    // Store to THR_Values and thr_user_presets[nThriiPreset]
    ask_preset_buf[22] = nThriiPreset;
    outqueue.enqueue(Outmessage(SysExMessage(ask_preset_buf.data(), ask_preset_buf.size()), 88, false, true));
  }
  else
  {
    // Just update the display with the User preset data
    THR_Values = thr_user_presets[nThriiPreset];
  }  
}

void select_user_patch(int16_t &presel_patch_id, uint8_t pnr)
{
  if( bank_first_patch + pnr <= npatches ) // The new patch-id is available
  {
    presel_patch_id = bank_first_patch + pnr;
    if(presel_patch_id != active_patch_id) // A new patch to be activated
    {
      Serial.printf("\n\rActivating patch %d ...\n\r", presel_patch_id);
      show_patch_num = false;
      patch_activate(presel_patch_id);
    }
    else // Solo switch
    {
      toggle_boost();
    }
  }
  else
  {
    Serial.printf("\n\rActivating patch - Invalid patch number: %d ...\n\r", bank_first_patch + pnr);
  }
}

void handle_home_amp(UIStates &_uistate, uint8_t &button_state);
void handle_home_patch(UIStates &_uistate, uint8_t &button_state);
void handle_patch_manual(UIStates &_uistate, uint8_t &button_state);

//                0        1            2              3        4        5        6                7
// enum UIStates {UI_idle, UI_home_amp, UI_home_patch, UI_manual, UI_edit, UI_save, UI_name, UI_init_act_set, UI_act_vol_sol, UI_patch, UI_ded_sol, UI_pat_vol_sol};
void fsm_9b_1(UIStates &_uistate, uint8_t &button_state) 
{
	if( button_state != 0 ) // A foot switch was pressed
	{
		TRACE_V_THR30IIPEDAL(Serial.println();)
		TRACE_V_THR30IIPEDAL(Serial.println("button_state: " + String(button_state));)
		TRACE_V_THR30IIPEDAL(Serial.println("Old UI_state: " + String(_uistate));)

		switch (_uistate)
		{
			case UI_idle:
			case UI_home_amp:
        handle_home_amp(_uistate, button_state);
      break;

			case UI_home_patch:
        handle_home_patch(_uistate, button_state);
			break;

			case UI_manual:
        handle_patch_manual(_uistate, button_state);
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
	} // button_state != 0
}

// ================================================================
// THRII 5 presets mode
// ================================================================
void handle_home_amp(UIStates &_uistate, uint8_t &button_state)
{ 
    switch (button_state)
    {
        case 1:
          maskCUpdate = maskAll;
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 2: 
          maskCUpdate = maskAll;
          button_state = 0;  // Remove flag, because it is handled      
        break;

        case 3: // Toggle between amp and custom patches
          _uistate = UI_home_patch;
          patch_activate(presel_patch_id);
          maskCUpdate = maskAll;
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;         
          button_state = 0;          // Remove flag, because it is handled
        break;
            
        case 5: // Activate the patch 1 in a bank
          if(THR_Values.getActiveUserSetting() != 0) // A new thrii preset to be activated
          {
            Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);
            select_thrii_preset(0);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          }
          //maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 6: // Activate the patch 2 in a bank
          if(THR_Values.getActiveUserSetting() != 1) // A new thrii preset to be activated
          {
            Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);
            select_thrii_preset(1);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          }
          //maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 7: // Activate the patch 3 in a bank
          if(THR_Values.getActiveUserSetting() != 2) // A new thrii preset to be activated
          {
            Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);
            select_thrii_preset(2);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          }
          //maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 8: // Activate the patch 4 in a bank
          if(THR_Values.getActiveUserSetting() != 3) // A new thrii preset to be activated
          {
            Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);
            select_thrii_preset(3);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          }
          //maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        case 9: // Activate the patch 5 in a bank
          if(THR_Values.getActiveUserSetting() != 4) // A new thrii preset to be activated
          {
            Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);
            select_thrii_preset(4);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          }
          button_state = 0;  // Button is handled
        break;

        // Buttons hold ============================================================
        case 11: // Rotate col/amp/cab, depending on which amp_select_mode is active
          switch(amp_select_mode)
          {
            case COL:
              THR_Values.next_col();
              Serial.println("Amp collection switched to: " + String(THR_Values.col));
            break;

            case AMP:
              THR_Values.next_amp();
              Serial.println("Amp type switched to: " + String(THR_Values.amp));
            break;

            case CAB:
              THR_Values.next_cab();
              Serial.println("Cabinet switched to: " + String(THR_Values.cab));
            break;
          }
          THR_Values.createPatch();
          maskCUpdate |= maskAmpUnit;
          button_state = 0;  // Button is handled
        break;

        case 12: // Rotate amp select mode ( COL -> AMP -> CAB -> )
          switch(amp_select_mode)
          {
              case COL:	amp_select_mode = AMP;	break;
              case AMP:	amp_select_mode = CAB; 	break;
              case CAB:	amp_select_mode = COL; 	break;
          }
          Serial.println("Amp Select Mode: " + String(amp_select_mode));
          maskCUpdate |= maskAmpSelMode;
          button_state = 0;  // Button is handled
        break;

        case 13:
          _uistate = UI_manual;
          _uistate_prev = UI_home_amp;
          maskCUpdate |= maskPatchSelMode; 
          button_state = 0;  // Button is handled
        break;

        case 14: 
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 15:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 16:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 17:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 18:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        case 19:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        default:
        break;
    }
}

// ================================================================
// User presets mode
// ================================================================
void handle_home_patch(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1: // Decrement patch to first entry in previous bank of 5 (bank size = 5)
          bank_first_patch = ((bank_first_patch-1)/5 - 1) * 5 + 1; // Decrement pre-selected patch-ID to first of next bank
          if (bank_first_patch < 1)	// Detect if went below first patch
          {
            bank_first_patch = ((npatches-1)/5) * 5 + 1; // Wrap back round to the last bank in library
          }
          presel_patch_id = bank_first_patch;
          THR_Values.boost_activated = false;
          show_patch_num = true;
          
          Serial.printf("Patch #%d pre-selected\n\r", bank_first_patch);
          maskCUpdate |= (maskPatchName | maskPatchIconBank); 
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 2: 
          bank_first_patch = ((bank_first_patch-1)/5 + 1) * 5 + 1; // Increment pre-selected patch-ID to first of next bank
          if (bank_first_patch > npatches) // Detect if went beyond last patch
          {
              bank_first_patch = 1;	// Wrap back round to first patch in library
          }
          presel_patch_id = bank_first_patch;
          THR_Values.boost_activated = false;
          show_patch_num = true;
          Serial.printf("Patch #%d pre-selected\n\r", bank_first_patch);
          maskCUpdate |= (maskPatchName | maskPatchIconBank); 
          button_state = 0;  // Remove flag, because it is handled      
        break;

        case 3: // Toggle between amp and custom patches
          _uistate = UI_home_amp;
          select_thrii_preset(nUserPreset);
          maskCUpdate = maskAll; 
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;         
          button_state = 0;          // Remove flag, because it is handled
        break;
            
        case 5: // Activate the patch 1 in a bank
          select_user_patch( presel_patch_id, 0 );
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 6: // Activate the patch 2 in a bank
          select_user_patch( presel_patch_id, 1 );
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 7: // Activate the patch 3 in a bank
          select_user_patch( presel_patch_id, 2 );
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 8: // Activate the patch 4 in a bank
          select_user_patch( presel_patch_id, 3 );
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        case 9: // Activate the patch 5 in a bank
          select_user_patch( presel_patch_id, 4 );
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        // Buttons hold ============================================================
        case 11: // Rotate col/amp/cab, depending on which amp_select_mode is active
          switch(amp_select_mode)
          {
            case COL:
              THR_Values.next_col();
              Serial.println("Amp collection switched to: " + String(THR_Values.col));
            break;

            case AMP:
              THR_Values.next_amp();
              Serial.println("Amp type switched to: " + String(THR_Values.amp));
            break;

            case CAB:
              THR_Values.next_cab();
              Serial.println("Cabinet switched to: " + String(THR_Values.cab));
            break;
          }
          THR_Values.createPatch();
          maskCUpdate |= maskAmpUnit;
          button_state = 0;  // Button is handled
        break;

        case 12: // Rotate amp select mode ( COL -> AMP -> CAB -> )
          switch(amp_select_mode)
          {
              case COL:	amp_select_mode = AMP;	break;
              case AMP:	amp_select_mode = CAB; 	break;
              case CAB:	amp_select_mode = COL; 	break;
          }
          Serial.println("Amp Select Mode: " + String(amp_select_mode));
          maskCUpdate |= maskAmpSelMode; 
          button_state = 0;  // Button is handled
        break;

        case 13:
          _uistate = UI_manual;
          _uistate_prev = UI_home_patch;
          maskCUpdate |= maskPatchSelMode; 
          button_state = 0;  // Button is handled
        break;

        case 14: 
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 15:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 16:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 17:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 18:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        case 19:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;
        
        default:
        break;
    }
}

// ================================================================
// Manual mode
// ================================================================
void handle_patch_manual(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1:
          maskCUpdate = maskAll; 
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 2: 
          maskCUpdate = maskAll; 
          button_state = 0;  // Remove flag, because it is handled      
        break;

        case 3: // Toggle between...?
          maskCUpdate = maskAll; 
          button_state = 0;  // Remove flag, because it is handled
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;    
          button_state = 0;          // Remove flag, because it is handled
        break;
            
        case 5:
          toggle_boost();
          maskCUpdate |= (maskPatchName | maskGainMaster | maskEQChart);
          button_state = 0;  // Button is handled
        break;

        case 6: // Toggle the Compressor Unit
          if(THR_Values.unit[COMPRESSOR])	{ Serial.println("Compressor unit switched off"); }
          else                            { Serial.println("Compressor unit switched on");  }
          THR_Values.Switch_On_Off_Compressor_Unit( !THR_Values.unit[COMPRESSOR] );
          maskCUpdate |= maskCompressor;
          button_state = 0;  // Button is handled
        break;

        case 7: // Toggle Effect
          if(THR_Values.unit[EFFECT]) { Serial.println("Effect unit switched off"); }
          else                        { Serial.println("Effect unit switched on");  }
          THR_Values.Switch_On_Off_Effect_Unit( !THR_Values.unit[EFFECT] );
          maskCUpdate |= maskFxUnit; 
          button_state = 0;  // Button is handled
        break;

        case 8: // Toggle Echo
          if(THR_Values.unit[ECHO]) { Serial.println("Echo unit switched off"); }
          else                      { Serial.println("Echo unit switched on");  }
          THR_Values.Switch_On_Off_Echo_Unit( ! THR_Values.unit[ECHO] );
          maskCUpdate |= maskEcho; 
          button_state = 0;  // Button is handled
        break;
        
        case 9: // Toggle Reverb
          if(THR_Values.unit[REVERB]) { Serial.println("Reverb unit switched off"); }
          else                        { Serial.println("Reverb unit switched on");  }
          THR_Values.Switch_On_Off_Reverb_Unit( !THR_Values.unit[REVERB] );
          maskCUpdate |= maskReverb; 
          button_state = 0;  // Button is handled
        break;

        // Buttons hold ============================================================
        case 11: // Rotate col/amp/cab, depending on which amp_select_mode is active
          switch(amp_select_mode)
          {
            case COL:
              THR_Values.next_col();
              Serial.println("Amp collection switched to: " + String(THR_Values.col));
            break;

            case AMP:
              THR_Values.next_amp();
              Serial.println("Amp type switched to: " + String(THR_Values.amp));
            break;

            case CAB:
              THR_Values.next_cab();
              Serial.println("Cabinet switched to: " + String(THR_Values.cab));
            break;
          }
          THR_Values.createPatch();
          maskCUpdate |= maskAmpUnit;
          button_state = 0;  // Button is handled
        break;

        case 12: // Rotate amp select mode ( COL -> AMP -> CAB -> )
          switch(amp_select_mode)
          {
              case COL:	amp_select_mode = AMP;	break;
              case AMP:	amp_select_mode = CAB; 	break;
              case CAB:	amp_select_mode = COL; 	break;
          }
          Serial.println("Amp Select Mode: " + String(amp_select_mode));
          maskCUpdate |= maskAmpSelMode;
          button_state = 0;  // Button is handled
        break;

        case 13:
          _uistate = _uistate_prev;
          maskCUpdate |= maskPatchSelMode;
          button_state = 0;  // Button is handled
        break;

        case 14: 
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 15:
          maskCUpdate = maskAll; 
          button_state = 0;  // Button is handled
        break;

        case 16: // Toggle the Gate Unit
          if(THR_Values.unit[GATE])	{ Serial.println("Gate unit switched off"); }
          else                      { Serial.println("Gate unit switched on");  }
          THR_Values.Switch_On_Off_Gate_Unit( !THR_Values.unit[GATE] );
          maskCUpdate |= maskNoiseGate;
          button_state = 0;  // Button is handled
        break;

        case 17: // Rotate Effect Mode ( Chorus > Flanger > Phaser > Tremolo > )
          switch(THR_Values.effecttype)
          {
            case CHORUS:  THR_Values.EffectSelect(FLANGER); break;
            case FLANGER: THR_Values.EffectSelect(PHASER);  break;
            case PHASER:  THR_Values.EffectSelect(TREMOLO); break;
            case TREMOLO: THR_Values.EffectSelect(CHORUS);  break;
          }
          THR_Values.createPatch();
          Serial.println("Effect unit switched to: " + String(THR_Values.effecttype));
          maskCUpdate |= maskFxUnit;
          button_state = 0;  // Button is handled
        break;

        case 18: // Rotate Echo Mode ( Tape Echo > Digital Delay > )
          switch(THR_Values.echotype)
          {
            case TAPE_ECHO:     THR_Values.EchoSelect(DIGITAL_DELAY); break;
            case DIGITAL_DELAY: THR_Values.EchoSelect(TAPE_ECHO);     break;
          }
          THR_Values.createPatch();
          Serial.println("Echo unit switched to: " + String(THR_Values.echotype));
          maskCUpdate |= maskEcho;
          button_state = 0;  // Button is handled
        break;
        
        case 19: // Rotate Reverb Mode ( Spring > Room > Plate > Hall > )
          switch(THR_Values.reverbtype)
          {
            case SPRING: THR_Values.ReverbSelect(ROOM);   break;
            case ROOM:   THR_Values.ReverbSelect(PLATE);  break;
            case PLATE:  THR_Values.ReverbSelect(HALL);   break;
            case HALL:   THR_Values.ReverbSelect(SPRING); break;
          }
          THR_Values.createPatch();
          Serial.println("Reverb unit switched to: " + String(THR_Values.reverbtype));
          maskCUpdate |= maskReverb; 
          button_state = 0;  // Button is handled
        break;
        
        default:
        break;
    }
}
