#include <algorithm> // fstd::ind_if()
#include "THR30II.h"
#include "trace.h"

THR30II_Settings::THR30II_Settings() // Constructor
{
	dumpFrameNumber = 0; 
	dumpByteCount = 0; // Received bytes
	dumplen = 0;       // Expected length in bytes
  Firmware = 0x00000000;
  ConnectedModel = 0x00000000; // FamilyID (2 Byte) + ModelNr. (2 Byte): 0x0024_0002=THR30II
	unit[EFFECT]=false; unit[ECHO]=false; unit[REVERB]=false; unit[COMPRESSOR]=false; unit[GATE]=false;
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
		TRACE_THR30IIPEDAL(Serial.println(tempotapsetting);)
		tempotapbpm = 60000 / tempotapint;
		TRACE_THR30IIPEDAL(Serial.println(tempotapbpm);)

		echo_setting[TAPE_ECHO][TA_TIME] = tempotapsetting;
		echo_setting[DIGITAL_DELAY][DD_TIME] = tempotapsetting;
		createPatch();
    
    // Set only the time parameter(s) iso sending a complete patch
    //std::map<String,uint16_t> &glob = Constants::glo;
    //EchoSetting(TAPE_ECHO,     glob["Time"], tempotapsetting);
    //EchoSetting(DIGITAL_DELAY, glob["Time"], tempotapsetting);
	}
}

///////////////////////////////////////////////////////////////////////
// The setters for locally stored THR30II-Settings class
///////////////////////////////////////////////////////////////////////

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
    guitar_volume = _g_vol;
}

void THR30II_Settings::SetAudioVol(uint16_t _a_vol)
{
    audio_volume = _a_vol;
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
		SendParameterSetting((un_cmd){THR30II_UNITS_VALS[GATE].key, THR30II_GATE_VALS[ctrl]}, (type_val<double>) {0x04, value}); // Send reverb type change to THR
	}
}

