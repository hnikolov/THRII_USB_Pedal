/////////////////////////////////////////////////////////////////////////////////////
// patchSetAll()
/////////////////////////////////////////////////////////////////////////////////////
#include <algorithm> // fstd::ind_if()
#include "THR30II_Pedal.h"
#include "trace.h"

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

/////////////////////////////////////////////////////////////////////////////

static std::map<uint16_t, string_or_ulong> globals; // Global settings from a patch-dump
std::map<uint16_t, Dumpunit> units; // Ushort value for UnitKey
uint16_t actualUnitKey = 0, actualSubunitKey = 0;
static String logg; // The log while analyzing dump

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

// Get data, when in state "Global"
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

// Get data, when in state "Unit"
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
} // end of patch_setAll