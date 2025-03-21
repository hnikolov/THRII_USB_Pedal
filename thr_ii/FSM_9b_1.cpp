#include "THR30II_Pedal.h"
#include "trace.h"

extern THR30II_Settings THR_Values;

extern ampSelectModes amp_select_mode;

extern std::array< THR30II_Settings, 5 > thr_user_presets;
int8_t nUserPreset = -1; // Used to cycle the THRII User presets

int16_t factory_bank_first_patch = 1; // The first patch in a bank
int16_t factory_presel_patch_id  = 1; // ID of actually pre-selected patch (absolute number)
int16_t factory_active_patch_id  = 1; // ID of actually selected patch     (absolute number)
int16_t user_bank_first_patch = 1; // The first patch in a bank
int16_t user_presel_patch_id  = 1; // ID of actually pre-selected patch (absolute number)
int16_t user_active_patch_id  = 1; // ID of actually selected patch     (absolute number)

int16_t *bank_first_patch = &user_bank_first_patch;
int16_t *presel_patch_id  = &user_presel_patch_id;
int16_t *active_patch_id  = &user_active_patch_id;

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
extern uint32_t maskAmpUnit;
extern uint32_t maskCompressor;
extern uint32_t maskNoiseGate;
extern uint32_t maskFxUnit;
extern uint32_t maskEcho;
extern uint32_t maskReverb;

// Used in Edit mode
extern uint32_t maskCompressorEn;
extern uint32_t maskNoiseGateEn;
extern uint32_t maskFxUnitEn;
extern uint32_t maskEchoEn;
extern uint32_t maskReverbEn;
extern uint32_t maskClearTft;

extern uint32_t maskAll;

extern void drawInfoBox(String info);

#include "stompBoxes.h"
extern std::vector <StompBox*> sboxes;

extern uint8_t last_updated_sbox;
uint8_t selected_sbox = RO; // The last sbox: Room Reverb

extern std::vector <JsonDocument> json_patchesII_user;
extern std::vector <JsonDocument> json_patchesII_factory;
extern std::vector <JsonDocument> *active_json_patchesII;
extern std::vector <String> libraryPatchNames;
extern std::vector <String> factoryPatchNames;
extern std::vector <String> *active_patch_names;
bool factory_presets_active = false;

extern uint16_t npatches_user;    // Counts the user patches stored on SD-card
extern uint16_t npatches_factory; // Counts the factory patches stored on SD-card
extern uint16_t *npatches;        // Reference to npathces_user or npatches_factory

#include "tabata_ui.h"
extern Tabata_Metronome tm;
extern TabataUI tmui;

extern String serializePreset( THR30II_Settings &thrs, String presetName, JsonDocument *current_doc );
extern void writeToFile( String fileName, String data );
extern std::vector <String> file_paths_user; // Full path, including file name ad extension of patches
String presetData;

UIStates _uistate_prev = UI_home_patch; // To remember amp vs custom patches state

enum presetsMode {P_THRII, P_FACTORY, P_USER};
presetsMode pMode = P_THRII;

// NOTE: Use F_ID and M_NR iso 0x24 and 0x01, see select_thrii_preset()
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
    TRACE_THR30IIPEDAL(Serial.println("Gain boost deactivated");)
  }
  else
  {
    do_gain_boost();
    TRACE_THR30IIPEDAL(Serial.println("Gain boost activated");)
  }
}

void toggle_factory_user_presets()
{
  if( factory_presets_active == true )
  {
    active_json_patchesII = &json_patchesII_user;
    active_patch_names    = &libraryPatchNames;
    npatches              = &npatches_user;
    bank_first_patch      = &user_bank_first_patch;
    presel_patch_id       = &user_presel_patch_id;
    active_patch_id       = &user_active_patch_id;
  }
  else
  {
    active_json_patchesII = &json_patchesII_factory;
    active_patch_names    = &factoryPatchNames;
    npatches              = &npatches_factory;
    bank_first_patch      = &factory_bank_first_patch;
    presel_patch_id       = &factory_presel_patch_id;
    active_patch_id       = &factory_active_patch_id;
  }
  factory_presets_active = !factory_presets_active;
}