void THR30II_Settings::CompressorSetting(THR30II_COMP ctrl, double value) // Setter for compressor effect parameters
{
	compressor_setting[ctrl] = constrain(value, 0, 100);

	if( sendChangestoTHR )
	{
    SendParameterSetting((un_cmd){THR30II_UNITS_VALS[COMPRESSOR].key, THR30II_COMP_VALS[ctrl]}, (type_val<double>){0x04, value}); // Send settings change to THR
    /*
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
			SendParameterSetting((un_cmd){THR30II_UNITS_VALS[COMPRESSOR].key, THR30II_COMP_VALS[CO_MIX]}, (type_val<double>){0x04, value}); // Send settings change to THR
		}
    */
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
}

void THR30II_Settings::EffectSelect(THR30II_EFF_TYPES type) // Setter for selection of the Effect type
{
	effecttype = type;
}

void THR30II_Settings::EchoSelect(THR30II_ECHO_TYPES type) // Setter for selection of the Effect type
{
	echotype = type;
}

void THR30II_Settings::SetPatchName(String nam, int nr) // nr = -1 as default for actual patchname
{														                            // corresponds to field "activeUserSetting"
	TRACE_THR30IIPEDAL(Serial.println(String("SetPatchName(): Patchnname: ") + nam + String(" nr: ") + String(nr) );)
	
	nr = constrain(nr, 0, 5); // Make 0 based index for array patchNames[]

	patchNames[nr] = nam.length() > 64 ? nam.substring(0,64) : nam; // Cut if necessary
}

String THR30II_Settings::getPatchName()
{
   return patchNames[constrain(activeUserSetting, 0, 5)];
}

// Find the correct (col, amp)-struct object for this ampkey
col_amp THR30II_Settings::THR30IIAmpKey_ToColAmp(uint16_t ampkey)
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

//--------- FUNCTION FOR SENDING CAB SETTING TO THR30II -----------------
void THR30II_Settings::SendCab() // Send cabinet setting to THR
{
  TRACE_THR30IIPEDAL(Serial.println("Swithching Cabinet...");)
	SendParameterSetting((un_cmd) {THR30II_UNITS_VALS[THR30II_UNITS::GATE].key, THR30II_CAB_COMMAND}, (type_val<uint16_t>) {0x02, (uint16_t)cab});
}

//--------- FUNCTION FOR SENDING UNIT STATE TO THR30II -----------------
void THR30II_Settings::SendUnitState(THR30II_UNITS un) //Send unit state setting to THR30II
{
  //uint16_t ak = THR30IIAmpKeys[(col_amp){col, amp}];

	type_val<uint16_t> tmp = {(byte){0x03}, unit[un]};
	SendParameterSetting((un_cmd) {THR30II_UNITS_VALS[THR30II_UNITS::GATE].key, THR30II_UNIT_ON_OFF_COMMANDS[un]}, tmp);
}

/////////////////////////////////////////////////////////////
//void THR30II_Settings::SendColAmp() // TODO
//{
//  uint16_t ak = THR30IIAmpKeys[(col_amp){col, amp}];
//	type_val<uint16_t> tmp = {(byte){0x03}, ak};
//
//  // AMP UKey = 0x0000013C;
//	SendParameterSetting((un_cmd) {THR30II_UNITS_VALS[THR30II_UNITS::CONTROL].key, /* NONE */}, tmp);
//}
//////////////////////////////////////////////////////////////

void THR30II_Settings::next_col()
{
  switch(col)
  {
    case CLASSIC:	 col = BOUTIQUE; break;
    case BOUTIQUE: col = MODERN; 	 break;
    case MODERN:	 col = CLASSIC;  break;
  }
}

void THR30II_Settings::prev_col()
{
  switch(col)
  {
    case CLASSIC:	 col = MODERN;   break;
    case BOUTIQUE: col = CLASSIC;  break;
    case MODERN:	 col = BOUTIQUE; break;
  }
}

void THR30II_Settings::next_amp()
{
  switch(amp)
  {
    case CLEAN:		amp = CRUNCH;	 break;
    case CRUNCH:	amp = LEAD;		 break;
    case LEAD:		amp = HI_GAIN; break;
    case HI_GAIN:	amp = SPECIAL; break;
    case SPECIAL:	amp = BASS;		 break;
    case BASS:		amp = ACO;	   break;
    case ACO:	    amp = FLAT;		 break;
    case FLAT:		amp = CLEAN;   break;
  }
}

void THR30II_Settings::prev_amp()
{
  switch(amp)
  {
    case CLEAN:		amp = FLAT;    break;
    case CRUNCH:	amp = CLEAN;   break;
    case LEAD:		amp = CRUNCH;  break;
    case HI_GAIN:	amp = LEAD;	   break;
    case SPECIAL:	amp = HI_GAIN; break;
    case BASS:		amp = SPECIAL; break;
    case ACO:		  amp = BASS;	   break;
    case FLAT:		amp = ACO;	   break;
  } 
}

void THR30II_Settings::next_cab()
{
  switch(cab)
  {
    case British_4x12:	  cab = American_4x12;   break;
    case American_4x12:	  cab = Brown_4x12;      break;
    case Brown_4x12:	    cab = Vintage_4x12;	   break;
    case Vintage_4x12:	  cab = Fuel_4x12;	     break;
    case Fuel_4x12:		    cab = Juicy_4x12;	     break;
    case Juicy_4x12:	    cab = Mods_4x12;       break;
    case Mods_4x12:		    cab = American_2x12;   break;
    case American_2x12:	  cab = British_2x12;	   break;
    case British_2x12:	  cab = British_Blues;   break;
    case British_Blues:	  cab = Boutique_2x12;   break;
    case Boutique_2x12:	  cab = Yamaha_2x12;	   break;
    case Yamaha_2x12:	    cab = California_1x12; break;
    case California_1x12: cab = American_1x12;   break;
    case American_1x12:	  cab = American_4x10;   break;
    case American_4x10:	  cab = Boutique_1x12;   break;
    case Boutique_1x12:	  cab = Bypass;		       break;
    case Bypass:		      cab = British_4x12;	   break;
  }
}

void THR30II_Settings::prev_cab()
{
  switch(cab)
  {
    case British_4x12:	  cab = Bypass;          break;
    case American_4x12:	  cab = British_4x12;    break;
    case Brown_4x12:	    cab = American_4x12;   break;
    case Vintage_4x12:	  cab = Brown_4x12;      break;
    case Fuel_4x12:		    cab = Vintage_4x12;	   break;
    case Juicy_4x12:      cab = Fuel_4x12;	     break;
    case Mods_4x12:	      cab = Juicy_4x12;	     break;
    case American_2x12:	  cab = Mods_4x12;       break;
    case British_2x12:	  cab = American_2x12;   break;
    case British_Blues:	  cab = British_2x12;	   break;
    case Boutique_2x12:	  cab = British_Blues;   break;
    case Yamaha_2x12:	    cab = Boutique_2x12;   break;
    case California_1x12: cab = Yamaha_2x12;	   break;
    case American_1x12:	  cab = California_1x12; break;
    case American_4x10:	  cab = American_1x12;   break;
    case Boutique_1x12:	  cab = American_4x10;   break;
    case Bypass:		      cab = Boutique_1x12;   break; 
  }
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

String THR30II_Settings::getAmpName()
{
  String name = "Not found";
  uint16_t ca = THR30IIAmpKeys[(col_amp){col, amp}];

  for( auto &i : Constants::glo )
  {
    if( i.second == ca )
    {
      name = i.first;
      break; // Stop searching
    }
  }
  return name;
}
//////////////////
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

int THR30II_Settings::SetLoadedPatch( const JsonDocument djd ) // Invoke all settings from a loaded patch
{
	if (!MIDI_Activated)
	{
		TRACE_THR30IIPEDAL(Serial.println("Midi Sync is not ready!");)
		// return -1; // TODO: Shall we return?
	}

	sendChangestoTHR = false; // Note: We apply all the settings in one shot with a MIDI-Patch-Upload to THRII via "createPatch()"
                            //       and not each setting separately. So we use the setters only for our local fields!

	TRACE_THR30IIPEDAL(Serial.println(F("SetLoadedPatch(): Setting loaded patch..."));)

	std::map<String, uint16_t> &glob = Constants::glo;
	
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

  // NOTE: Set usable default values of all (effect, echo, reverb)
  //       In this way, all effects can be switched and used although settings not available in a preset
  SetDefaults();

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

  createPatch(); // Send all settings as a patch dump SysEx to THRII
	
	TRACE_THR30IIPEDAL(Serial.println(F("SetLoadedPatch(): Done setting."));)
	return 0;
}
////////////////

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

//--------- METHOD FOR UPLOADING PATCHNAME TO THR30II -----------------
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
  //===============================
  // MIX per effect
  //===============================
  double FX_MIX = 0.0;
  switch( effecttype )
  {
    case CHORUS:  FX_MIX = effect_setting[CHORUS][CH_MIX];  break;
    case FLANGER: FX_MIX = effect_setting[FLANGER][FL_MIX]; break;
    case PHASER:  FX_MIX = effect_setting[PHASER][PH_MIX];  break;
    case TREMOLO: FX_MIX = effect_setting[TREMOLO][TR_MIX]; break;
  }
	conbyt4(ValToNumber(FX_MIX));
	//conbyt4(ValToNumber(effect_setting[PHASER][PH_MIX])); // MIX is independent from the effect type!

	//1C01 = FX3EnableState  (EchoOn)
	conbyt2(glob["FX3EnableState"]);
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[ECHO] ? 0x01u : 0x00u);         
	// 1B01 = FX3MixState     (EchoMix)
	conbyt2(glob["FX3MixState"]);
	conbyt4(0x00030000u); // Type int

  double DL_MIX = 0.0;
  switch( echotype )
  {
    case TAPE_ECHO:     DL_MIX = echo_setting[TAPE_ECHO][TA_MIX];     break;
    case DIGITAL_DELAY: DL_MIX = echo_setting[DIGITAL_DELAY][DD_MIX]; break;
  }
	conbyt4(ValToNumber(DL_MIX));
	//conbyt4(ValToNumber(echo_setting[TAPE_ECHO][TA_MIX])); // MIX is independent from the echo type!
	
	// 1F01 = FX4EnableState  (RevOn)
	conbyt2(glob["FX4EnableState"]);		
	conbyt4(0x00010000u); // Type binary
	conbyt4(unit[REVERB] ? 0x01u : 0x00u);
	// 2601 = FX4WetSendState (RevMix)
	conbyt2(glob["FX4WetSendState"]);
	conbyt4(0x00030000u); // Type int

  double RB_MIX = 0.0;
  switch( reverbtype )
  {
    case SPRING: RB_MIX = reverb_setting[SPRING][SP_MIX]; break;
    case PLATE:  RB_MIX = reverb_setting[PLATE][PL_MIX];  break;
    case HALL:   RB_MIX = reverb_setting[HALL][HA_MIX];   break;
    case ROOM:   RB_MIX = reverb_setting[ROOM][RO_MIX];   break;
  }
	conbyt4(ValToNumber(RB_MIX));
	//conbyt4(ValToNumber(reverb_setting[SPRING][SP_MIX]) ); // MIX is independent from the effect type!
	
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

} // End of THR30II_Settings::createPatch()

