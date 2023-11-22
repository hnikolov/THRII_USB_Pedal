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

extern uint16_t npatches; // Counts the patches stored on SD-card or in PROGMEN
extern int8_t nUserPreset; // Used to cycle the THRII User presets

int16_t presel_patch_id;   	 // ID of actually pre-selected patch (absolute number)
int16_t active_patch_id;   	 // ID of actually selected patch     (absolute number)
bool send_patch_now = false; // pre-select patch to send (false) or send immediately (true)

extern byte maskUpdate;

/////////////////////////////////////////////////////////////////

std::array<byte, 29> ask_preset_buf = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// Switch to preset #4 (__0x03__)
std::array<byte, 29> switch_preset_buf = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x01, 0x08, 0x00, 0x0e, 0x0b, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// TODO: Switch col/amp:  Classic Lead
// std::array<byte, 29> sendbuf_head = {0xf0, 0x00, 0x01, 0x0c, 0x24, 0x01, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
// std::array<byte, 29> sendbuf_body = {0xf0, 0x00, 0x01, 0x00, 0x24, 0x00, 0x4d, 0x00, 0x04, 0x00, 0x00, 0x07, 0x04, 0x0c, 0x01, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};

//                0        1            2              3        4        5        6                7
// enum UIStates {UI_idle, UI_home_amp, UI_home_patch, UI_edit, UI_save, UI_name, UI_init_act_set, UI_act_vol_sol, UI_patch, UI_ded_sol, UI_pat_vol_sol};

void fsm_10b_1(UIStates &_uistate, uint8_t &button_state) 
{
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
                THR_Values.next_col();
                /*
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
                */
                Serial.println("Amp collection switched to: " + String(THR_Values.col));
							break;

							case AMP:
                THR_Values.next_amp();
								/*
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
                */
                Serial.println("Amp type switched to: " + String(THR_Values.amp));
							break;

							case CAB:
                THR_Values.next_cab();
                /*
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
								*/
                Serial.println("Cabinet switched to: " + String(THR_Values.cab));
							break;
						}
						THR_Values.createPatch();
            //outqueue.enqueue(Outmessage(SysExMessage(sendbuf_head.data(), sendbuf_head.size()), 88, false, false));
            //outqueue.enqueue(Outmessage(SysExMessage(sendbuf_body.data(), sendbuf_body.size()), 88, false, true));
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
            // Create a function activate_user_preset(nr)
            _uistate = UI_home_amp;
            nUserPreset++;
            if( nUserPreset > 4 ) { nUserPreset = 0; }
            
            THR_Values.setActiveUserSetting(nUserPreset);
            THR_Values.setUserSettingsHaveChanged(false);
            THR_Values.thrSettings = false;
            THR_Values.boost_activated = false;

            // Switch to User preset ASAP, TODO: Check for timeouts...
            switch_preset_buf[22] = (byte)nUserPreset;
            outqueue.enqueue(Outmessage(SysExMessage(switch_preset_buf.data(), switch_preset_buf.size()), 88, false, true));

            // Request User preset data if not already done
            if(thr_user_presets[nUserPreset].getActiveUserSetting() == -1)
            {
              Serial.println("Requesting data for User preset #" + String(nUserPreset));

              // Store to THR_Values and thr_user_presets[nUserPreset]
              ask_preset_buf[22] = (byte)nUserPreset;
              outqueue.enqueue(Outmessage(SysExMessage(ask_preset_buf.data(), ask_preset_buf.size()), 88, false, true));
            }
            else
            {
              // Just update the display with the User preset data
              THR_Values = thr_user_presets[nUserPreset];
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
}