void select_thrii_preset(byte nThriiPreset)
{
  nUserPreset = nThriiPreset;
  THR_Values.setActiveUserSetting(nThriiPreset);
  THR_Values.setUserSettingsHaveChanged(false);
  THR_Values.thrSettings = false;
  THR_Values.boost_activated = false;

  // Fill in Family ID and model number
  switch_preset_buf[4] = THR_Values.F_ID;
  switch_preset_buf[5] = THR_Values.M_NR;

  // Switch to User preset ASAP, TODO: Check for timeouts...
  switch_preset_buf[22] = nThriiPreset;
  outqueue.enqueue(Outmessage(SysExMessage(switch_preset_buf.data(), switch_preset_buf.size()), 88, false, true));

  // Request User preset data if not already done
  if(thr_user_presets[nThriiPreset].getActiveUserSetting() == -1)
  {
    TRACE_THR30IIPEDAL(Serial.println("Requesting data for User preset #" + String(nThriiPreset));)

    // Fill in Family ID and model number
    ask_preset_buf[4] = THR_Values.F_ID;
    ask_preset_buf[5] = THR_Values.M_NR;

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

void select_user_patch(int16_t *presel_patch_id, uint8_t pnr)
{
  if( *bank_first_patch + pnr <= *npatches ) // The new patch-id is available
  {
    *presel_patch_id = *bank_first_patch + pnr;
    if( *presel_patch_id != *active_patch_id ) // A new patch to be activated
    {
      TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating patch %d ...\n\r", *presel_patch_id);)
      show_patch_num = false;
      *active_patch_id = *presel_patch_id;
      patch_activate(*presel_patch_id);
    }
    else // Solo switch
    {
      toggle_boost();
    }
  }
  else
  {
    TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating patch - Invalid patch number: %d ...\n\r", *bank_first_patch + pnr);)
  }
}

// Forward declarations
void handle_home_amp(UIStates &_uistate, uint8_t &button_state);
void handle_home_patch(UIStates &_uistate, uint8_t &button_state);
void handle_patch_manual(UIStates &_uistate, uint8_t &button_state);
void handle_edit_mode(UIStates &_uistate, uint8_t &button_state);
void handle_metronome_mode(UIStates &_uistate, uint8_t &button_state);

// enum UIStates {UI_idle, UI_home_amp, UI_home_patch, UI_manual, UI_edit, UI_save, UI_name, UI_metronome, UI_tabata};
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
        handle_edit_mode(_uistate, button_state);
			break;

			case UI_save:
			break;

			case UI_name:
			break;

      case UI_metronome:
      case UI_tabata:
        handle_metronome_mode(_uistate, button_state);
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
        break;

        case 2: 
          maskCUpdate = maskAll;
        break;

        case 3: // Switch to Manual mode
          _uistate = UI_manual;
          _uistate_prev = UI_home_amp;
          drawInfoBox("Manual mode");
          // maskCUpdate |= (maskPatchSelMode | maskAmpSelMode); // Triggered by drawInfoBox()
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;         
        break;
            
        case 5: // Activate the patch 1 in a bank
          if(THR_Values.getActiveUserSetting() != 0) // A new thrii preset to be activated
          {
            TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating thrii preset %d ...\n\r", 1);)
            select_thrii_preset(0);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster);
          }
        break;

        case 6: // Activate the patch 2 in a bank
          if(THR_Values.getActiveUserSetting() != 1) // A new thrii preset to be activated
          {
            TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating thrii preset %d ...\n\r", 2);)
            select_thrii_preset(1);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster);
          }
        break;

        case 7: // Activate the patch 3 in a bank
          if(THR_Values.getActiveUserSetting() != 2) // A new thrii preset to be activated
          {
            TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating thrii preset %d ...\n\r", 3);)
            select_thrii_preset(2);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster);
          }
        break;

        case 8: // Activate the patch 4 in a bank
          if(THR_Values.getActiveUserSetting() != 3) // A new thrii preset to be activated
          {
            TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating thrii preset %d ...\n\r", 4);)
            select_thrii_preset(3);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster);
          }
        break;
        
        case 9: // Activate the patch 5 in a bank
          if(THR_Values.getActiveUserSetting() != 4) // A new thrii preset to be activated
          {
            TRACE_THR30IIPEDAL(Serial.printf("\n\rActivating thrii preset %d ...\n\r", 5);)
            select_thrii_preset(4);
            maskCUpdate = maskAll;
          }
          else // Solo switch
          {
            toggle_boost();
            maskCUpdate |= (maskPatchName | maskGainMaster);
          }
        break;

        // Buttons hold ============================================================
        case 11: // Cycle between (amp -> user -> factory) presets
          switch( pMode )
          {
            case P_THRII:
              pMode = P_USER;
              drawInfoBox("User presets");
              _uistate = UI_home_patch;
              factory_presets_active = true;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;

            case P_USER:
              pMode = P_FACTORY;
              drawInfoBox("Factory presets");
              _uistate = UI_home_patch;
              factory_presets_active = false;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;
              
            case P_FACTORY: // Note: This check not needed here
              pMode = P_THRII;
              drawInfoBox("THRII presets");
              select_thrii_preset( nUserPreset );              
              break;
          }
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 12: // Cycle between (amp -> factory -> user) presets
          switch( pMode )
          {
            case P_THRII:
              pMode = P_FACTORY;
              drawInfoBox("Factory presets");
              _uistate = UI_home_patch;
              factory_presets_active = false;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;

            case P_FACTORY:
              pMode = P_USER;
              drawInfoBox("User presets");
              _uistate = UI_home_patch;  
              factory_presets_active = true;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);      
              break;

            case P_USER: // Note: This check not needed here
              pMode = P_THRII;
              drawInfoBox("THRII presets");
              select_thrii_preset( nUserPreset );        
              break;
          }
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 13: // Select the Edit mode
          _uistate = UI_edit;
          _uistate_prev = UI_home_amp;
          selected_sbox = AM; // Amp unit
          maskCUpdate = (maskClearTft | maskAmpUnit | maskGainMaster);
        break;

        case 14:
          TRACE_THR30IIPEDAL(Serial.println("Activating Metronome mode");)
          _uistate = UI_metronome;
          _uistate_prev = UI_home_amp;
          maskCUpdate = maskAll;
          tmui.draw_metronome();
        break;

        case 15:
          maskCUpdate = maskAll; 
        break;

        case 16:
          maskCUpdate = maskAll; 
        break;

        case 17:
          maskCUpdate = maskAll; 
        break;

        case 18:
          maskCUpdate = maskAll; 
        break;
        
        case 19:
          maskCUpdate = maskAll; 
        break;
        
        default:
        break;
    }
    button_state = 0; // Button is handled
}