void THR30II_Settings::SetDefaults()
{
  // NOTE: Set usable default values of all (effect, echo, reverb)
  //       In this way, all effects can be switched and used although settings not available in a preset
  //       Compressor, Amp, and Noise Gate are always set in a preset, no reason to set defaults
  effect_setting[TREMOLO][TR_SPEED] = 34.0;  // 27
  effect_setting[TREMOLO][TR_DEPTH] = 71.0;  // 31
  effect_setting[TREMOLO][TR_MIX]   = 41.0;  // 62

  effect_setting[CHORUS][CH_SPEED]    = 12.0;
  effect_setting[CHORUS][CH_DEPTH]    = 23.0;
  effect_setting[CHORUS][CH_PREDELAY] = 50.0;
  effect_setting[CHORUS][CH_FEEDBACK] = 12.0;
  effect_setting[CHORUS][CH_MIX]      = 45.0;

  effect_setting[FLANGER][FL_SPEED] = 4.0;
  effect_setting[FLANGER][FL_DEPTH] = 44.0;
  effect_setting[FLANGER][FL_MIX]   = 26.0;

  effect_setting[PHASER][PH_SPEED]    =  1.0;
  effect_setting[PHASER][PH_FEEDBACK] = 43.0;
  effect_setting[PHASER][PH_MIX]      = 33.0;
  // --
  echo_setting[TAPE_ECHO][TA_TIME]     = 41.0;
  echo_setting[TAPE_ECHO][TA_FEEDBACK] = 50.0;
  echo_setting[TAPE_ECHO][TA_BASS]     = 43.0;
  echo_setting[TAPE_ECHO][TA_TREBLE]   = 55.0;
  echo_setting[TAPE_ECHO][TA_MIX]      = 26.0;

  echo_setting[DIGITAL_DELAY][DD_TIME]     = 39.0;
  echo_setting[DIGITAL_DELAY][DD_FEEDBACK] = 38.0;
  echo_setting[DIGITAL_DELAY][DD_BASS]     = 26.0;
  echo_setting[DIGITAL_DELAY][DD_TREBLE]   = 37.0;
  echo_setting[DIGITAL_DELAY][DD_MIX]      = 36.0;
  // --
  reverb_setting[SPRING][SP_REVERB] = 25.0;
  reverb_setting[SPRING][SP_TONE]   = 50.0;
  reverb_setting[SPRING][SP_MIX]    = 19.0;

  reverb_setting[PLATE][PL_DECAY]    = 24.0;
  reverb_setting[PLATE][PL_PREDELAY] = 31.0;
  reverb_setting[PLATE][PL_TONE]     = 54.0;
  reverb_setting[PLATE][PL_MIX]      = 20.0;

  reverb_setting[HALL][HA_DECAY]    = 18.0;
  reverb_setting[HALL][HA_PREDELAY] = 10.0;
  reverb_setting[HALL][HA_TONE]     = 62.0;
  reverb_setting[HALL][HA_MIX]      = 45.0;

  reverb_setting[ROOM][RO_DECAY]    = 13.0;
  reverb_setting[ROOM][RO_PREDELAY] =  6.0;
  reverb_setting[ROOM][RO_TONE]     = 59.0;
  reverb_setting[ROOM][RO_MIX]      = 10.0;
}

//---------FUNCTION FOR SENDING COL/AMP SETTING TO THR30II -----------------
// TODO
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
