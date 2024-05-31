#include <ArduinoJson.h> 	// For patches stored in JSON (.thrl6p) format
#include <string>
#include <vector>			  	// In some cases we will use dynamic arrays - though heap usage is problematic
							  	        // Where ever possibly we use std::array instead to keep values on stack instead
#include <SD.h>
#include "trace.h"

const int sd_chipsel = BUILTIN_SDCARD;
//File32 file;

#define path_presets_user    "/thrii/user_presets"
#define path_presets_factory "/thrii/factory_presets"

std::vector <JsonDocument> json_patchesII_user;
std::vector <JsonDocument> json_patchesII_factory;
std::vector <JsonDocument> *active_json_patchesII = &json_patchesII_user;

std::vector <String> libraryPatchNames; // All the names of the user patches stored on SD-card
std::vector <String> factoryPatchNames; // All the names of the factory patches stored on SD-card
std::vector <String> *active_patch_names = &libraryPatchNames;
std::vector <String> file_paths_factory; // Full path, including file name ad extension of patches
std::vector <String> file_paths_user;    // Full path, including file name ad extension of patches

uint16_t npatches_user = 0;    // Counts the user patches stored on SD-card
uint16_t npatches_factory = 0; // Counts the factory patches stored on SD-card
uint16_t *npatches = &npatches_user;

std::string readFile(File file)
{
  std::string str;

  while( file.available() > 0 ) 
  {
    str += (char)file.read();
  }

  return str;
}

//------------------------------------------------------
// The order of reading the files in a directory on the SD card is not guaranteed to be always the same
// Therefore, first read and sort the files based on their names. Use file name format: "01_name_etc.thrl6p"
//------------------------------------------------------
void initializeSortedPresets(const String &path, std::vector <JsonDocument> &json_patchesII, std::vector <String> &patchNames, uint16_t &npatches, std::vector <String> &file_paths)
{
  JsonDocument doc;
  File dir = SD.open(path.c_str());

  while( true )
  {
    File entry = dir.openNextFile();
    if( !entry ) { break; } // No more files
    if( entry.available() ) // Zero is returned for directories
    {
      String fpath = path + "/" + String(entry.name());
      TRACE_THR30IIPEDAL(Serial.println( fpath );)
      file_paths.push_back(fpath);
    }
    entry.close();
  }

  if( file_paths.size() > 0 )
  {
    std::sort(file_paths.begin(), file_paths.end(), [](String a, String b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });

    for(auto fpath : file_paths)
    {
      TRACE_V_THR30IIPEDAL(Serial.println(fpath);)
      File entry = SD.open(fpath.c_str(), FILE_READ);
      std::string data = readFile( entry );
      TRACE_V_THR30IIPEDAL(Serial.println( data.c_str() );)
      entry.close();

      DeserializationError dse = deserializeJson(doc, data);
      if( dse )
      {
        TRACE_THR30IIPEDAL(Serial.print("deserializeJson() failed: ");)
        TRACE_THR30IIPEDAL(Serial.println( dse.c_str() );)
        continue;
      }
      
      json_patchesII.push_back( doc );
      patchNames.push_back((const char*) (doc["data"]["meta"]["name"]));            
      TRACE_THR30IIPEDAL(Serial.println((const char*) (doc["data"]["meta"]["name"]));)
    }
    npatches = json_patchesII.size();
  }
}
//------------------------------------------------------

void init_patches_from_sdcard()
{
  if(!SD.begin(sd_chipsel))
	{
    TRACE_THR30IIPEDAL(Serial.println(F("Card failed, or not present."));)
  }
  else
	{
		TRACE_THR30IIPEDAL(Serial.println(F("Card initialized."));)
    TRACE_V_THR30IIPEDAL(Serial.print(F("VolumeBegin -> success. FAT-Type: "));)

    TRACE_THR30IIPEDAL(Serial.println("Initializing factory presets from SDCard");)
    initializeSortedPresets(path_presets_factory, json_patchesII_factory, factoryPatchNames, npatches_factory, file_paths_factory);

    TRACE_THR30IIPEDAL(Serial.println("Initializing user presets from SDCard");)
    initializeSortedPresets(path_presets_user, json_patchesII_user, libraryPatchNames, npatches_user, file_paths_user);

    TRACE_THR30IIPEDAL(Serial.printf(F("\n\rLoaded %d JSON user patches and %d JSON factory patches.\n\r"), npatches_user, npatches_factory);)
  }
}

