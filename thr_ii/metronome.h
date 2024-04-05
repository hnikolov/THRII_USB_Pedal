#ifndef _METRONOME_H_
#define _METRONOME_H_

class Metronome
{
private:
  uint8_t cTICK_PIN;
  uint16_t bpm = 100;
  bool running = false;
  uint32_t interval = 0;
  uint32_t dotted_interval = 0;
  uint32_t next_click = 0;

  uint16_t accent_click = 2600; // Hz
  uint16_t normal_click = accent_click / 2; // Hz
  uint8_t time_signature = 4; // = 4/4; 3 = 3/4
  uint8_t max_ts = 9; // = 9/4
  uint16_t sig[4] = {accent_click, normal_click, normal_click, normal_click};
  uint8_t idx = 0;

  uint32_t tempotaptime0 = 0;
  uint32_t tempotaptime1 = 0;
  uint32_t tempotapint = 1000;
  uint32_t tempotaptimeout = 1500; // Check what whould be this value

public:
  Metronome(uint8_t tick_pin):
    cTICK_PIN(tick_pin) {}

  uint16_t bpm_to_interval()
  {
    uint16_t per_second = (60.0 / bpm) * 1000; // ms
    dotted_interval = per_second * 1.5;
    return per_second;
  }

  void setTimeSignature( uint8_t ts)
  {
    time_signature = ts;
    Serial.println("Metronome time signature set to: " + String(time_signature) + "/4");
  }

  void prevTimeSignature()
  {
    time_signature--;
    if( time_signature < 2 ) { time_signature = max_ts; }
    Serial.println("Metronome time signature changed to: " + String(time_signature) + "/4");
  }

  void nextTimeSignature()
  {
    time_signature++;
    if( time_signature > max_ts ) { time_signature = 2; } // 2/4
    Serial.println("Metronome time signature changed to: " + String(time_signature) + "/4");
  }

  void setBPM( uint16_t _bpm )
  {
    bpm = _bpm;
    interval = bpm_to_interval();
    Serial.println("Metronome bpm set to: " + String(bpm) + ", interval: " + String(interval));
  }

  uint16_t getBPM()
  {
    return bpm;
  }

  void incBPM( uint16_t val )
  {
    bpm += val;
    if( bpm > 300 ) { bpm = 300; }
    interval = bpm_to_interval();
    Serial.println("Metronome bpm decreased to: " + String(bpm) + ", interval: " + String(interval));
  }

  void decBPM( uint16_t val )
  {
    if( bpm < val ) { bpm  = 0;   }
    else            { bpm -= val; }
    interval = bpm_to_interval();
    Serial.println("Metronome bpm increased to: " + String(bpm) + ", interval: " + String(interval));
  }

  void tapBPM()
  {
    // Get button press interval (discarding spurious results from long intervals, i.e. only do stuff on multiple presses)
    tempotaptime0 = tempotaptime1; // Time of last press
    tempotaptime1 = millis();      // Time of most recent press
    if( tempotaptime1 - tempotaptime0 < tempotaptimeout ) // Ignore long intervals > tempotaptimeout
    {
      tempotapint = tempotaptime1 - tempotaptime0; // Time interval between presses
      bpm = 60000 / tempotapint;
      interval = bpm_to_interval();
      Serial.println("Metronome bpm set by tapping to: " + String(bpm));
    }
  }

  bool isRunning()
  {
    return running;
  }

  void start()
  {
    running = true;
    interval = bpm_to_interval();
    next_click = millis() + interval;
    Serial.println("Metronome start, " + String(bpm) + " bpm, interval: " + String(interval));
  }

  void stop()
  {
    if( running == true )
    {
      Serial.println("Metronome stopped");
      running = false;
      idx = 0;
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
      if( now >= next_click )
      {
        next_click = now + interval;
        Serial.println("Click");
        /*
        tone(TICK_PIN, sig[idx++], 5);
        if( idx == ksig ) { idx = 0; }
        //*/
        // if( idx == 0 ) { tone(cTICK_PIN, accent_click, 5); next_click = now + dotted_interval;} // For irregular time signatures, TODO: 3/4 == 7/8
        if( idx == 0 ) { tone(cTICK_PIN, accent_click, 5); }
        else           { tone(cTICK_PIN, normal_click, 5); }
        idx++;
        if( idx == time_signature ) { idx = 0; }
        //*/
      }
    }
  }
};

#endif // _METRONOME_H_
