#ifndef _STOMPBOXES_H_
#define _STOMPBOXES_H_

#include <vector>
#include "THR30II_Pedal.h"

#include <TFT_eSPI.h>
extern TFT_eSPI tft;
extern TFT_eSprite spr;

// Locally supplied fonts
#include "fonts/Free_Fonts.h" // Needs to be enabled in my_custom_setup.h as well
#include "fonts/NotoSansBold15.h"
#include "fonts/NotoSansBold36.h"
#include "fonts/NotoSansMonoSCB20.h"
//#include "fonts/Latin_Hiragana_24.h"
//#include "fonts/Final_Frontier_28.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_XLARGE NotoSansBold36
//#define AA_FONT_LARGE Latin_Hiragana_24
#define AA_FONT_MONO  NotoSansMonoSCB20 // NotoSansMono-SemiCondensedBold 20pt

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
class Knob
{
private:
  String name;
  uint16_t x, y;   // Coordinates of the top-left corner
  uint16_t fg, bg; // Main colors
  uint8_t show_value; // 0 - do not show, 1 - show in knob, 2 - show below

  uint8_t radius       = 20; // Outer arc radius
  uint8_t thickness    =  1; // Thickness
  uint8_t inner_radius = radius - thickness; // Calculate inner radius (can be 0 for circle segment)

  uint16_t start_angle = 45; // Start angle must be in range 0 to 360
  uint16_t max_angle = 360 - start_angle; // To have a symetry

  bool arc_end = 0;  // true = round ends, false = square ends (arc_end parameter can be omitted, ends will then be square)

  bool focused = false;

public:
  Knob(const String &sName, uint16_t xCoord, uint16_t yCoord, uint16_t fgColor, uint16_t bgColor, uint8_t shValue):
    name(sName),
    x(xCoord), y(yCoord), 
    fg(fgColor), bg(bgColor),
    show_value(shValue) {}

  void setFocus( bool status )
  {
    // We do not use sprite here because the name can be different size. No need to calculate sprite size if we use the tft
    focused = status;
    if( focused == true ) { tft.setTextColor(TFT_BLACK, fg, true); }
    else                  { tft.setTextColor(fg, TFT_BLACK, true); }
    tft.drawCentreString(name, x+22, y-20, 2); // x + (knob width/2)
  }

  void draw(uint16_t val)
  {
    uint16_t end_angle = map(val, 0, 100, start_angle+1, max_angle);

    spr.createSprite(radius*2+4+1, radius*2+4+1);
    spr.fillSmoothCircle(radius+2, radius+2, radius-6, bg, bg);
    spr.drawSmoothArc(radius+2, radius+2, radius, inner_radius, start_angle, max_angle, TFT_THRDARKGREY, TFT_BLACK, arc_end);
    spr.drawSmoothArc(radius+2, radius+2, radius, inner_radius, start_angle, end_angle, TFT_LIGHTGREY, TFT_BLACK, arc_end);
    
    if( show_value == 1 )
    {
      spr.setTextColor(TFT_THRGREY, bg, true);
      spr.drawCentreString(String(val), radius+3, radius-6, 2);
    }
    
    spr.pushSprite(x, y);
    spr.deleteSprite();

    if( show_value == 2 )
    {
      spr.createSprite(radius*2, 16);
      spr.setTextColor(fg, TFT_BLACK, true);
      spr.drawCentreString(String(val), radius+2, 0, 2);
      spr.pushSprite(x, y+radius*2+3);
      spr.deleteSprite();  
    }
  }
};

static uint16_t cmap[] =
{
  TFT_BLACK,   TFT_WHITE,
  TFT_THRLIME, TFT_THRDIMLIME,  // Foreground and Background colors, to be set in each stompbox object
  TFT_THRGREY, TFT_THRDARKGREY, // Jacks
  TFT_THRRED,  TFT_THRDRED, TFT_THRLIGHTRED // On/Off led
};

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
class StompBox
{
public:
  String name;
  std::vector <String> par_names;
  uint16_t fg, bg; // Main colors
  bool enabled;

  uint16_t w, x, m, xk;
  uint16_t y = 55; // y position of stompbox
  uint16_t yk;     // y position of knobs