//====================================
// Serialize preset (thrii settings) to json string
// Note: Support writing changes to user presets only
//       1) THRII presets can be written by folding the respective button on the amp
//       2) Factory presets should not be changeable
//====================================
#include "THR30II.h"
String serializePreset( THR30II_Settings &thrs, String presetName, JsonDocument *current_doc )
{
    JsonDocument doc;
    JsonObject data = doc["data"].to<JsonObject>();
    data["device"] = 2359298;
    data["device_version"] = 19988587;

    JsonObject data_meta = data["meta"].to<JsonObject>();
    data_meta["name"] = presetName; // Note: User preset name only.
    data_meta["tnid"] = 0;

    JsonObject data_tone = data["tone"].to<JsonObject>();

    JsonObject data_tone_THRGroupAmp = data_tone["THRGroupAmp"].to<JsonObject>();
    data_tone_THRGroupAmp["@asset"]  = thrs.getAmpName();
    data_tone_THRGroupAmp["Drive"]   = thrs.control[CTRL_GAIN]   / 100.0;
    data_tone_THRGroupAmp["Master"]  = thrs.control[CTRL_MASTER] / 100.0;
    data_tone_THRGroupAmp["Bass"]    = thrs.control[CTRL_BASS]   / 100.0;
    data_tone_THRGroupAmp["Mid"]     = thrs.control[CTRL_MID]    / 100.0;
    data_tone_THRGroupAmp["Treble"]  = thrs.control[CTRL_TREBLE] / 100.0;

    JsonObject data_tone_THRGroupCab    = data_tone["THRGroupCab"].to<JsonObject>();
    data_tone_THRGroupCab["@asset"]     = "speakerSimulator";
    data_tone_THRGroupCab["SpkSimType"] = thrs.cab;

    JsonObject data_tone_THRGroupFX1Compressor  = data_tone["THRGroupFX1Compressor"].to<JsonObject>();
    data_tone_THRGroupFX1Compressor["@asset"]   = "RedComp";
    data_tone_THRGroupFX1Compressor["@enabled"] = thrs.unit[COMPRESSOR];
    data_tone_THRGroupFX1Compressor["Level"]    = thrs.compressor_setting[CO_LEVEL]   / 100.0;
    data_tone_THRGroupFX1Compressor["Sustain"]  = thrs.compressor_setting[CO_SUSTAIN] / 100.0;

    JsonObject data_tone_THRGroupFX2Effect  = data_tone["THRGroupFX2Effect"].to<JsonObject>();
    data_tone_THRGroupFX2Effect["@enabled"] = thrs.unit[EFFECT];

    switch( thrs.effecttype )
    {
      case CHORUS:
          data_tone_THRGroupFX2Effect["@asset"]   = "StereoSquareChorus";
          data_tone_THRGroupFX2Effect["Freq"]     = thrs.effect_setting[CHORUS][CH_SPEED]    / 100.0;
          data_tone_THRGroupFX2Effect["Depth"]    = thrs.effect_setting[CHORUS][CH_DEPTH]    / 100.0;
          data_tone_THRGroupFX2Effect["Pre"]      = thrs.effect_setting[CHORUS][CH_PREDELAY] / 100.0;
          data_tone_THRGroupFX2Effect["Feedback"] = thrs.effect_setting[CHORUS][CH_FEEDBACK] / 100.0;
          data_tone_THRGroupFX2Effect["@wetDry"]  = thrs.effect_setting[CHORUS][CH_MIX]      / 100.0;
      break;

      case FLANGER:
          data_tone_THRGroupFX2Effect["@asset"]  = "L6Flanger";
          data_tone_THRGroupFX2Effect["Freq"]    = thrs.effect_setting[FLANGER][FL_SPEED] / 100.0;
          data_tone_THRGroupFX2Effect["Depth"]   = thrs.effect_setting[FLANGER][FL_DEPTH] / 100.0;
          data_tone_THRGroupFX2Effect["@wetDry"] = thrs.effect_setting[FLANGER][FL_MIX]   / 100.0;
      break;

      case PHASER:
          data_tone_THRGroupFX2Effect["@asset"]   = "Phaser";
          data_tone_THRGroupFX2Effect["Speed"]    = thrs.effect_setting[PHASER][PH_SPEED]    / 100.0;
          data_tone_THRGroupFX2Effect["Feedback"] = thrs.effect_setting[PHASER][PH_FEEDBACK] / 100.0;
          data_tone_THRGroupFX2Effect["@wetDry"]  = thrs.effect_setting[PHASER][PH_MIX]      / 100.0;
      break;

      case TREMOLO:
          data_tone_THRGroupFX2Effect["@asset"]  = "BiasTremolo";
          data_tone_THRGroupFX2Effect["Speed"]   = thrs.effect_setting[TREMOLO][TR_SPEED] / 100.0;
          data_tone_THRGroupFX2Effect["Depth"]   = thrs.effect_setting[TREMOLO][TR_DEPTH] / 100.0;
          data_tone_THRGroupFX2Effect["@wetDry"] = thrs.effect_setting[TREMOLO][TR_MIX]   / 100.0;
      break;
    } // of switch(effecttype)

    JsonObject data_tone_THRGroupFX3EffectEcho  = data_tone["THRGroupFX3EffectEcho"].to<JsonObject>();
    data_tone_THRGroupFX3EffectEcho["@enabled"] = thrs.unit[ECHO];

    switch( thrs.echotype )
    {
      case TAPE_ECHO:
          data_tone_THRGroupFX3EffectEcho["@asset"]   = "TapeEcho";
          data_tone_THRGroupFX3EffectEcho["Time"]     = thrs.echo_setting[TAPE_ECHO][TA_TIME]     / 100.0;
          data_tone_THRGroupFX3EffectEcho["Feedback"] = thrs.echo_setting[TAPE_ECHO][TA_FEEDBACK] / 100.0;
          data_tone_THRGroupFX3EffectEcho["Bass"]     = thrs.echo_setting[TAPE_ECHO][TA_BASS]     / 100.0;
          data_tone_THRGroupFX3EffectEcho["Treble"]   = thrs.echo_setting[TAPE_ECHO][TA_TREBLE]   / 100.0;
          data_tone_THRGroupFX3EffectEcho["@wetDry"]  = thrs.echo_setting[TAPE_ECHO][TA_MIX]      / 100.0;
      break;

      case DIGITAL_DELAY:
          data_tone_THRGroupFX3EffectEcho["@asset"]   = "L6DigitalDelay";
          data_tone_THRGroupFX3EffectEcho["Time"]     = thrs.echo_setting[DIGITAL_DELAY][DD_TIME]     / 100.0;
          data_tone_THRGroupFX3EffectEcho["Feedback"] = thrs.echo_setting[DIGITAL_DELAY][DD_FEEDBACK] / 100.0;
          data_tone_THRGroupFX3EffectEcho["Bass"]     = thrs.echo_setting[DIGITAL_DELAY][DD_BASS]     / 100.0;
          data_tone_THRGroupFX3EffectEcho["Treble"]   = thrs.echo_setting[DIGITAL_DELAY][DD_TREBLE]   / 100.0;
          data_tone_THRGroupFX3EffectEcho["@wetDry"]  = thrs.echo_setting[DIGITAL_DELAY][DD_MIX]      / 100.0;
      break;
    } // of switch(echotype)

    JsonObject data_tone_THRGroupFX4EffectReverb  = data_tone["THRGroupFX4EffectReverb"].to<JsonObject>();
    data_tone_THRGroupFX4EffectReverb["@enabled"] = thrs.unit[REVERB];

    switch( thrs.reverbtype )
    {
      case SPRING:
          data_tone_THRGroupFX4EffectReverb["@asset"]  = "StandardSpring";
          data_tone_THRGroupFX4EffectReverb["Time"]    = thrs.reverb_setting[SPRING][SP_REVERB] / 100.0;
          data_tone_THRGroupFX4EffectReverb["Tone"]    = thrs.reverb_setting[SPRING][SP_TONE]   / 100.0;
          data_tone_THRGroupFX4EffectReverb["@wetDry"] = thrs.reverb_setting[SPRING][SP_MIX]    / 100.0;
      break;

      case ROOM:
          data_tone_THRGroupFX4EffectReverb["@asset"]   = "SmallRoom1";
          data_tone_THRGroupFX4EffectReverb["Decay"]    = thrs.reverb_setting[ROOM][RO_DECAY]    / 100.0;
          data_tone_THRGroupFX4EffectReverb["PreDelay"] = thrs.reverb_setting[ROOM][RO_PREDELAY] / 100.0;
          data_tone_THRGroupFX4EffectReverb["Tone"]     = thrs.reverb_setting[ROOM][RO_TONE]     / 100.0;
          data_tone_THRGroupFX4EffectReverb["@wetDry"]  = thrs.reverb_setting[ROOM][RO_MIX]      / 100.0;
      break;

      case PLATE:
          data_tone_THRGroupFX4EffectReverb["@asset"]   = "LargePlate1";
          data_tone_THRGroupFX4EffectReverb["Decay"]    = thrs.reverb_setting[PLATE][PL_DECAY]    / 100.0;
          data_tone_THRGroupFX4EffectReverb["PreDelay"] = thrs.reverb_setting[PLATE][PL_PREDELAY] / 100.0;
          data_tone_THRGroupFX4EffectReverb["Tone"]     = thrs.reverb_setting[PLATE][PL_TONE]     / 100.0;
          data_tone_THRGroupFX4EffectReverb["@wetDry"]  = thrs.reverb_setting[PLATE][PL_MIX]      / 100.0;
      break;

      case HALL:
          data_tone_THRGroupFX4EffectReverb["@asset"]   = "ReallyLargeHall";
          data_tone_THRGroupFX4EffectReverb["Decay"]    = thrs.reverb_setting[HALL][HA_DECAY]    / 100.0;
          data_tone_THRGroupFX4EffectReverb["PreDelay"] = thrs.reverb_setting[HALL][HA_PREDELAY] / 100.0;
          data_tone_THRGroupFX4EffectReverb["Tone"]     = thrs.reverb_setting[HALL][HA_TONE]     / 100.0;
          data_tone_THRGroupFX4EffectReverb["@wetDry"]  = thrs.reverb_setting[HALL][HA_MIX]      / 100.0;
      break;
    } // of switch(reverbtype)

    JsonObject data_tone_THRGroupGate  = data_tone["THRGroupGate"].to<JsonObject>();
    data_tone_THRGroupGate["@asset"]   = "noiseGate";
    data_tone_THRGroupGate["@enabled"] = thrs.unit[GATE];
    data_tone_THRGroupGate["Decay"]    = thrs.gate_setting[GA_DECAY] / 100.0;
    data_tone_THRGroupGate["Thresh"]   = map(thrs.gate_setting[GA_THRESHOLD], 0, 100, -96, 0); // [0 .. 100] to [-96dB .. 0dB]

    data_tone["global"]["THRPresetParamTempo"] = 110;

    JsonObject meta  = doc["meta"].to<JsonObject>();
    meta["original"] = 0;
    meta["pbn"]      = 0;
    meta["premium"]  = 0;

    doc["schema"]  = "L6Preset";
    doc["version"] = 5;

    *current_doc = doc;

    String output;
    doc.shrinkToFit(); // Optional
//    serializeJson(doc, output);
    serializeJsonPretty(doc, output);
    return output;
}

void deleteFile(String fileName)
{
  if (SD.exists(fileName.c_str()))
  {
    TRACE_THR30IIPEDAL(Serial.println("Removing file " + fileName);)
    SD.remove(fileName.c_str());
    TRACE_THR30IIPEDAL(Serial.println("Done");)
  }
}

void writeToFile( String fileName, String data )
{
  // TODO: Why delete first?
  deleteFile( fileName );

  //File myFile = SD.open(fileName.c_str(), O_READ | O_WRITE | O_CREAT | O_TRUNC);
  File myFile = SD.open(fileName.c_str(), FILE_WRITE);
  if( myFile )
  {
    TRACE_THR30IIPEDAL(Serial.println("Writing to: " + fileName);)
    myFile.println( data );
    myFile.close();
    TRACE_THR30IIPEDAL(Serial.println("Done");)
  }
  else
  {
    TRACE_THR30IIPEDAL(Serial.println("Error opening file: " + fileName);)
  }
}
