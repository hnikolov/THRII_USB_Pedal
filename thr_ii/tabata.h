#ifndef _TABATA_H_
#define _TABATA_H_

class Tabata
{
//private:
public:
  uint8_t cTICK_PIN;
  uint16_t accent_click = 2600; // Hz
  uint16_t normal_click = accent_click / 2; // Hz

  uint16_t practice_time;
  uint16_t rest_time;
  uint16_t practice_cnt = 1;
  uint16_t rest_cnt = 1;
  uint16_t ready_cnt = 0;
  uint16_t ready_time = 3;
  uint32_t second = 0;
  bool running = false;
  enum TbStates {Tb_practice, Tb_rest, Tb_ready};
  TbStates _tbstate = Tb_ready;

public:
  Tabata(uint16_t practice_time=45, uint16_t rest_time=15, uint8_t tick_pin=7):
    cTICK_PIN(tick_pin),
    practice_time(practice_time), // seconds
    rest_time(rest_time) {} // seconds

  TbStates getState()
  {
    return _tbstate;
  }

  bool isPractice()
  {
    return _tbstate == Tb_practice;
  }

  bool isRest()
  {
    return _tbstate == Tb_rest;
  }

  void setPracticeTime( uint16_t seconds )
  {
    practice_time = seconds;
    Serial.println("Tabata practice_time time set to: " + String(practice_time));
  }

  void setRestTime( uint16_t seconds )
  {
    rest_time = seconds; // Note: Rest time cannot be zero
    Serial.println("Tabata rest time set to: " + String(rest_time));
  }

  void incPracticeTime( uint16_t seconds )
  {
    practice_time += seconds;
    Serial.println("Tabata practice_time time increased to: " + String(practice_time));
  }

  void incRestTime( uint16_t seconds )
  {
    rest_time += seconds;
    Serial.println("Tabata rest time increased to: " + String(rest_time));
  }

  void decPracticeTime( uint16_t seconds )
  {
    if( practice_time < seconds ) { practice_time = 0;        }
    else                          { practice_time -= seconds; }
    Serial.println("Tabata practice time decreased to: " + String(practice_time));
  }

  void decRestTime( uint16_t seconds )
  {
    if( rest_time <= seconds ) { rest_time = 1;        } // Note: Rest time cannot be zero
    else                       { rest_time -= seconds; }
    Serial.println("Tabata rest time decreased to: " + String(rest_time));
  }

  bool isRunning()
  {
    return running;
  }

  void start()
  {
    _tbstate = Tb_ready;
    ready_cnt = 0;
    second = millis();
    running = true;
    Serial.println("Tabata start");
  }

  void stop()
  {
    if( running == true )
    {
      Serial.println("Tabata stopped");
      _tbstate = Tb_ready;
      ready_cnt = 0;
      running = false;
    }
  }

  void toggle()
  {
    if( isRunning() ) { stop();  }
    else              { start(); }
  }

  void update()
  // Not using interrupts, so this function must be called very regularly in the Arduino loop() function
  {
    if( running == true )
    {
      uint32_t now = millis();
      switch( _tbstate )
      {
        case Tb_practice:
          if( now - second >= 1000 ) // 1 second 
          {
            Serial.println("Practice: " + String(practice_cnt));
            practice_cnt++;
            second = now;

            if( practice_cnt > practice_time )
            {
              _tbstate = Tb_rest;
              rest_cnt = 1; // seconds
              Serial.println("Tabata: Rest");
              tone(cTICK_PIN, normal_click, 5);
            }
            else if( practice_cnt > practice_time - 3 )
            {
              tone(cTICK_PIN, accent_click, 5);
            }
          }
        break;
      
        case Tb_rest:
          if( now - second >= 1000 ) // 1 second
          {
            Serial.println("Rest: " + String(rest_cnt));
            rest_cnt++;
            second = now;

            if( rest_cnt > rest_time )
            {
              _tbstate = Tb_practice;
              practice_cnt = 1;
              Serial.println("Tabata: Practice");
              tone(cTICK_PIN, accent_click, 5);
            }
            else if( rest_cnt > rest_time - 3 )
            {
              tone(cTICK_PIN, normal_click, 5);
            }
          }
        break;

        case Tb_ready:
          if( now - second >= 1000 ) // 1 second
          {
            Serial.println("Ready: " + String(ready_cnt));
            ready_cnt++;
            second = now;
          
            if( ready_cnt > ready_time )
            {
              _tbstate = Tb_practice;
              practice_cnt = 1;
              Serial.println("Tabata: Practice");
              tone(cTICK_PIN, accent_click, 5);
            }
            else if( ready_cnt > ready_time - 3 )
            {
              tone(cTICK_PIN, normal_click, 5);
            }
          }
        break;

        default:
        break;
      }
    }
  }
};

#endif // _TABATA_H_
