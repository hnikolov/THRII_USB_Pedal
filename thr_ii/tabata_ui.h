#ifndef TABATA_UI_H
#define TABATA_UI_H

#include "tabata_metronome.h"
#include "stompBoxes.h" // Fonts definition

extern uint32_t maskCUpdate;

///////////////////////////////////////////////////////////////////////////////
// NOTE: Smooth arcs need 16-bit color
///////////////////////////////////////////////////////////////////////////////
class TabataUI
{
private:
  Tabata_Metronome &tm;
  uint16_t x, y;   // Coordinates of the top-left corner
  uint16_t fg, bg; // Main colors

  uint8_t radius       = 90; // Outer arc radius
  uint8_t thickness    = 10; // Thickness
  uint8_t inner_radius = radius - thickness; // Calculate inner radius (can be 0 for circle segment)

  uint16_t start_angle = 0; // Start angle must be in range 0 to 360
  uint16_t max_angle = 360 - start_angle; // To have a symetry
  uint16_t end_angle = 0;
  uint16_t range = 100;

  bool arc_end = 1; // true = round ends, false = square ends (arc_end parameter can be omitted, ends will then be square)

  // Local shadow variables used to track state
  bool isMetronome = tm.isMetronome;
  uint16_t practice_time = 0;
  uint16_t rest_time = 0;
  uint16_t practice_cnt;
  uint16_t rest_cnt;
  uint16_t ready_cnt;
  uint16_t bpm = 0;
  uint8_t time_signature = 0;
  bool isRunning = false;
  bool update_metronome, update_tabata;

  void draw_text_line_1(String txt)
  {
    spr.setTextColor(TFT_THRGREY, bg, true);
    spr.setTextDatum(MC_DATUM); // Middle centre datum
    spr.setTextPadding(radius);
    spr.loadFont(AA_FONT_XLARGE);
    spr.drawString(txt, radius, radius-20);
    //spr.setFreeFont(FSB24);      // Select Free Serif font
    //spr.drawString(txt, radius, radius-20, GFXFF);
    spr.setTextPadding(0);
    spr.unloadFont();
  }

  void draw_text_line_2(String txt)
  {
    spr.setTextColor(TFT_THRGREY, bg, true);
    spr.setTextDatum(MC_DATUM); // Middle centre datum
    spr.setTextPadding(radius);
    spr.drawString(txt, radius+2, radius+30, 4);
    //spr.setFreeFont(FSS18);      // Select Free Serif font
    //spr.drawString(txt, radius, radius+30, GFXFF);
    spr.setTextPadding(0);
    spr.unloadFont();
  }

public:
  TabataUI(Tabata_Metronome &_tm, uint16_t xCoord, uint16_t yCoord, uint16_t fgColor, uint16_t bgColor):
    tm(_tm),
    x(xCoord), y(yCoord), 
    fg(fgColor), bg(bgColor) {}

  void setRange(uint16_t val)
  {
    range  = val;
  }

  void draw_metronome()
  {
    if( maskCUpdate )
    {
       tft.fillScreen(TFT_BLACK);
      maskCUpdate = 0;
    }
    spr.createSprite(radius*2+4, radius*2+4);
    draw_progress_arc(0); // Filled if in rest/ready state
    draw_text_line_1(String(tm.metronome.getBPM()));
    draw_text_line_2(String(tm.metronome.getTimeSignature())+"/4"); // TODO" /8
    spr.pushSprite(x, y);
    spr.deleteSprite();
  }

  void draw_tabata()
  {
    spr.createSprite(radius*2+4, radius*2+4);
    draw_progress_arc(0); // Filled if in rest/ready state
    draw_text_line_1(String(tm.tabata.practice_time) + "/" + String(tm.tabata.rest_time));
    draw_text_line_2("Ready");
    spr.pushSprite(x, y);
    spr.deleteSprite(); 
  }

