#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include <CRC32.h> // https://github.com/bakercp/CRC32

#include "Globals.h"
#include "trace.h"

void Constants::set_all(const byte* buf)
{
  uint32_t vals = *((uint32_t*) buf);     // How many values are in the symbol table
  uint32_t len =  *((uint32_t*) (buf+4)); // How many bytes build the symbol table

  TRACE_THR30IIPEDAL( Serial.printf("%d symbols found.\n\r", vals);
                      Serial.printf("%d file length.\n\r", len);
                    )
  int symStart = (int)(12 * vals + 8); // Where is the start of the symbol names?    
  
  TRACE_THR30IIPEDAL(Serial.printf("Start of symbols: %d\n\r", symStart);)
  
  // CRC32 calculation over the symbol table, used to calculate the 'Magic Key' ==========================================
  // uint32_t checksum = CRC32::calculate(buf, len);
  // Serial.println("Symbol table CRC32 = 0x" + String(checksum, HEX) + " *********************************************");
  // =====================================================================================================================

  // print out for analysis only
  // char tmp[60];

  // for(uint16_t i =0; i < len; i++ )
  // {
  //     if(i %16==0)
  //     {
  //         sprintf(tmp,"\r\n%04X : ",i);
  //     }
  //     Serial.print(tmp);
  //     sprintf(tmp,"%02X ",buf[i]);
  //     Serial.print(tmp);
  // }

  TRACE_THR30IIPEDAL(Serial.println();)
  glo.clear();

  char *beg= (char*) (buf+symStart);
  char *p=beg;

  for(uint16_t keynr=0; keynr< vals; keynr++ )
  {
      glo.emplace(String(p), keynr);
      TRACE_V_THR30IIPEDAL(Serial.printf("%s : %d\n\r",p, keynr);)
      while( *(p++)!='\0');
  }
};

// Get count of free memory (for development only)
#ifdef __arm__
// Should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else // __ARM__
extern char *__brkval;
#endif // __arm__
 
int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif // __arm__
}