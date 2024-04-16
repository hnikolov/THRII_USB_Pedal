#ifndef _TABATA_METRONOME_H_
#define _TABATA_METRONOME_H_

#include "metronome.h"
#include "tabata.h"

class Tabata_Metronome
{
public:
  Tabata tabata;
  Metronome metronome;

  bool isMetronome = true;
  bool with_metronome = false;

public:
  Tabata_Metronome(uint8_t tick_pin):
    tabata(45, 15, tick_pin),
    metronome(tick_pin) {}

  void selectMetronomeMode( bool sel )
  {
    stop();
    isMetronome = sel;
    if( isMetronome == true ) { Serial.println("Selected Metronome"); }
    else                      { Serial.println("Selected Tabata");    }
  }

  void toggleTabataMetronomeMode()
  {
    stop();
    isMetronome = !isMetronome;
    if( isMetronome == true ) { Serial.println("Switched to Metronome"); }
    else                      { Serial.println("Switched to Tabata");    }
  }

  void enableMetronomeInTabata( bool en )
  {
    with_metronome = en;
    if( with_metronome ) { Serial.println("Tabata metronome: metronome enabled");  }
    else                 { Serial.println("Tabata metronome: metronome disabled"); }
  }

  void toggleMetronomeInTabata()
  {
    // TODO: If metronome is enabled during the ready counting state, the effect is "pause"
    with_metronome = !with_metronome;
    if( with_metronome == true ) { metronome.start(); }
    else                         { metronome.stop();  }
    Serial.println("Toggled tabata with metronome: " + String(with_metronome));
  }

  bool isRunning()
  {
    if( isMetronome == true ) { return metronome.isRunning(); }
    else                      { return tabata.isRunning();    }
  }

  void start()
  {
    if( isMetronome == true )         { metronome.start(); }
    else if( with_metronome == true ) { _start();          }
    else                              { tabata.start();    }
  }

  inline void _start()
  {
    tabata._tbstate = tabata.Tb_practice; // Skip the Ready state
    tabata.practice_cnt = 1;
    tabata.second = millis();
    tabata.running = true;
    metronome.start();
    Serial.println("Tabata metronome start");
  }

  void stop()
  {
    if( isMetronome == true )         { metronome.stop(); }
    else if( with_metronome == true ) { _stop();          }
    else                              { tabata.stop();    }
  }

  inline void _stop()
  {
    metronome.stop();
    tabata.stop();
    Serial.println("Tabata metronome stopped");
  }

  void toggle()
  {
    if( isMetronome == true )         { metronome.toggle(); }
    else if( with_metronome == true ) { _toggle();          }
    else                              { tabata.toggle();    }
  }

  inline void _toggle()
  {
    if( isRunning() ) { _stop();  }
    else              { _start(); }
  }

  void update()
  {
    if( isMetronome == true )         { metronome.update(); }
    else if( with_metronome == true ) { _update();          }
    else                              { tabata.update();    }
  }

  inline void _update()
  // Not using interrupts, so this function must be called very regularly in the Arduino loop() function
  {
    if( tabata.running == true )
    {
      uint32_t now = millis();
      switch( tabata._tbstate )
      {
        case tabata.Tb_practice:
          metronome.update();
          if( now - tabata.second >= 1000 ) // 1 second
          {
            Serial.println("Tabata metronome Practice: " + String(tabata.practice_cnt));
            tabata.practice_cnt++;
            tabata.second = now;

            if( tabata.practice_cnt > tabata.practice_time )
            {
              tabata._tbstate = tabata.Tb_rest;
              tabata.rest_cnt = 1; // seconds
              Serial.println("Tabata metronome: Rest");
              metronome.stop();
            }
          }
        break;
      
        case tabata.Tb_rest:
          if( now - tabata.second >= 1000 ) // 1 second
          {
            Serial.println("Tabata metronome Rest: " + String(tabata.rest_cnt));
            tabata.rest_cnt++;
            tabata.second = now;
          
            if( tabata.rest_cnt > tabata.rest_time )
            {
              tabata._tbstate = tabata.Tb_practice;
              tabata.practice_cnt = 1;
              Serial.println("Tabata metronome: Practice");
              metronome.start();
            }
          }
        break;

        default:
        break;
      }
    }
  }
};

#endif // _TABATA_METRONOME_H_
