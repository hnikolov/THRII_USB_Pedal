#include <ArduinoJson.h> 	// For patches stored in JSON (.thrl6p) format
#include <string>
#include <vector>			  	// In some cases we will use dynamic arrays - though heap usage is problematic
							  	        // Where ever possibly we use std::array instead to keep values on stack instead

#include <SD.h>
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

uint16_t npatches_user = 0;    // Counts the user patches stored on SD-card
uint16_t npatches_factory = 0; // Counts the factory patches stored on SD-card
uint16_t *npatches = &npatches_user;

// Normal TRACE/DEBUG
#define TRACE_THR30IIPEDAL(x) x	  // trace on
// #define TRACE_THR30IIPEDAL(x)	// trace off

// Verbose TRACE/DEBUG
#define TRACE_V_THR30IIPEDAL(x) x	 // trace on
// #define TRACE_V_THR30IIPEDAL(x) // trace off

std::string readFile(File file)
{
  std::string str;

  while( file.available() > 0 ) 
  {
    str += (char)file.read();
  }

  return str;
}

//-----------------
// Not used anymore
//-----------------
void initializePresets(File dir, std::vector <JsonDocument> &json_patchesII, std::vector <String> &patchNames, uint16_t &npatches)
{
  JsonDocument doc;

  while( true )
  {
    File entry = dir.openNextFile();
    //while(file.isBusy()); // File32
    if( !entry ) { break; } // No more files
    
    if( entry.available() ) // Zero is returned for directories
    {
      // NOTE: Very basic approach here, no check that the file is .thrl6p
      // Can be checked: int8_t len = strlen(filename); if(strstr(strlwr(entry.name() + (len - 7)), ".thrl6p")) {}
      Serial.println( entry.name() );
      
      std::string data = readFile( entry );
//      Serial.println( data.c_str() );
      
      DeserializationError dse = deserializeJson(doc, data);
      if( dse )
      {
          Serial.print("deserializeJson() failed: ");
          Serial.println( dse.c_str() );
          continue;
      }
      
      json_patchesII.push_back( doc );
      patchNames.push_back((const char*) (doc["data"]["meta"]["name"]));            
      Serial.println((const char*) (doc["data"]["meta"]["name"]));   
    }
    entry.close();
  }
  // Serial.printf(F("\n\rThere are %d JSON patches found on SD Card\n\r"), json_patchesII.size());
  npatches = json_patchesII.size();
}

//------------------------------------------------------
// The order of reading the files in a directory on the SD card is not guaranteed to be always the same
// Therefore, first read and sort the files based on their names. Use file name format: "01 name etc.thrl6p"
//------------------------------------------------------
void initializeSortedPresets(const String &path, std::vector <JsonDocument> &json_patchesII, std::vector <String> &patchNames, uint16_t &npatches)
{
  JsonDocument doc;
//  std::vector <File> directory; // Uses more memory
  std::vector <String> file_paths;

  File dir = SD.open(path.c_str());

  while( true )
  {
    File entry = dir.openNextFile();
    if( !entry ) { break; } // No more files
    if( entry.available() ) // Zero is returned for directories
    {
      String fpath = path + "/" + String(entry.name());
      Serial.println( fpath );
//      directory.push_back(entry);
      file_paths.push_back(fpath);
    }
    entry.close(); // DO NOT CLOSE IF PUSH File objects!!!
  }

//  if( directory.size() > 0 )
  if( file_paths.size() > 0 )
  {
//    std::sort(directory.begin(), directory.end(), [](File a, File b) { return strcasecmp(a.name(), b.name()) < 0; });
    std::sort(file_paths.begin(), file_paths.end(), [](String a, String b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });

    for(auto fpath : file_paths)
//    for(auto entry : directory)
    {
      //Serial.println(fpath);
      File entry = SD.open(fpath.c_str(), FILE_READ);
      std::string data = readFile( entry );
//      Serial.println( data.c_str() );
      entry.close();

      DeserializationError dse = deserializeJson(doc, data);
      if( dse )
      {
          Serial.print("deserializeJson() failed: ");
          Serial.println( dse.c_str() );
          continue;
      }
      
      json_patchesII.push_back( doc );
      patchNames.push_back((const char*) (doc["data"]["meta"]["name"]));            
      Serial.println((const char*) (doc["data"]["meta"]["name"])); 
    }
    npatches = json_patchesII.size();
  }
}
//------------------------------------------------------

void init_patches_from_sdcard()
{
  if (!SD.begin(sd_chipsel))
	{
    Serial.println(F("Card failed, or not present."));
  }
  else
	{
		Serial.println(F("Card initialized."));
    TRACE_THR30IIPEDAL(Serial.print(F("VolumeBegin -> success. FAT-Type: "));)

    Serial.println("Initializing user presets from SDCard");
//    File dir = SD.open(path_presets_user);
//    dir.seek(0); // Always start from the first file in the directory?
//    initializePresets(dir, json_patchesII_user, libraryPatchNames, npatches_user);
    initializeSortedPresets(path_presets_user, json_patchesII_user, libraryPatchNames, npatches_user);

    Serial.println("Initializing factory presets from SDCard");
//    dir = SD.open(path_presets_factory);
//    dir.seek(0); // Always start from the first file in the directory?
//    initializePresets(dir, json_patchesII_factory, factoryPatchNames, npatches_factory);
    initializeSortedPresets(path_presets_factory, json_patchesII_factory, factoryPatchNames, npatches_factory);

    TRACE_THR30IIPEDAL(Serial.printf(F("\n\rLoaded %d JSON user patches and %d JSON factory patches.\n\r"), npatches_user, npatches_factory);)
  }
}