  std::vector <Knob> knobs;
  uint8_t f_idx = 0; // ndex of the currently selected knob

public:
  StompBox(const String &sName, std::vector <String> &vParamNames, uint16_t fgColor, uint16_t bgColor):
    name(sName),
    par_names(vParamNames),
    fg(fgColor), bg(bgColor) {}

  uint8_t n; // Number of knobs

  void init()
  {
    n = par_names.size();
    uint8_t nn = n;
    if( nn < 3 ) { nn = 3; } // Minimum stompbox width is for 3 knobs
    w = nn * 51 + 16; // StompBox width 5 knobs (270 / 5 = 54, knob diameter is 40)
    // Note: use tft.width() or tft.height() depending on orientation
    x = (tft.width()-w)/2; // x-coordinate in order to center the stompbox in the middle of tft
    m = x + w/2; // Middle x-point of tft
    xk = x + 10; // x position of 1st knob (sprite coordinates)
    yk = y + 53;

    init_knobs();
  }

  virtual void erase()
  {
    // NOTE: Issues with large sprites, therefore, reduce color depth
    spr.setColorDepth(1);
    spr.createSprite(w+26, 240-y); // Height is 240 pixels
    //spr.fillScreen(TFT_BLACK);   // No need, default color is black
    spr.pushSprite(x-13, y);
    spr.deleteSprite();
    spr.setColorDepth(16);
  }

  inline void createPalettedSprite(int16_t width, int16_t height)
  {
    // NOTE: Issues with large sprites, therefore, reduce color depth and use palette
    spr.setColorDepth(4);
    spr.createSprite(width, height);

    // Amp unit is not rendered correctly when:
    //   i) switch to edit mode (OK)
    //  ii) exit edit mode
    // iii) switch to edit mode again (not OK)
    // NOTE:
    //   It seems sprites are always created OK, the issue exists even if sprites not used
    //   Investigation is needed
    if( spr.created() == false ) // Try again
    {
      Serial.println("Create Sprite Error 1 ***********************"); // To be removed
      spr.deleteSprite();
      spr.createSprite(width, height);
    }

    if( spr.width() != width || spr.height() != height ) // Try again
    {
      Serial.println("Create Sprite Error 2 ***********************"); // To be removed
      spr.deleteSprite();
      spr.createSprite(width, height);
    }
    // ----------------------------------------------------------------
    spr.createPalette(cmap);
    spr.setPaletteColor(2, fg); // To set FG of a stompbox
    spr.setPaletteColor(3, bg); // To set FG of a stompbox
    //spr.fillSprite(0); // Black, no need: sprite is default init with color index 0
  }

  inline void deinitPalettedSprite(int32_t x, int32_t y)
  {
    spr.pushSprite(x, y);
    spr.unloadFont();
    spr.deleteSprite();
    spr.setColorDepth(16);
  }

  void init_knobs()
  {
    for(int i = 0; i < n; i++)
    {
      if( n == 2 ) { knobs.push_back(Knob(par_names[i], xk + i*(51+25)+14, yk, fg, bg, 2)); }
      else         { knobs.push_back(Knob(par_names[i], xk + i*51        , yk, fg, bg, 2)); }
    }
  }

  void draw_knob(uint8_t idx, uint16_t val)
  {
    if( idx > n ) { return; } // Should not happen
    knobs[idx].draw(val);
  }

  void draw_selected_knob(uint16_t val)
  {
    knobs[f_idx].draw(val);
  }

  void setFocusNext()
  {
    knobs[f_idx].setFocus(false);
    if( f_idx == n-1 ) { f_idx = 0; }
    else               { f_idx++;   }
    knobs[f_idx].setFocus(true);
  }

  void setFocusPrev()
  {
    knobs[f_idx].setFocus(false);
    if( f_idx == 0 ) { f_idx = n-1; }
    else             { f_idx--;     }
    knobs[f_idx].setFocus(true);
  }

  void setFocus(uint8_t idx, bool status )
  {
    if( idx > n ) { return; } // Should not happen
    knobs[idx].setFocus(status);
    f_idx = idx;
  }

  uint8_t getFocus() // Index of the selected knob
  {
    return f_idx;
  }

  virtual void setEnabled( bool status )
  {
    if( enabled != status )
    {
      enabled = status;
      draw_led();
    }   
  }
  