// ================================================================
// User presets mode
// ================================================================
void handle_home_patch(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1: // Decrement patch to first entry in previous bank of 5 (bank size = 5)
          *bank_first_patch = ((*bank_first_patch-1)/5 - 1) * 5 + 1; // Decrement pre-selected patch-ID to first of next bank
          if (*bank_first_patch < 1)	// Detect if went below first patch
          {
            *bank_first_patch = ((*npatches-1)/5) * 5 + 1; // Wrap back round to the last bank in library
          }
          *presel_patch_id = *bank_first_patch;
          THR_Values.boost_activated = false;
          show_patch_num = true;
          
          TRACE_THR30IIPEDAL(Serial.printf("Patch #%d pre-selected\n\r", *bank_first_patch);)
          maskCUpdate |= (maskPatchName | maskPatchIconBank); 
        break;

        case 2: 
          *bank_first_patch = ((*bank_first_patch-1)/5 + 1) * 5 + 1; // Increment pre-selected patch-ID to first of next bank
          if (*bank_first_patch > *npatches) // Detect if went beyond last patch
          {
              *bank_first_patch = 1;	// Wrap back round to first patch in library
          }
          *presel_patch_id = *bank_first_patch;
          THR_Values.boost_activated = false;
          show_patch_num = true;
          TRACE_THR30IIPEDAL(Serial.printf("Patch #%d pre-selected\n\r", *bank_first_patch);)
          maskCUpdate |= (maskPatchName | maskPatchIconBank); 
        break;

        case 3: // Switch to Manual mode
          _uistate = UI_manual;
          _uistate_prev = UI_home_patch;
          drawInfoBox("Manual mode");
          // maskCUpdate |= (maskPatchSelMode | maskAmpSelMode); // Triggered by drawInfoBox()
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;         
        break;
            
        case 5: // Activate the patch 1 in a bank
          select_user_patch( presel_patch_id, 0 );
          maskCUpdate = maskAll; 
        break;

        case 6: // Activate the patch 2 in a bank
          select_user_patch( presel_patch_id, 1 );
          maskCUpdate = maskAll; 
        break;

        case 7: // Activate the patch 3 in a bank
          select_user_patch( presel_patch_id, 2 );
          maskCUpdate = maskAll; 
        break;

        case 8: // Activate the patch 4 in a bank
          select_user_patch( presel_patch_id, 3 );
          maskCUpdate = maskAll; 
        break;
        
        case 9: // Activate the patch 5 in a bank
          select_user_patch( presel_patch_id, 4 );
          maskCUpdate = maskAll; 
        break;

        // Buttons hold ============================================================
        case 11: // Cycle between (amp -> user -> factory) presets
          switch( pMode )
          {
            case P_THRII: // Note: This check not needed here
              pMode = P_USER;
              drawInfoBox("User presets");
              factory_presets_active = true;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;

            case P_USER:
              pMode = P_FACTORY;
              drawInfoBox("Factory presets");
              factory_presets_active = false;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;
              
            case P_FACTORY:
              pMode = P_THRII;
              drawInfoBox("THRII presets");
              _uistate = UI_home_amp;
              select_thrii_preset( nUserPreset );              
              break;
          }
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 12: // Cycle between (amp -> factory -> user) presets
          switch( pMode )
          {
            case P_THRII: // Note: This check not needed here
              pMode = P_FACTORY;
              drawInfoBox("Factory presets");
              factory_presets_active = false;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);
              break;

            case P_FACTORY:
              pMode = P_USER;
              drawInfoBox("User presets");
              factory_presets_active = true;
              toggle_factory_user_presets();
              *active_patch_id = *presel_patch_id;
              patch_activate(*presel_patch_id);      
              break;

            case P_USER:
              pMode = P_THRII;
              drawInfoBox("THRII presets");
              _uistate = UI_home_amp;
              select_thrii_preset( nUserPreset );        
              break;
          }
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 13: // Switch to Edit mode
          _uistate = UI_edit;
          _uistate_prev = UI_home_patch;
          selected_sbox = AM; // Amp unit
          maskCUpdate = (maskClearTft | maskAmpUnit | maskGainMaster);
        break;

        case 14:
          TRACE_THR30IIPEDAL(Serial.println("Activating Metronome mode");)
          _uistate = UI_metronome;
          _uistate_prev = UI_home_patch;
          maskCUpdate = maskAll;
          tmui.draw_metronome();
        break;

        case 15:
          maskCUpdate = maskAll; 
        break;

        case 16:
          maskCUpdate = maskAll; 
        break;

        case 17:
          maskCUpdate = maskAll; 
        break;

        case 18:
          maskCUpdate = maskAll; 
        break;
        
        case 19:
          maskCUpdate = maskAll; 
        break;
        
        default:
        break;
    }
    button_state = 0; // Button is handled
}