  void update()
  {
    bool push_sprite = false;
    spr.createSprite(radius*2+4, radius*2+4);
    // ---------------------------------------------------------------
    // Metronome
    // ---------------------------------------------------------------
    if( bpm != tm.metronome.getBPM() ) // Change of BPM
    {
      bpm = tm.metronome.getBPM();
      update_metronome = true;
    }

    if( time_signature != tm.metronome.getTimeSignature() ) // Change of time signature
    {
      time_signature = tm.metronome.getTimeSignature();
      update_metronome = true;
    }

    if( isMetronome != tm.isMetronome ) // Switch to metronome mode
    {
      isMetronome = tm.isMetronome;
      update_metronome = tm.isMetronome;
    }

    if( update_metronome == true )
    {
      draw_progress_arc(0); // Filled if in rest/ready state
      draw_text_line_1(String(tm.metronome.getBPM()));
      draw_text_line_2(String(tm.metronome.getTimeSignature())+"/4"); // TODO: /8
      update_metronome = false;
      push_sprite = true;
    }
    // ---------------------------------------------------------------
    // Tabata
    // ---------------------------------------------------------------
    if( isMetronome == false )
    {
      if( isRunning != tm.isRunning() )
      {
        isRunning = tm.isRunning();
        update_tabata = true;
      }

      if( practice_time != tm.tabata.practice_time )
      {
        practice_time = tm.tabata.practice_time;
        update_tabata = true;
      }

      if( rest_time != tm.tabata.rest_time )
      {
        rest_time = tm.tabata.rest_time;
        update_tabata = true;
      }

      if( update_tabata == true )
      {
        if( tm.tabata._tbstate == tm.tabata.Tb_ready )
        {
          draw_progress_arc(0); // Filled if in rest/ready state
          draw_text_line_1(String(practice_time) + "/" + String(rest_time));
          draw_text_line_2("Ready");
          update_tabata = false;
          push_sprite = true;
        }
      }

      if( tm.tabata._tbstate == tm.tabata.Tb_practice )  { setRange(practice_time); }
      else if( tm.tabata._tbstate == tm.tabata.Tb_rest ) { setRange(rest_time);     }
      else                                               { setRange(tm.tabata.ready_time); }

      // Tabata Practice -----------------------------------------------
      if( tm.tabata.getState() == tm.tabata.Tb_practice )
      {
        if( practice_cnt != tm.tabata.practice_cnt )
        {
          practice_cnt = tm.tabata.practice_cnt;
          draw_progress_arc(practice_cnt);
          if( tm.with_metronome == true )
          {
            draw_text_line_1(String(tm.metronome.getBPM()));
            draw_text_line_2(String(tm.metronome.getTimeSignature()) + "/4");
          }
          else
          {
            draw_text_line_1(String(practice_cnt));
            //draw_text_line_1(String(range - practice_cnt + 1)); // Countdown
            draw_text_line_2("Practice");
          }
          push_sprite = true;
        }
      }
      // Tabata Rest ----------------------------------------------------
      else if( tm.tabata.getState() == tm.tabata.Tb_rest )
      {
        if( rest_cnt != tm.tabata.rest_cnt )
        {
          rest_cnt = tm.tabata.rest_cnt;
          draw_progress_arc(rest_cnt);
          draw_text_line_1(String(rest_cnt));
          //draw_text_line_1(String(range - rest_cnt + 1)); // Countdown
          draw_text_line_2("Rest");
          push_sprite = true;
        }
      }
      // Tabata Ready ----------------------------------------------------
      else if( tm.tabata.getState() == tm.tabata.Tb_ready )
      {
        if( ready_cnt != tm.tabata.ready_cnt )
        {
          ready_cnt = tm.tabata.ready_cnt;
          draw_progress_arc(ready_cnt); // TODO: isRunning?
          draw_text_line_1(String(practice_time) + "/" + String(rest_time));
          draw_text_line_2("Ready");
          push_sprite = true;
        }
      }
    }
    if( push_sprite ) { spr.pushSprite(x, y); }
    spr.deleteSprite();
  }

  void draw_progress_arc(uint16_t val)
  {
    end_angle = map(val, 0, range, 1, 360);

    spr.drawSmoothArc(radius, radius, radius, inner_radius, 0, 360, TFT_THRDARKGREY, TFT_BLACK, arc_end);
    if( tm.tabata.getState() == tm.tabata.Tb_practice )
    {
      spr.drawSmoothArc(radius, radius, radius, inner_radius, start_angle, end_angle, TFT_LIGHTGREY, TFT_BLACK, arc_end); // To Fill
    }
    else
    {
      spr.drawSmoothArc(radius, radius, radius, inner_radius, end_angle, start_angle, TFT_LIGHTGREY, TFT_BLACK, arc_end); // To Empty
    }
  }
};

#endif