  virtual void draw()
  {
    createPalettedSprite(w, 240-y); // Height is 240 pixels
    // 0-black, 2-fg, 3-bg, 4-grey

    // Stompbox
    spr.fillSmoothRoundRect(0, 0, w, 135, 10, 2, 3);
    spr.fillRect(0, 125, w, 10, 3);
    spr.fillSmoothRoundRect(5, 5, w-10, 110, 10, 0, 0);
    spr.fillSmoothRoundRect(5, 5, w-10,  14, 10, 0, 0); // Better top round
    spr.fillRect(5, 105, w-10, 15, 0);
    spr.fillRect(0, 132, w, 3, 4);
    spr.fillRect(0, 135, w, 50, 2);
    spr.fillSmoothRoundRect(5, 175, w-10, 15, 10, 0, 0);

    // Name
    spr.setTextColor(0);        // cmap[0] = black
    spr.setFreeFont(FSB9);      // Select Free Serif font
    spr.setTextDatum(TC_DATUM); // Middle center
    spr.drawString(name, w/2, 145, GFXFF);

    deinitPalettedSprite(x, y);

    // Jacks
    spr.createSprite(13, 45);
    spr.fillRect(5, 0, 8, 45, TFT_THRGREY);
    spr.fillRect(0, 5, 5, 35, TFT_THRDARKGREY);
    spr.pushSprite(x-13, y+135);
    spr.fillScreen(0);
    spr.fillRect(0, 0, 8, 45, TFT_THRGREY);
    spr.fillRect(8, 5, 5, 35, TFT_THRDARKGREY);
    spr.pushSprite(x+w, y+135);
    spr.deleteSprite();

    // On/off LED
    draw_led();

    // The Knobs
    for(auto & knob: knobs)
    {
      knob.setFocus(false);
    }
    knobs[f_idx].setFocus(true);
  }

private:
  void draw_led() const
  {
    spr.createSprite(14, 14);
    if( enabled )
    {
      spr.drawSpot(7, 7, 6, TFT_THRRED, TFT_THRDRED);
      spr.drawSpot(10, 4, 1.5, TFT_THRLIGHTRED, TFT_THRDRED);
    }
    else
    {
      spr.drawSpot(7, 7, 6, TFT_THRDRED, TFT_THRDRED);
    }
    spr.pushSprite(m-7, y+13);
    spr.deleteSprite();     
  }
};

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
class AmpBox: public StompBox
{
public:
  AmpBox(const String &sName, std::vector <String> &vParamNames, uint16_t fgColor, uint16_t bgColor): 
    StompBox(sName, vParamNames, fgColor, bgColor) { y = 99; }

  virtual void erase()
  {
    // NOTE: Issues with large sprites, therefore, reduce color depth
    spr.setColorDepth(1);
    spr.createSprite(w, 240-y);  // Height is 240 pixels
    //spr.fillScreen(TFT_BLACK); // No need, default color is black
    spr.pushSprite(x, 35);       // Note: to erase the amp unit info (amp,col,cab)
    deinitPalettedSprite(x, y);
  }

  virtual void setEnabled( bool status ) {} // Not needed for Amp

  virtual void draw()
  {
    createPalettedSprite(w, 240-y); // Height is 240 pixels
    // 0-black, 2-fg, 3-bg

    // Amp box
    spr.fillSmoothRoundRect(0, 22, w, 107, 10, 2, 3);
    spr.fillSmoothRoundRect(5, 5+22, w-10, 89, 10, 0, 0);
    spr.fillSmoothRoundRect(5, 5+22, w-10, 14, 10, 0, 0);       // Better top round
    spr.fillSmoothRoundRect(5, 5+22+89-14, w-10, 14, 10, 0, 0); // Better bottom round

    // The Legs
    spr.fillSmoothRoundRect(  30, 130, 20, 10, 10, 3, 3);
    spr.fillSmoothRoundRect(w-50, 130, 20, 10, 10, 3, 3);
    deinitPalettedSprite(x, y);

    // The Knobs
    for(auto & knob: knobs)
    {
      knob.setFocus(false);
    }
    knobs[f_idx].setFocus(true);
  }
};
//////////////////////////////////////////////////////////////////////////////////////////

#endif // _STOMPBOXES_H_