// ================================================================
// Manual mode
// ================================================================
void handle_patch_manual(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1:
          if( _uistate_prev != UI_home_amp )
          {
            *presel_patch_id = *presel_patch_id - 1;
            if( *presel_patch_id < 1 ) // Detect if went  below first patch
            {
              *presel_patch_id = *npatches;	// Wrap back round to the last patch in library
            }
            *bank_first_patch = ((*presel_patch_id-1)/5) * 5 + 1;
            *active_patch_id = *presel_patch_id;
            patch_activate(*presel_patch_id);
          }
          else
          {
            nUserPreset = nUserPreset - 1;
            if( nUserPreset < 0 ) { nUserPreset = 4; }
            select_thrii_preset( nUserPreset );
          }
          maskCUpdate = maskAll;
        break;

        case 2:
          if( _uistate_prev != UI_home_amp )
          {
            *presel_patch_id = *presel_patch_id + 1;
            if( *presel_patch_id > *npatches ) // Detect if went beyond last patch
            {
              *presel_patch_id = 1;	// Wrap back round to first patch in library
            }
            *bank_first_patch = ((*presel_patch_id-1)/5) * 5 + 1;
            *active_patch_id = *presel_patch_id;
            patch_activate(*presel_patch_id);
          }
          else
          {
            nUserPreset = nUserPreset + 1;
            if( nUserPreset > 4 ) { nUserPreset = 0; }
            select_thrii_preset( nUserPreset );
          }
          maskCUpdate = maskAll;
        break;

        case 3: // Switch to previous mode (amp/presets)
          _uistate = _uistate_prev;
          drawInfoBox("Presets mode");
          // maskCUpdate |= (maskPatchSelMode | maskAmpSelMode); // Triggered by drawInfoBox()
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          maskCUpdate |= maskEcho;    
        break;
            
        case 5: // Toggle the Compressor Unit
          if(THR_Values.unit[COMPRESSOR])	{ TRACE_THR30IIPEDAL(Serial.println("Compressor unit switched off");) }
          else                            { TRACE_THR30IIPEDAL(Serial.println("Compressor unit switched on");)  }
          THR_Values.Switch_On_Off_Compressor_Unit( !THR_Values.unit[COMPRESSOR] );
          maskCUpdate |= maskCompressor;
        break;

        case 6:
          toggle_boost();
          maskCUpdate |= (maskPatchName | maskGainMaster);        
        break;

        case 7: // Toggle Effect
          if(THR_Values.unit[EFFECT]) { TRACE_THR30IIPEDAL(Serial.println("Effect unit switched off");) }
          else                        { TRACE_THR30IIPEDAL(Serial.println("Effect unit switched on");)  }
          THR_Values.Switch_On_Off_Effect_Unit( !THR_Values.unit[EFFECT] );
          maskCUpdate |= maskFxUnit; 
        break;

        case 8: // Toggle Echo
          if(THR_Values.unit[ECHO]) { TRACE_THR30IIPEDAL(Serial.println("Echo unit switched off");) }
          else                      { TRACE_THR30IIPEDAL(Serial.println("Echo unit switched on");)  }
          THR_Values.Switch_On_Off_Echo_Unit( ! THR_Values.unit[ECHO] );
          maskCUpdate |= maskEcho; 
        break;
        
        case 9: // Toggle Reverb
          if(THR_Values.unit[REVERB]) { TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched off");) }
          else                        { TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched on");)  }
          THR_Values.Switch_On_Off_Reverb_Unit( !THR_Values.unit[REVERB] );
          maskCUpdate |= maskReverb; 
        break;

        // Buttons hold ============================================================
        case 11: // Rotate col/amp/cab, depending on which amp_select_mode is active
          switch(amp_select_mode)
          {
            case COL:
              THR_Values.next_col();
              switch(THR_Values.col)
              {
                  case CLASSIC:  drawInfoBox("Col: CLASSIC");  break;
                  case BOUTIQUE: drawInfoBox("Col: BOUTIQUE"); break;
                  case MODERN:   drawInfoBox("Col: MODERN");   break;
              }
              TRACE_THR30IIPEDAL(Serial.println("Amp collection switched to: " + String(THR_Values.col));)
            break;

            case AMP:
              THR_Values.next_amp();
              TRACE_THR30IIPEDAL(Serial.println("Amp type switched to: " + String(THR_Values.amp));)
              maskCUpdate |= maskAmpUnit;
            break;

            case CAB:
              THR_Values.next_cab();
              TRACE_THR30IIPEDAL(Serial.println("Cabinet switched to: " + String(THR_Values.cab));)
              maskCUpdate |= maskAmpUnit;
            break;
          }
          THR_Values.createPatch();
          // maskCUpdate |= maskAmpUnit;
        break;

        case 12: // Rotate amp select mode ( COL -> AMP -> CAB -> )
          switch(amp_select_mode)
          {
              case COL:	amp_select_mode = AMP; drawInfoBox("Amp type");        break;
              case AMP:	amp_select_mode = CAB; drawInfoBox("Speaker Cabinet"); break;
              case CAB:	amp_select_mode = COL; drawInfoBox("Amp collection");  break;
          }
          TRACE_THR30IIPEDAL(Serial.println("Amp Select Mode: " + String(amp_select_mode));)
          // maskCUpdate |= maskAmpSelMode; // Triggered by drawInfoBox()
        break;

        case 13:
          _uistate = UI_edit;
          // _uistate_prev not set here. Will return to the previous-previuos state; Patch name not updated when returning to manual mode
          // ALso, in edit mode, we need to detect that we came there from user presets, so we can write changes
          selected_sbox = AM; // Amp unit
          maskCUpdate = (maskClearTft | maskAmpUnit | maskGainMaster);
        break;

        case 14:
          // _uistate_prev not set here. Will return to the previous-previuos state, issues when returning to manual mode
          TRACE_THR30IIPEDAL(Serial.println("Activating Metronome mode");)
          _uistate = UI_metronome;
          maskCUpdate = maskAll;
          tmui.draw_metronome();
        break;

        case 15:
          maskCUpdate = maskAll; 
        break;

        case 16: // Toggle the Gate Unit
          if(THR_Values.unit[GATE])	{ TRACE_THR30IIPEDAL(Serial.println("Gate unit switched off");) }
          else                      { TRACE_THR30IIPEDAL(Serial.println("Gate unit switched on");)  }
          THR_Values.Switch_On_Off_Gate_Unit( !THR_Values.unit[GATE] );
          maskCUpdate |= maskNoiseGate;
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
          TRACE_THR30IIPEDAL(Serial.println("Effect unit switched to: " + String(THR_Values.effecttype));)
          maskCUpdate |= maskFxUnit;
        break;

        case 18: // Rotate Echo Mode ( Tape Echo > Digital Delay > )
          switch(THR_Values.echotype)
          {
            case TAPE_ECHO:     THR_Values.EchoSelect(DIGITAL_DELAY); break;
            case DIGITAL_DELAY: THR_Values.EchoSelect(TAPE_ECHO);     break;
          }
          THR_Values.createPatch();
          TRACE_THR30IIPEDAL(Serial.println("Echo unit switched to: " + String(THR_Values.echotype));)
          maskCUpdate |= maskEcho;
        break;
        
        case 19: // Rotate Reverb Mode ( Spring > Room > Plate > Hall > )
          switch(THR_Values.reverbtype)
          {
            case SPRING: THR_Values.ReverbSelect(PLATE);  break;
            case PLATE:  THR_Values.ReverbSelect(HALL);   break;
            case HALL:   THR_Values.ReverbSelect(ROOM);   break;
            case ROOM:   THR_Values.ReverbSelect(SPRING); break;
          }
          THR_Values.createPatch();
          TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched to: " + String(THR_Values.reverbtype));)
          maskCUpdate |= maskReverb; 
        break;
        
        default:
        break;
    }
    button_state = 0; // Button is handled
}

// ================================================================
// Edit mode
// ================================================================
void handle_edit_mode(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1:
          sboxes[selected_sbox]->setFocusPrev();
        break;

        case 2:
          sboxes[selected_sbox]->setFocusNext();
        break;

        case 3: // Switch to previous mode (amp/presets) - cancel the edit mode
          last_updated_sbox = RO; // The last sbox: ROOM REVERB
          _uistate = _uistate_prev;
          drawInfoBox("Not stored");
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 4: // Tap tempo
          THR_Values.EchoTempoTap(); // Get tempo tap input and apply to echo unit
          selected_sbox = TA;
          maskCUpdate |= maskEcho;
        break;

        case 5: // Edit the Compressor Unit
          if( selected_sbox != CO )
          {
            TRACE_THR30IIPEDAL(Serial.println("Compressor selected");)
            selected_sbox = CO;
            maskCUpdate |= (maskCompressor | maskCompressorEn);
          }
          else
          {
            if(THR_Values.unit[COMPRESSOR])	{ TRACE_THR30IIPEDAL(Serial.println("Compressor unit switched off");) }
            else                            { TRACE_THR30IIPEDAL(Serial.println("Compressor unit switched on");)  }
            THR_Values.Switch_On_Off_Compressor_Unit( !THR_Values.unit[COMPRESSOR] );
            maskCUpdate |= maskCompressorEn;
          }
          break;

        case 6: // Edit the Amp
          if( selected_sbox != AM )
          {
            selected_sbox = AM;
            maskCUpdate |= (maskAmpUnit | maskGainMaster);
          }
        break;

        case 7: // Edit Effect
          if( selected_sbox < 3 || selected_sbox > 6 )
          {
            TRACE_THR30IIPEDAL(Serial.println("Effect unit selected");)
            switch(THR_Values.effecttype)
            {
              case CHORUS:  selected_sbox = CH; break;
              case FLANGER: selected_sbox = FL; break;
              case PHASER:  selected_sbox = PH; break;
              case TREMOLO: selected_sbox = TR; break;
            }
            maskCUpdate |= (maskFxUnit | maskFxUnitEn);
          }
          else
          {
            if(THR_Values.unit[EFFECT]) { TRACE_THR30IIPEDAL(Serial.println("Effect unit switched off");) }
            else                        { TRACE_THR30IIPEDAL(Serial.println("Effect unit switched on");)  }
            THR_Values.Switch_On_Off_Effect_Unit( !THR_Values.unit[EFFECT] );
            maskCUpdate |= maskFxUnitEn;
          }
        break;

        case 8: // Edit Echo
          if( selected_sbox < 7 || selected_sbox > 8 )
          {
            TRACE_THR30IIPEDAL(Serial.println("Echo unit selected");)
            switch(THR_Values.echotype)
            {
              case TAPE_ECHO:     selected_sbox = TA; break;
              case DIGITAL_DELAY: selected_sbox = DD; break;
            }
            maskCUpdate |= (maskEcho | maskEchoEn);
          }
          else
          {
            if(THR_Values.unit[ECHO]) { TRACE_THR30IIPEDAL(Serial.println("Echo unit switched off");) }
            else                      { TRACE_THR30IIPEDAL(Serial.println("Echo unit switched on");)  }
            THR_Values.Switch_On_Off_Echo_Unit( !THR_Values.unit[ECHO] );
            maskCUpdate |= maskEchoEn;
          }          
        break;

        case 9: // Edit Reverb
          if( selected_sbox < 9 || selected_sbox > 12 )
          {
            TRACE_THR30IIPEDAL(Serial.println("Echo unit selected");)
            switch(THR_Values.reverbtype)
            {
              case SPRING: selected_sbox = SP; break;
              case PLATE:  selected_sbox = PL; break;
              case HALL:   selected_sbox = HA; break;
              case ROOM:   selected_sbox = RO; break;
            }
            maskCUpdate |= (maskReverb | maskReverbEn);
          }
          else
          {
            if(THR_Values.unit[REVERB]) { TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched off");) }
            else                        { TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched on");)  }
            THR_Values.Switch_On_Off_Reverb_Unit( !THR_Values.unit[REVERB] );
            maskCUpdate |= maskReverbEn;
          } 
        break;

        // Buttons hold ============================================================
        case 11: // Rotate col/amp/cab, depending on which amp_select_mode is active
          switch(amp_select_mode)
          {
            case COL:
              THR_Values.next_col();
              TRACE_THR30IIPEDAL(Serial.println("Amp collection switched to: " + String(THR_Values.col));)
            break;

            case AMP:
              THR_Values.next_amp();
              TRACE_THR30IIPEDAL(Serial.println("Amp type switched to: " + String(THR_Values.amp));)
            break;

            case CAB:
              THR_Values.next_cab();
              TRACE_THR30IIPEDAL(Serial.println("Cabinet switched to: " + String(THR_Values.cab));)
            break;
          }
          THR_Values.createPatch();
          selected_sbox = AM;
          maskCUpdate |= (maskAmpUnit | maskGainMaster);
        break;

        case 12: // Rotate amp select mode ( COL -> AMP -> CAB -> )
          switch(amp_select_mode)
          {
              case COL:	amp_select_mode = AMP; break;
              case AMP:	amp_select_mode = CAB; break;
              case CAB:	amp_select_mode = COL; break;
          }
          TRACE_THR30IIPEDAL(Serial.println("Amp Select Mode: " + String(amp_select_mode));)
          selected_sbox = AM;
          maskCUpdate |= (maskAmpUnit | maskGainMaster);
        break;

        case 13: // Save the changes and Switch to previous mode
          // Note: Support writing changes to user presets only
          //       1) THRII presets can be written by folding the respective button on the amp
          //       2) Factory presets should not change on SD Card
          if( _uistate_prev == UI_home_patch )
          {
            drawInfoBox("Not stored");
            // Store changes in memory (as json) for both, user and factory presets
            presetData = serializePreset( THR_Values, (*active_patch_names)[*active_patch_id - 1], &(*active_json_patchesII)[*active_patch_id - 1] );
            //Serial.println( presetData );

            // Writing changes to user presets only
            if( factory_presets_active == false )
            {
              drawInfoBox("Writing preset...");
              writeToFile( file_paths_user[*active_patch_id - 1], presetData );
            }
          }
          else
          {
              drawInfoBox("Not stored");
          }
          last_updated_sbox = RO; // The last sbox: ROOM REVERB
          _uistate = _uistate_prev;
          // maskCUpdate = maskAll; // Triggered by drawInfoBox()
        break;

        case 14:
          TRACE_THR30IIPEDAL(Serial.println("Activating Metronome mode");)
          _uistate = UI_metronome;
          _uistate_prev = UI_edit;
          maskCUpdate = maskAll;
          tmui.draw_metronome();
        break;

        case 15:
          maskCUpdate = maskAll;
        break;

        case 16: // Edit the Gate Unit
          if( selected_sbox != GA )
          {
            TRACE_THR30IIPEDAL(Serial.println("Gate unit selected");)
            selected_sbox = GA;
            maskCUpdate |= (maskNoiseGate | maskNoiseGateEn);
          }
          else
          {
            if(THR_Values.unit[GATE])	{ TRACE_THR30IIPEDAL(Serial.println("Gate unit switched off");) }
            else                      { TRACE_THR30IIPEDAL(Serial.println("Gate unit switched on");)  }
            THR_Values.Switch_On_Off_Gate_Unit( !THR_Values.unit[GATE] );
            maskCUpdate |= maskNoiseGateEn;
          }
        break;

        case 17: // Rotate Effect Mode ( Chorus > Flanger > Phaser > Tremolo > )
          if( selected_sbox >= 3 && selected_sbox <= 6 )
          {
            switch(THR_Values.effecttype)
            {
              case CHORUS:  THR_Values.EffectSelect(FLANGER); selected_sbox = FL; break;
              case FLANGER: THR_Values.EffectSelect(PHASER);  selected_sbox = PH; break;
              case PHASER:  THR_Values.EffectSelect(TREMOLO); selected_sbox = TR; break;
              case TREMOLO: THR_Values.EffectSelect(CHORUS);  selected_sbox = CH; break;
            }
            THR_Values.createPatch();
            TRACE_THR30IIPEDAL(Serial.println("Effect unit switched to: " + String(THR_Values.effecttype));)
            maskCUpdate |= (maskFxUnit | maskFxUnitEn);
          }
        break;

        case 18: // Rotate Echo Mode ( Tape Echo > Digital Delay > )
          if( selected_sbox >= 7 && selected_sbox <= 8 )
          {
            switch(THR_Values.echotype)
            {
              case TAPE_ECHO:     THR_Values.EchoSelect(DIGITAL_DELAY); selected_sbox = DD; break;
              case DIGITAL_DELAY: THR_Values.EchoSelect(TAPE_ECHO);     selected_sbox = TA; break;
            }
            THR_Values.createPatch();
            TRACE_THR30IIPEDAL(Serial.println("Echo unit switched to: " + String(THR_Values.echotype));)
            maskCUpdate |= (maskEcho | maskEchoEn);
          }
        break;

        case 19: // Rotate Reverb Mode ( Spring > Room > Plate > Hall > )
          if( selected_sbox >= 9 && selected_sbox <= 12 )
          {
            switch(THR_Values.reverbtype)
            {
              case SPRING: THR_Values.ReverbSelect(PLATE);  selected_sbox = PL; break;
              case PLATE:  THR_Values.ReverbSelect(HALL);   selected_sbox = HA; break;
              case HALL:   THR_Values.ReverbSelect(ROOM);   selected_sbox = RO; break;
              case ROOM:   THR_Values.ReverbSelect(SPRING); selected_sbox = SP; break;
            }
            THR_Values.createPatch();
            TRACE_THR30IIPEDAL(Serial.println("Reverb unit switched to: " + String(THR_Values.reverbtype));)
            maskCUpdate |= (maskReverb | maskReverbEn);
          }
        break;

        default:
        break;
    }
    button_state = 0; // Button is handled
}

// ==================================================================
// Metronome/Tabata mode
// ==================================================================
void handle_metronome_mode(UIStates &_uistate, uint8_t &button_state)
{
    switch (button_state)
    {
        case 1:
          tm.metronome.prevTimeSignature();
        break;

        case 2:
          tm.metronome.nextTimeSignature();
        break;

        case 3:
          if( _uistate == UI_tabata ) { tm.toggleMetronomeInTabata(); }
        break;

        case 4: // Tap tempo
          tm.metronome.tapBPM(); // Get tempo tap input and apply to the metronome unit
        break;

        case 5:
          if( _uistate == UI_metronome ) { tm.metronome.decBPM(10); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.decBPM(10);       }
            else                    { tm.tabata.decPracticeTime(10); } // Note: Practice time
          }
        break;

        case 6:
          if( _uistate == UI_metronome ) { tm.metronome.decBPM(5); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.decBPM(5);       }
            else                    { tm.tabata.decPracticeTime(5); }
          }
        break;

        case 7: // Start / Stop
          tm.toggle();
        break;

        case 8:
          if( _uistate == UI_metronome ) { tm.metronome.incBPM(5); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.incBPM(5);       }
            else                    { tm.tabata.incPracticeTime(5); }
          }
        break;

        case 9:
          if( _uistate == UI_metronome ) { tm.metronome.incBPM(10); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.incBPM(10);       }
            else                    { tm.tabata.incPracticeTime(10); }
          }
        break;

        // Buttons hold ============================================================
        case 11:
        break;

        case 12:
        break;

        case 13:
        break;

        case 14: // Switch to previous mode - cancel the metronome/tabata mode
          // FIXME: Some issue when returning to edit mode
          if( _uistate_prev == UI_edit ) { _uistate_prev = UI_home_patch; }
          _uistate = _uistate_prev;
          tm.stop();
          maskCUpdate = maskAll;
        break;

        case 15:
          if( _uistate == UI_metronome ) { tm.metronome.decBPM(20); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.decBPM(20);   }
            else                    { tm.tabata.decRestTime(10); } // Note: Rest time
          }
        break;

        case 16:
          if( _uistate == UI_metronome ) { tm.metronome.decBPM(1); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.decBPM(1);   }
            else                    { tm.tabata.decRestTime(5); }
          }
        break;

        case 17: // Switch between Tabata and Metronome
          tm.toggleTabataMetronomeMode();
          if( _uistate == UI_metronome )   { _uistate = UI_tabata;    tmui.draw_tabata();    }
          else if( _uistate == UI_tabata ) { _uistate = UI_metronome; tmui.draw_metronome(); }
          TRACE_THR30IIPEDAL(Serial.println("Toggle Metronome/Tabata");)
        break;

        case 18:
          if( _uistate == UI_metronome ) { tm.metronome.incBPM(1); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.incBPM(1);   }
            else                    { tm.tabata.incRestTime(5); }
          }
        break;

        case 19:
          if( _uistate == UI_metronome ) { tm.metronome.incBPM(20); }
          else if( _uistate == UI_tabata )
          {
            if( tm.with_metronome ) { tm.metronome.incBPM(20);   }
            else                    { tm.tabata.incRestTime(10); }
          }
        break;

        default:
        break;
    }
    button_state = 0; // Button is handled
}
