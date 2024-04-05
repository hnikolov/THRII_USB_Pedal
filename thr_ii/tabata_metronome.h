#ifndef _TABATA_METRONOME_H_
#define _TABATA_METRONOME_H_

#include "metronome.h"
#include "tabata.h"

class Tabata_Metronome: public Tabata
{
//private:
public:
  Metronome metronome;
  bool with_metronome = false;

public:
  Tabata_Metronome(uint16_t practice_time, uint16_t rest_time, uint8_t tick_pin):
    Tabata(practice_time, rest_time, tick_pin),
    metronome(tick_pin) {}

  void enableMetronome( bool en )
  {
    with_metronome = en;
    if( with_metronome ) { Serial.println("Tabata metronome: metronome enabled");  }
    else                 { Serial.println("Tabata metronome: metronome disabled"); }
  }

  void toggleMetronome()
  {
    if( with_metronome == true )
    {
      with_metronome = false;
      metronome.stop();
      Serial.println("Tabata metronome: metronome disabled");
    }
    else
    {
      with_metronome = true;
      metronome.start();
      Serial.println("Tabata metronome: metronome enabled");
    }
  }

  virtual void start()
  {
    if( with_metronome == true ) { _start();        }
    else                         { Tabata::start(); }
  }

  void _start()
  {
    _tbstate = Tb_practice;
    practice_cnt = 0;
    second = millis();
    running = true;
    metronome.start();
    Serial.println("Tabata metronome start");
  }

  virtual void stop()
  {
    if( running == true )
    {
      running = false;
      metronome.stop();
      Serial.println("Tabata metronome stopped");
    }
  }

  virtual void update()
  {
    if( with_metronome == true ) { _update();        }
    else                         { Tabata::update(); }
  }

  void _update()
  // Not using interrupts, so this function must be called very regularly in the Arduino loop() function
  {
    if( running == true )
    {
      uint32_t now = millis();
      switch( _tbstate )
      {
        case Tb_practice:
          metronome.update();
          if( now - second >= 1000 ) // 1 second 
          {
            practice_cnt++;
            second = now;
            Serial.println("Tabata metronome Practice: " + String(practice_cnt));

            if( practice_cnt >= practice_time )
            {
              _tbstate = Tb_rest;
              rest_cnt = rest_time; // seconds
              Serial.println("Tabata metronome: Rest");
              metronome.stop();
            }
          }
        break;
      
        case Tb_rest:
          if( now - second >= 1000 ) // 1 second 
          {
            Serial.println("Tabata metronome Rest: " + String(rest_cnt));
            rest_cnt--;
            second = now;
          
            if( rest_cnt == 0 )
            {
              _tbstate = Tb_practice;
              practice_cnt = 0;
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
