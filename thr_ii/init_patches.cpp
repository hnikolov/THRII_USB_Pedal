#include <ArduinoJson.h> 	// For patches stored in JSON (.thrl6p) format
#include <string>
#include <vector>			  	// In some cases we will use dynamic arrays - though heap usage is problematic
							  	        // Where ever possibly we use std::array instead to keep values on stack instead

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

#if USE_SDCARD
  #define path_presets_user    "/thrii/user_presets"
  #define path_presets_factory "/thrii/factory_presets"

	//std::vector<std::string> patchesII; // patches are read in dynamically, not as a static PROGMEM array
  std::vector <JsonDocument> json_patchesII_user;
  std::vector <JsonDocument> json_patchesII_factory;
#else
	                                    // patchesII is declared in "patches.h" in this case
#endif

uint16_t npatches, npatches_user, npatches_factory = 0; // Counts the patches stored on SD-card or in PROGMEN
std::vector <String> libraryPatchNames; // All the names of the patches stored on SD-card or in PROGMEN
std::vector <String> factoryPatchNames;

////////////////////////////////////////////////
// Normal TRACE/DEBUG
#define TRACE_THR30IIPEDAL(x) x	  // trace on
// #define TRACE_THR30IIPEDAL(x)	// trace off

// Verbose TRACE/DEBUG
#define TRACE_V_THR30IIPEDAL(x) x	 // trace on
// #define TRACE_V_THR30IIPEDAL(x) // trace off

/////////////////////////////////////////////////
/*
void init_patches_SDCard()
{  
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
										//patchesII.emplace_back(std::string(pat.c_str()));
										//TRACE_V_THR30IIPEDAL(Serial.println(patchesII.back().c_str()) ;)
										//npatches++;
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
		//npatches = patchesII.size();  //from file patches.h 
		TRACE_V_THR30IIPEDAL(Serial.printf("From PROGMEM: npatches: %d",npatches) ;)
	#endif
}
*/
/*
void init_patch_names()
{
//  DynamicJsonDocument djd (4096); // Buffer size for JSON decoding of thrl6p-files is at least 1967 (ArduinoJson Assistant) 2048 recommended
	JsonDocument djd;

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
}
*/
// =========================================================================
std::string readFile(File file)
{
  std::string str;

  while( file.available() > 0 ) 
  {
    str += (char)file.read();
  }

  return str;
}

void initializePresets(File dir, std::vector <JsonDocument> &json_patchesII, std::vector <String> &patchNames, uint16_t &npatches)
{
  JsonDocument doc;

  while( true )
  {
    File entry = dir.openNextFile();
    //while(file.isBusy()); // File32
    delay(10);
    if( !entry ) { break; }  // No more files
    
    if( entry.available() ) // Zero is returned for directories
    {
      Serial.println( entry.name() ); // Check what name is given by the library
      
      std::string data = readFile(entry);
//        Serial.println( data.c_str() );
      
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

void init_patches_from_sdcard()
{
  if (!SD.begin(sd_chipsel))
	{
    Serial.println(F("Card failed, or not present."));
  }
  else
	{
		Serial.println(F("Card initialized."));
    TRACE_THR30IIPEDAL( Serial.print(F("VolumeBegin->success. FAT-Type: "));)

    Serial.println("Initializing user presets from SDCard");
    File dir = SD.open(path_presets_user);
    initializePresets(dir, json_patchesII_user, libraryPatchNames, npatches_user);

    Serial.println("Initializing factory presets from SDCard");
    dir = SD.open(path_presets_factory);
    initializePresets(dir, json_patchesII_factory, factoryPatchNames, npatches_factory);
  }
}