/////////////////////////////////////////////////////////////////////////////////////////////////
// UI DRAWING FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "THR30II_Pedal.h"

/////////////////////////
#include <TFT_eSPI.h> // Include the graphics library (this includes the sprite functions)
extern TFT_eSPI tft;
extern TFT_eSprite spr;

// Include font definition file (belongs to Adafruit GFX Library )
//#include <Fonts/FreeSansBold18pt7b.h>
//#include <Fonts/FreeSansOblique24pt7b.h>
//#include <Fonts/FreeSansOblique18pt7b.h>
//#include <Fonts/FreeSans12pt7b.h>
//#include <Fonts/FreeMono9pt7b.h>
//#include <Fonts/FreeSans9pt7b.h>


#include "stompBoxes.h"
extern std::vector <StompBox*> sboxes;

/*
// Moved to stompboxes.h
// Locally supplied fonts
// #fonts/include "Free_Fonts.h"
#include "fonts/NotoSansBold15.h"
#include "fonts/NotoSansBold36.h"
#include "fonts/NotoSansMonoSCB20.h"
#include "fonts/Latin_Hiragana_24.h"
//#include "fonts/Final_Frontier_28.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_XLARGE NotoSansBold36
#define AA_FONT_LARGE Latin_Hiragana_24
#define AA_FONT_MONO  NotoSansMonoSCB20 // NotoSansMono-SemiCondensedBold 20pt
*/

extern std::vector <String> *active_patch_names;

extern uint16_t *npatches;       // Reference to npathces_user or npatches_factory
extern int16_t *presel_patch_id; // ID of actually pre-selected patch (absolute number)
extern int16_t *active_patch_id; // ID of actually selected patch     (absolute number)
extern int8_t nUserPreset;       // Used to cycle the THRII User presets
extern bool show_patch_num;      // Used to show match numbers only when switching banks
extern bool factory_presets_active;

extern bool midi_connected;

/////////////////////////
uint32_t maskCUpdate = 0;

uint32_t maskPatchID       = 1 <<  0;
uint32_t maskPatchIconBank = 1 <<  1;
uint32_t maskPatchSelMode  = 1 <<  2;
uint32_t maskPatchSet      = 1 <<  3;
uint32_t maskAmpSelMode    = 1 <<  4;
uint32_t maskConnIcon      = 1 <<  5; // Not used
uint32_t maskPatchName     = 1 <<  6;
uint32_t maskGainMaster    = 1 <<  7; // drawPPChart
uint32_t maskVolumeAudio   = 1 <<  8; // drawPPChart
uint32_t maskEQChart       = 1 <<  9;
uint32_t maskAmpUnit       = 1 << 10;
uint32_t maskCompressor    = 1 << 11; // drawUtilUnit
uint32_t maskNoiseGate     = 1 << 12; // drawUtilUnit
uint32_t maskFxUnit        = 1 << 13; // drawFxUnit
uint32_t maskEcho          = 1 << 14; // drawFxUnit
uint32_t maskReverb        = 1 << 15; // drawFxUnit

// Used in Edit mode
uint32_t maskCompressorEn  = 1 << 16; // drawUtilUnit (Edit mode)
uint32_t maskNoiseGateEn   = 1 << 17; // drawUtilUnit (Edit mode)
uint32_t maskFxUnitEn      = 1 << 18; // drawFxUnit   (Edit mode)
uint32_t maskEchoEn        = 1 << 19; // drawFxUnit   (Edit mode)
uint32_t maskReverbEn      = 1 << 20; // drawFxUnit   (Edit mode)
uint32_t maskClearTft      = 1 << 21; // Used when switching to Edit mode

uint32_t maskAll           = 0xffffffff;

ampSelectModes amp_select_mode = COL; // Currently used in updateStatusMask()

bool bgfill = true;

// Can be: THRII (5 presets), User, and Factory presets
void drawPatchSet(uint16_t colour, String ps_name)
{
  int x = 0, y = 0, w = 80, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
//  spr.setTextFont(2);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour, bgfill);
  spr.drawString(ps_name, w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}
/*
void drawPatchID(uint16_t fgcolour, int patchID, bool inverted = false)
{
  // TODO: y = 20, h = 60?
 	int x = 0, y = 0, w = 80, h = 80;
  uint16_t fg_colour = fgcolour;
  uint16_t bg_colour = TFT_THRVDARKGREY;
  if( inverted )
  {
    fg_colour = TFT_THRVDARKGREY;
    bg_colour = fgcolour;
  }
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bg_colour, TFT_BLACK);
	spr.setTextFont(7);
	spr.setTextSize(1);
	spr.setTextDatum(MR_DATUM);
	spr.setTextColor(TFT_BLACK, bgcolour, bgfill); // TODO in case of inverted
	spr.drawNumber(88, w-7, h/2);
  spr.setTextColor(fg_colour, bg_colour, bgfill);
	spr.drawNumber(patchID, w-7, h/2);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}
/*/
void drawPatchID(uint16_t fgcolour, int patchID)
{
// 	int x = 0, y = 0, w = 80, h = 80;
 	int x = 0, y = 20, w = 80, h = 60;
//	uint16_t bgcolour = TFT_THRVDARKGREY;
	spr.createSprite(w, h);
	//spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, TFT_THRBROWN, TFT_BLACK);
	spr.setTextFont(7);
	spr.setTextSize(1);
	spr.setTextDatum(MR_DATUM);
//	spr.setTextColor(TFT_BLACK, bgcolour, bgfill);
	spr.setTextColor(TFT_THRDARKBROWN, TFT_THRDARKBROWN, bgfill);
	spr.drawNumber(88, w-10, h/2);
	spr.setTextColor(fgcolour, fgcolour, bgfill);
	spr.drawNumber(patchID, w-10, h/2);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}
//*/

void drawPatchIcon(int x, int y, int w, int h, uint16_t colour, int patchID, bool show_num = true)
{
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
//  spr.setTextFont(2);  
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour, bgfill);
  if( show_num ) { spr.drawNumber(patchID, 10, 11); }
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawPatchIconBank(int presel_patch_id, int active_patch_id, int8_t active_user_setting, bool show_num = false)
{
	uint16_t iconcolour = 0;
	int banksize = 5;
	int first = ((presel_patch_id-1)/banksize)*banksize+1; // Find number for 1st icon in row
  int x = 110; // Note, coordinate of first patch icon becomes x + 20
	
	switch( _uistate ) 
  {
    // TODO: avoid code duplication
    case UI_manual:
      if( _uistate_prev == UI_home_amp )
      {
        for(int i = 1; i <= 5; i++) { drawPatchIcon(x + 20*i, 0, 20, 20, TFT_THRBROWN, i, false); }
        drawPatchIcon(x + 20*active_user_setting, 0, 20, 20, TFT_THRCREAM, active_user_setting + 1, false);
      }
		break;

		case UI_home_amp:
      for(int i = 1; i <= 5; i++) { drawPatchIcon(x + 20*i, 0, 20, 20, TFT_THRBROWN, i, false); }

      // Note: if user preset is selected and parameter is changed via THRII, then getActiveUserSetting() returns -1
      drawPatchIcon(x + 20*active_user_setting, 0, 20, 20, TFT_THRCREAM, active_user_setting + 1, false);
		break;
		
		case UI_home_patch:
		case UI_edit:
			for(int i = first; i < first+banksize; i++)
			{
				if( i == active_patch_id )
				{
					iconcolour = TFT_THRCREAM; // Highlight active patch icon
				}
				else if( i == presel_patch_id )
				{
					iconcolour = TFT_THRORANGE; // Highlight pre-selected patch icon
				}
				else
				{
					iconcolour = TFT_THRBROWN; // Colour for unselected patch icons
				}
				if( i > *npatches )
				{
					iconcolour = TFT_BLACK;
				}
				drawPatchIcon(x + 20*(i-first+1), 0, 20, 20, iconcolour, i, show_num);
			}
		break;

		default:
		break;
	}
}

// Used to indicate Manual mode
void drawPatchSelMode(uint16_t colour, String string="MAN")
{
  int x = 230, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  //spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour, bgfill);
  spr.drawString(string, w/2, h/2+1);
  //spr.drawString("EDIT", w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawAmpSelMode(uint16_t colour, String string)
{
  int x = 80, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  //spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour, bgfill);
  spr.drawString(string, w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawConnIcon(bool THRconnected)
{
  spr.createSprite(40, 20);
  if( THRconnected ) // Connected: green
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_DARKGREEN, TFT_BLACK);    // Bg
    spr.drawWideLine(1, 9, 14, 9, 3, TFT_THRCREAM, TFT_DARKGREEN);         // Left wire
    spr.drawWideLine(26, 9, 38, 9, 3, TFT_THRCREAM, TFT_DARKGREEN);        // Right wire
    spr.fillSmoothRoundRect(12, 2, 8, 15, 3, TFT_THRCREAM, TFT_DARKGREEN); // Plug
    spr.fillRect(16, 2, 4, 15, TFT_THRCREAM);                              // Plug
    spr.fillSmoothRoundRect(21, 2, 8, 15, 3, TFT_THRCREAM, TFT_DARKGREEN); // Socket
    spr.fillRect(21, 2, 4, 15, TFT_THRCREAM);                              // Socket
    spr.fillRect(16, 4, 5, 3, TFT_THRCREAM);                               // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_THRCREAM);                              // Lower prong
  }
  else // Disconnected: red 
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_RED, TFT_BLACK);    // Bg
    spr.drawWideLine(1, 9, 10, 9, 3, TFT_THRCREAM, TFT_RED);         // Left wire
    spr.drawWideLine(30, 9, 38, 9, 3, TFT_THRCREAM, TFT_RED);        // Right wire
    spr.fillSmoothRoundRect(8, 2, 8, 15, 3, TFT_THRCREAM, TFT_RED);  // Plug
    spr.fillRect(12, 2, 4, 15, TFT_THRCREAM);                        // Plug
    spr.fillSmoothRoundRect(24, 2, 8, 15, 3, TFT_THRCREAM, TFT_RED); // Socket
    spr.fillRect(24, 2, 4, 15, TFT_THRCREAM);                        // Socket
    spr.fillRect(16, 4, 5, 3, TFT_THRCREAM);                         // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_THRCREAM);                        // Lower prong
  }
  spr.pushSprite(280, 0);
  spr.deleteSprite();
}

// Split patch name into 2 lines if too long. Split at space, '-', or '_' char
uint8_t getSplitPos( String patchname )
{
    uint8_t pos = 14; // Use < 14 to have shorter first line
    while( pos > 0 )
    {
      char tmp = patchname.charAt( pos );
      if( tmp == ' ' || tmp == '-' || tmp == '_' )
      {
        return pos;
      }
      pos--;
    }
    
    return 14; // Should not happen
}

void drawPatchName(uint16_t fgcolour, String patchname, bool inverted = false)
{
  int x = 00, y = 80, w = 320, h = 95;
  uint16_t fg_colour = fgcolour;
  uint16_t bg_colour = TFT_THRVDARKGREY;
  if( inverted )
  {
    fg_colour = TFT_THRVDARKGREY;
    bg_colour = fgcolour;
  }
  spr.createSprite(w, h); // 16-bit colour needed to show antialiased fonts, do not use palette
  spr.fillSmoothRoundRect(0, 0, w, h-1, 3, bg_colour, TFT_BLACK);
  spr.loadFont(AA_FONT_XLARGE);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fg_colour, bg_colour, bgfill);
  
  if( spr.textWidth(patchname) > w )
  {
    uint8_t pos = getSplitPos( patchname );
    String line1 = patchname.substring(0, pos); // TODO: Split at space char
    String line2 = patchname.substring(pos, patchname.length());
    spr.drawString(line1, w/2, h/2-18);
    spr.drawString(line2, w/2, h/2+24);
  }
  else
  {
    spr.drawString(patchname, w/2, h/2);
  }

  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawBarChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int barh)
{
  int tpad = 20; int pad = 0;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  //spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour, bgfill);
  spr.drawString(label, w/2-1, 11);
  spr.fillRect(pad, h-(h-tpad-pad)*barh/100, w-1-2*pad, (h-tpad-pad)*barh/100, fgcolour);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void draw2BarChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int b1, int b2)
{
  int tpad = 20; int pad = 0;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  //spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour, bgfill);
  spr.drawString(label, w/2, 11);
  spr.fillRect(pad, h-(h-tpad-pad)*b1/100, w/2-1-2*pad, (h-tpad-pad)*b1/100, fgcolour);
  spr.fillRect(w/2+pad, h-(h-tpad-pad)*b2/100, w/2-1-2*pad, (h-tpad-pad)*b2/100, fgcolour);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawPPChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int ped1, int ped2)
{
	int tpad = 20; int pad = 0;
	int ped1height = ped1*(h-tpad-pad)/100; // For guitar and audio volume
	int ped2height = ped2*(h-tpad-pad)/100; // 
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	//spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour, bgfill);
	spr.drawString(label, w/2-1, 12);
	spr.fillRect(pad, h-ped1height, w/2-1-2*pad, ped1height, fgcolour);
	spr.fillRect(w/2+pad, h-ped2height, w/2-1-2*pad, ped2height, fgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawEQChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int b, int m, int t)
{
  int tpad = 20; int pad = 0;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  //spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour, bgfill);
  spr.drawString(label, w/2, 12);
  spr.fillRect(pad, h-(h-tpad-pad)*b/100, w/3-1-2*pad, (h-tpad-pad)*b/100, fgcolour);
  spr.fillRect(w/3+pad, h-(h-tpad-pad)*m/100, w/3-1-2*pad, (h-tpad-pad)*m/100, fgcolour);
  spr.fillRect(2*w/3+pad, h-(h-tpad-pad)*t/100, w/3-1-2*pad, (h-tpad-pad)*t/100, fgcolour);
  spr.pushSprite(x, y);
  spr.unloadFont();
  spr.deleteSprite();
}

void drawAmpUnit(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, THR30II_COL col, THR30II_AMP amp, THR30II_CAB cab)
{
	int tpad = 2; int pad = 2;
	uint16_t ampcolour1 [3] = {TFT_THRRED, TFT_THRBLUE, TFT_THRGREEN};
	uint16_t ampcolour2 [3] = {TFT_THRLIGHTRED, TFT_THRLIGHTBLUE, TFT_THRLIGHTGREEN};
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);	// Unit body
	spr.fillSmoothRoundRect(pad, tpad, w-1-2*pad, h-1-tpad-pad, 1, fgcolour, bgcolour);	// Draw area
	spr.drawSpot(16, 16, 7.5, ampcolour2[col], fgcolour);	// Col status light
	spr.drawSpot(16, 16, 6, ampcolour1[col], ampcolour2[col]);
	spr.drawSpot(19, 13, 1.5, ampcolour2[col], ampcolour1[col]);	// Col status light
	spr.loadFont(AA_FONT_MONO);
	//spr.setTextFont(4);
  spr.setTextDatum(ML_DATUM);
	spr.setTextColor(bgcolour, fgcolour, bgfill);
	spr.drawString(THR30II_AMP_NAMES[amp], 28, 18);	// Unit label
	spr.drawString(THR30II_CAB_NAMES[cab], 12, 43);	// Unit label
	
	int cabcentrex = 210;
	int cabcentrey = 30;
	int cabw = 50;
	int cabh = 50;

	switch( cab )	// Draw cabinet icon depending on type
	{
		case 0 ... 6:
		case 14:
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-cabh/4,   9, bgcolour, fgcolour); // Top left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-cabh/4,   7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-cabh/4,   9, bgcolour, fgcolour); // Top right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-cabh/4,   7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey+cabh/4-2, 9, bgcolour, fgcolour); // Bottom left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey+cabh/4-2, 7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey+cabh/4-2, 9, bgcolour, fgcolour); // Bottom right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey+cabh/4-2, 7, fgcolour, bgcolour);
		break;

		case 7 ... 11:
			cabh = 30;
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-1, 9, bgcolour, fgcolour);	// Top left
			spr.drawSpot(cabcentrex-cabw/4,   cabcentrey-1, 7, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-1, 9, bgcolour, fgcolour);	// Top right
			spr.drawSpot(cabcentrex+cabw/4-2, cabcentrey-1, 7, fgcolour, bgcolour);
		break;

		case 12 ... 13:
		case 15:
			cabw = 40;
			cabh = 40;
			spr.fillSmoothRoundRect(cabcentrex-cabw/2, cabcentrey-cabh/2, cabw-1, cabh-1, 5, bgcolour, fgcolour);
			spr.fillSmoothRoundRect(cabcentrex-cabw/2+2, cabcentrey-cabh/2+2, cabw-5, cabh-5, 3, fgcolour, bgcolour);
			spr.drawSpot(cabcentrex-1, cabcentrey-1, 15, bgcolour, fgcolour);	// Top left
			spr.drawSpot(cabcentrex-1, cabcentrey-1, 13, fgcolour, bgcolour);
		break;

		case 16:
		break;
	}

	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawUtilUnit(int x, int y, int w, int h, int bpad, uint16_t bgcolour, uint16_t fgcolour, String label, double utilparams[2])
{
	int tpad = 26; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = 0; int barh = grh/2;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-bpad, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	//spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
//	spr.setTextColor(fgcolour, bgcolour, bgfill);
	spr.setTextColor(TFT_BLACK, bgcolour, bgfill);
	spr.drawString(label, w/2, 13);
	spr.fillRect(grx, gry, grw, grh, fgcolour);	// Draw graph area
	barw = grw * utilparams[0]/100;
	spr.fillRect(grx, gry, barw, barh+1, bgcolour);
	barw = grw * utilparams[1]/100;
	spr.fillRect(grx, gry+1+barh, barw, barh+1, bgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawFXUnit(int x, int y, int w, int h, uint16_t fgcolour, uint16_t bgcolour, String label, int nbars, double FXparams[5])
{
	int tpad = 24; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = grw / nbars;	// Calculate bar width
	int barh = 0;

	spr.createSprite(w, h);

	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, fgcolour, TFT_BLACK); // Draw FX unit
  spr.fillRect(grx, gry, grw, grh, bgcolour); // Draw graph area

	spr.loadFont(AA_FONT_SMALL);
	//spr.setTextFont(2);
  spr.setTextDatum(MC_DATUM);
//	spr.setTextColor(bgcolour, fgcolour, bgfill);
	spr.setTextColor(TFT_BLACK, fgcolour, bgfill);
	spr.drawString(label, w/2, 13);	// Write label

	if( nbars == 4 ) { barw = grw / nbars + 1; }

	for(int i = 0; i < nbars; i++)
	{
		barh = (grh-1) * FXparams[i] / 100;	// Calculate bar height
    // Widen last bar by 1px
		if( i == nbars - 1 ) { spr.fillRect(grx + i * barw, gry + grh - barh, barw+1, barh, fgcolour); }
    else                 { spr.fillRect(grx + i * barw, gry + grh - barh, barw,   barh, fgcolour); }
	}

	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void THR30II_Settings::updateConnectedBanner() // Show the connected Model 
{
	// //Display "THRxxII" in blue (as long as connection is established)

	// s1=THRxxII_MODEL_NAME();
	// if(ConnectedModel != 0)
	// {
	// 	tft.loadFont(AA_FONT_LARGE); // Set current font    
	// 	tft.setTextSize(1);
	// 	tft.setTextColor(TFT_BLUE, TFT_BLACK, bgfill);
	// 	tft.setTextDatum(TL_DATUM);
	// 	tft.setTextPadding(160);
	// 	tft.drawString(s1, 0, 0);//printAt(tft,0,0, s1 ); // Print e.g. "THR30II" in blue in header position
	// 	tft.setTextPadding(0);
	// 	tft.unloadFont();
	// }
}

/////////////////////////////////////////////////////////////////
// Redraw all the bars and values on the TFT
// x-position (0) where to place top left corner of status mask
// y-position     where to place top left corner of status mask
/////////////////////////////////////////////////////////////////
void updateStatusMask(THR30II_Settings &thrs, uint32_t &maskCUpdate)
{
  if( maskCUpdate ) { drawConnIcon( midi_connected ); } // Just for completeness

	// Patch number
  if( maskCUpdate & maskPatchID )
  {
    if((_uistate == UI_home_amp) || (_uistate == UI_manual  && _uistate_prev == UI_home_amp))
    {
      drawPatchID(TFT_THRCREAM, nUserPreset + 1);
    }
    else
    {
      drawPatchID(TFT_THRCREAM, *active_patch_id);
    }
  }

	// Patch icon bank
  if( maskCUpdate & maskPatchIconBank )
  {
	  drawPatchIconBank(*presel_patch_id, *active_patch_id, nUserPreset + 1, show_patch_num);
  }

	// Use Patch select mode to indicate manual mode
  if( maskCUpdate & maskPatchSelMode )
  {
    if( _uistate == UI_manual ) { drawPatchSelMode(TFT_THRCREAM); }
    else                        { drawPatchSelMode(TFT_THRBROWN); }
  }

  // Show which set of patches is used
  if( maskCUpdate & maskPatchSet )
  {
    if( _uistate == UI_home_amp )          { drawPatchSet(TFT_THRCREAM, "THRII");   }
    else if( _uistate == UI_home_patch ) 
    {
      if( factory_presets_active == true ) { drawPatchSet(TFT_THRCREAM, "FACTORY"); }
      else                                 { drawPatchSet(TFT_THRCREAM, "USER");    }
    }
  }

	// Amp select mode (COL/AMP/CAB); Highlighted only in Manual mode
  if( maskCUpdate & maskAmpSelMode )
  {
    uint16_t colour = TFT_THRBROWN;
    if( _uistate == UI_manual ) { colour = TFT_THRCREAM; }

    switch( amp_select_mode )
    {
      case COL: drawAmpSelMode(colour, "COL"); break;
      case AMP:	drawAmpSelMode(colour, "AMP"); break;
      case CAB: drawAmpSelMode(colour, "CAB"); break;
    }
  }

	// Amp/Cabinet ---------------------------------------------------------------
  if( maskCUpdate & maskAmpUnit )
  {
    switch( thrs.col )
    {
      case BOUTIQUE: tft.setTextColor(TFT_BLUE,  TFT_BLACK, bgfill);	break;
      case MODERN:   tft.setTextColor(TFT_GREEN, TFT_BLACK, bgfill); 	break;
      case CLASSIC:  tft.setTextColor(TFT_RED,   TFT_BLACK, bgfill); 	break;
    }
    drawAmpUnit(80, 20, 240, 60, TFT_THRCREAM, TFT_THRBROWN, "Amp", thrs.col, thrs.amp, thrs.cab);
  }

	String FXtitle;
	uint16_t FXbgcolour  =  0;
	uint16_t FXfgcolour  =  0;
	double FXparams[5]   = {0};
	double utilparams[2] = {0};
	//uint8_t FXw  =  60;
	uint8_t FXw  = 56;
	uint8_t FXw2 = 48;
	uint8_t FXh  = 65;
  uint16_t FXx = 0;
	uint8_t FXy  = 240 - FXh;
	uint8_t nFXbars = 5;

	// FX1 Compressor -------------------------------------------------------------
  if( maskCUpdate & maskCompressor )
  {
    FXparams[0] = thrs.compressor_setting[CO_SUSTAIN];
    FXparams[1] = thrs.compressor_setting[CO_LEVEL];
    nFXbars = 2;
    if(thrs.unit[COMPRESSOR]) { drawFXUnit(FXx, FXy, FXw2, FXh, TFT_THRCOMP, TFT_THRDIMCOMP,   "Cmp", nFXbars, FXparams); }
    else                      { drawFXUnit(FXx, FXy, FXw2, FXh, TFT_THRGREY, TFT_THRVDARKGREY, "Cmp", nFXbars, FXparams); }
  }
  FXx += FXw2;

  // Amp unit -------------------------------------------------------------------
  if( maskCUpdate & (maskGainMaster | maskEQChart) )
  {
    FXparams[0] = thrs.control[CTRL_GAIN];
    FXparams[1] = thrs.control[CTRL_MASTER];
    FXparams[2] = thrs.control[CTRL_BASS];
    FXparams[3] = thrs.control[CTRL_MID];
    FXparams[4] = thrs.control[CTRL_TREBLE];
    nFXbars = 5;
    drawFXUnit(FXx, FXy, FXw, FXh, TFT_THRAMP, TFT_THRDIMAMP, "Amp", nFXbars, FXparams);
  }
  FXx += FXw;

 	// Gate -----------------------------------------------------------------------
  if( maskCUpdate & maskNoiseGate )
  {
    FXparams[0] = thrs.gate_setting[GA_THRESHOLD];
    FXparams[1] = thrs.gate_setting[GA_DECAY];
    nFXbars = 2;
    if(thrs.unit[GATE]) {	drawFXUnit(FXx, FXy, FXw2, FXh, TFT_THRGATE,    TFT_THRDIMGATE,   "Gate", nFXbars, FXparams); }
    else                { drawFXUnit(FXx, FXy, FXw2, FXh, TFT_THRDIMGATE, TFT_THRVDARKGREY, "Gate", nFXbars, FXparams); }
  }
  FXx += FXw2;
  
	// FX2 Effect (Chorus/Flanger/Phaser/Tremolo) ----------------------------------
  if( maskCUpdate & maskFxUnit )
  {
    switch( thrs.effecttype )
    {
      case CHORUS:
        FXtitle = "Chor";	 // Set FX unit title
        if( thrs.unit[EFFECT] ) // FX2 activated
        {
          FXbgcolour = TFT_THRCHOR;
          FXfgcolour = TFT_THRDIMCHOR;
        }
        else // FX2 deactivated
        {
          FXbgcolour = TFT_THRDIMCHOR;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.effect_setting[CHORUS][CH_SPEED];
        FXparams[1] = thrs.effect_setting[CHORUS][CH_DEPTH];
        FXparams[2] = thrs.effect_setting[CHORUS][CH_PREDELAY];
        FXparams[3] = thrs.effect_setting[CHORUS][CH_FEEDBACK];
        FXparams[4] = thrs.effect_setting[CHORUS][CH_MIX];
        nFXbars = 5;  
      break;

      case FLANGER: 
        FXtitle = "Flan";	 // Set FX unit title
        if( thrs.unit[EFFECT] ) // FX2 activated
        {
          FXbgcolour = TFT_THRFLAN;
          FXfgcolour = TFT_THRDIMFLAN;
        }
        else // FX2 deactivated
        {
          FXbgcolour = TFT_THRDIMFLAN;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.effect_setting[FLANGER][FL_SPEED];
        FXparams[1] = thrs.effect_setting[FLANGER][FL_DEPTH];
        FXparams[2] = thrs.effect_setting[FLANGER][FL_MIX];
        FXparams[3] = 0;
        FXparams[4] = 0;
        nFXbars = 3;
      break;

      case PHASER:
        FXtitle = "Phas";	 // Set FX unit title
        if( thrs.unit[EFFECT] ) // FX2 activated
        {
          FXbgcolour = TFT_THRPHAS;
          FXfgcolour = TFT_THRDIMPHAS;
        }
        else // FX2 deactivated
        {
          FXbgcolour = TFT_THRDIMPHAS;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.effect_setting[PHASER][PH_SPEED];
        FXparams[1] = thrs.effect_setting[PHASER][PH_FEEDBACK];
        FXparams[2] = thrs.effect_setting[PHASER][PH_MIX];
        FXparams[3] = 0;
        FXparams[4] = 0;
        nFXbars = 3;		  
      break;		

      case TREMOLO:
        FXtitle = "Trem";	 // Set FX unit title
        if( thrs.unit[EFFECT] ) // FX2 activated
        {
          FXbgcolour = TFT_THRTREM;
          FXfgcolour = TFT_THRDIMTREM;
        }
        else // FX2 deactivated
        {
          FXbgcolour = TFT_THRDIMTREM;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.effect_setting[TREMOLO][TR_SPEED];
        FXparams[1] = thrs.effect_setting[TREMOLO][TR_DEPTH];
        FXparams[2] = thrs.effect_setting[TREMOLO][TR_MIX];
        FXparams[3] = 0;
        FXparams[4] = 0;
        nFXbars = 3;
      break;
    } // of switch(effecttype)

	  drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams);
  }
  FXx += FXw;

  // FX3 Echo (Tape Echo/Digital Delay)
  if( maskCUpdate & maskEcho )
  {
    switch( thrs.echotype )
    {
      case TAPE_ECHO:
        FXtitle = "Tape";	// Set FX unit title
        if( thrs.unit[ECHO] )  // FX3 activated
        {
          FXbgcolour = TFT_THRTAPE;
          FXfgcolour = TFT_THRDIMTAPE;
        }
        else // FX2 deactivated
        {
          FXbgcolour = TFT_THRDIMTAPE;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.echo_setting[TAPE_ECHO][TA_TIME];
        FXparams[1] = thrs.echo_setting[TAPE_ECHO][TA_FEEDBACK];
        FXparams[2] = thrs.echo_setting[TAPE_ECHO][TA_BASS];
        FXparams[3] = thrs.echo_setting[TAPE_ECHO][TA_TREBLE];
        FXparams[4] = thrs.echo_setting[TAPE_ECHO][TA_MIX];
        nFXbars = 5;
      break;

      case DIGITAL_DELAY:
        FXtitle = "D.D.";	// Set FX unit title
        if( thrs.unit[ECHO] )  // FX3 activated
        {
          FXbgcolour = TFT_THRDDLY;
          FXfgcolour = TFT_THRDIMDDLY;
        }
        else // FX3 deactivated
        {
          FXbgcolour = TFT_THRDIMDDLY;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.echo_setting[DIGITAL_DELAY][DD_TIME];
        FXparams[1] = thrs.echo_setting[DIGITAL_DELAY][DD_FEEDBACK];
        FXparams[2] = thrs.echo_setting[DIGITAL_DELAY][DD_BASS];
        FXparams[3] = thrs.echo_setting[DIGITAL_DELAY][DD_TREBLE];
        FXparams[4] = thrs.echo_setting[DIGITAL_DELAY][DD_MIX];
        nFXbars = 5;  
      break;
    }	// of switch(effecttype)
    
    drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams);
  }
  FXx += FXw;

 	// FX4 Reverb (Spring/Room/Plate/Hall)
  if( maskCUpdate & maskReverb )
  {
    switch( thrs.reverbtype )
    {
      case SPRING:
        FXtitle = "Spr";   // Set FX unit title
        if( thrs.unit[REVERB] ) // FX4 activated
        {
          FXbgcolour = TFT_THRSPR;
          FXfgcolour = TFT_THRDIMSPR;
        }
        else // FX4 deactivated
        {
          FXbgcolour = TFT_THRDIMSPR;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.reverb_setting[SPRING][SP_REVERB];
        FXparams[1] = thrs.reverb_setting[SPRING][SP_TONE];
        FXparams[2] = thrs.reverb_setting[SPRING][SP_MIX];
        FXparams[3] = 0;
        FXparams[4] = 0;
        nFXbars = 3;  
      break;

      case ROOM:
        FXtitle = "Room";  // Set FX unit title
        if( thrs.unit[REVERB] ) // FX4 activated
        {
          FXbgcolour = TFT_THRROOM;
          FXfgcolour = TFT_THRDIMROOM;
        }
        else // FX4 deactivated
        {
          FXbgcolour = TFT_THRDIMROOM;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.reverb_setting[ROOM][RO_DECAY];
        FXparams[1] = thrs.reverb_setting[ROOM][RO_PREDELAY];
        FXparams[2] = thrs.reverb_setting[ROOM][RO_TONE];
        FXparams[3] = thrs.reverb_setting[ROOM][RO_MIX];
        FXparams[4] = 0;
        nFXbars = 4;  
      break;

      case PLATE:
        FXtitle = "Plate"; // Set FX unit title
        if( thrs.unit[REVERB] ) // FX4 activated
        {
          FXbgcolour = TFT_THRPLATE;
          FXfgcolour = TFT_THRDIMPLATE;
        }
        else // FX4 deactivated
        {
          FXbgcolour = TFT_THRDIMPLATE;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.reverb_setting[PLATE][PL_DECAY];
        FXparams[1] = thrs.reverb_setting[PLATE][PL_PREDELAY];
        FXparams[2] = thrs.reverb_setting[PLATE][PL_TONE];
        FXparams[3] = thrs.reverb_setting[PLATE][PL_MIX];
        FXparams[4] = 0;
        nFXbars = 4;  
      break;

      case HALL:
        FXtitle = "Hall";	 // Set FX unit title
        if( thrs.unit[REVERB] ) // FX4 activated
        {
          FXbgcolour = TFT_THRHALL;
          FXfgcolour = TFT_THRDIMHALL;
        }
        else // FX4 deactivated
        {
          FXbgcolour = TFT_THRDIMHALL;
          FXfgcolour = TFT_THRVDARKGREY;
        }
        FXparams[0] = thrs.reverb_setting[HALL][HA_DECAY];
        FXparams[1] = thrs.reverb_setting[HALL][HA_PREDELAY];
        FXparams[2] = thrs.reverb_setting[HALL][HA_TONE];
        FXparams[3] = thrs.reverb_setting[HALL][HA_MIX];
        FXparams[4] = 0;
        nFXbars = 4;  
      break;
    }	// of switch(reverbtype)
      
    drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams);
  }
  FXx += FXw;
/*
  // Show THRII Guitar and Audio volume values
  if( maskCUpdate & maskVolumeAudio )
  {
    drawPPChart(FXx, FXy, 27, FXh, TFT_THRBROWN, TFT_THRCREAM, "VA", thrs.guitar_volume, thrs.audio_volume);
  }
*/
  if( maskCUpdate & maskPatchName )
  {
    String s2, s3;

    if( _uistate == UI_home_amp || (_uistate == UI_manual && _uistate_prev == UI_home_amp) )
    {
      if( thrs.thrSettings )
      {
        s2 = thrs.THRII_MODEL_NAME();
        if( s2 != "None") { s2 += " PANEL";       }
        else              { s2 = "NOT CONNECTED"; }
        drawPatchName(TFT_SKYBLUE, s2, thrs.boost_activated);
      }
      else if( thrs.getUserSettingsHaveChanged() )
      {
        s2 = thrs.getPatchName();
        drawPatchName(TFT_THRORANGE, s2, thrs.boost_activated);
      }
      else
      {
        s2 = thrs.getPatchName();
        drawPatchName(TFT_THRCREAM, s2, thrs.boost_activated);
      }
    }
    else if( _uistate == UI_home_patch || (_uistate == UI_manual && _uistate_prev == UI_home_patch) )
    {
      // If unchanged User Memory setting is active:
      if( !thrs.getUserSettingsHaveChanged() )
      {
        if( *presel_patch_id != *active_patch_id )
        {
          s2 = (*active_patch_names)[*presel_patch_id - 1]; // Patch names are 0-indexed
          drawPatchName(TFT_THRORANGE, s2, thrs.boost_activated);
        }
        else
        {
          s2 = (*active_patch_names)[*active_patch_id - 1]; // Patch names are 0-indexed
          drawPatchName(TFT_THRCREAM, s2, thrs.boost_activated);
        }
      }
      else
      {
        s2 = (*active_patch_names)[*active_patch_id - 1]; // Patch names are 0-indexed
        drawPatchName(TFT_THRORANGE, s2, thrs.boost_activated);
      }
    }
  }
  maskCUpdate = 0; // Display has been updated
}

uint8_t last_updated_sbox = 12;

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
void tftUpdateEdit(THR30II_Settings &thrs, uint32_t &maskCUpdate)
{
  if( maskCUpdate & maskClearTft ) // Clear TFT when switching to Edit mode
  {
    //tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  }
// =================================================================================

	// Use Patch select mode to indicate manual mode
  if( maskCUpdate )
  {
    drawPatchSelMode(TFT_THRCREAM, "EDIT");
    drawConnIcon( midi_connected ); // Just for completeness
  }

	// Amp select mode (COL/AMP/CAB); Highlighted only in Manual mode
  if( maskCUpdate )
  {
    uint16_t colour = TFT_THRCREAM;

    switch( amp_select_mode )
    {
      case COL: drawAmpSelMode(colour, "COL"); break;
      case AMP:	drawAmpSelMode(colour, "AMP"); break;
      case CAB: drawAmpSelMode(colour, "CAB"); break;
    }
  }
// =================================================================================

// FIXME: Get rid of hard-coded indexes.
// NOTE: Alignment is a must: updateStatusMask, FSM9b_1, enums definitions (THR30II.h)

  // FX1 Compressor -------------------------------------------------------------
  if( maskCUpdate & maskCompressor || (maskCUpdate & maskCompressorEn) )
  {
    if( last_updated_sbox != 0 )
    {
      sboxes[last_updated_sbox]->erase();
      last_updated_sbox = 0;
      sboxes[0]->draw();
    }
  }

  if( maskCUpdate & maskCompressorEn ) { sboxes[0]->setEnabled(thrs.unit[COMPRESSOR]); }

  if( maskCUpdate & maskCompressor )
  {
    sboxes[0]->draw_knob(0, thrs.compressor_setting[CO_SUSTAIN]);
    sboxes[0]->draw_knob(1, thrs.compressor_setting[CO_LEVEL]);
  }

	// Amp/Cabinet ---------------------------------------------------------------
  if( maskCUpdate & maskAmpUnit )
  {
    if( last_updated_sbox != 1 )
    {
      sboxes[last_updated_sbox]->erase();
      last_updated_sbox = 1;
      sboxes[1]->draw();
    }

    switch( thrs.col )
    {
      case BOUTIQUE: tft.setTextColor(TFT_BLUE,  TFT_BLACK, bgfill);	break;
      case MODERN:   tft.setTextColor(TFT_GREEN, TFT_BLACK, bgfill); 	break;
      case CLASSIC:  tft.setTextColor(TFT_RED,   TFT_BLACK, bgfill); 	break;
    }
    drawAmpUnit(40, 35, 240, 60, TFT_THRCREAM, TFT_THRBROWN, "Amp", thrs.col, thrs.amp, thrs.cab);    
  }

  if( maskCUpdate & maskGainMaster )
  {
    sboxes[1]->draw_knob(0, thrs.control[CTRL_GAIN]);
    sboxes[1]->draw_knob(1, thrs.control[CTRL_MASTER]);
    sboxes[1]->draw_knob(2, thrs.control[CTRL_BASS]);
    sboxes[1]->draw_knob(3, thrs.control[CTRL_MID]);
    sboxes[1]->draw_knob(4, thrs.control[CTRL_TREBLE]);
  }

 	// Gate -----------------------------------------------------------------------
  if( maskCUpdate & maskNoiseGate || (maskCUpdate & maskNoiseGateEn) )
  {
    if( last_updated_sbox != 2 )
    {
      sboxes[last_updated_sbox]->erase();
      last_updated_sbox = 2;
      sboxes[2]->draw();
    }
  }

  if( maskCUpdate & maskNoiseGateEn ) { sboxes[2]->setEnabled(thrs.unit[GATE]); }

  if( maskCUpdate & maskNoiseGate )
  {
    // TODO: Draw selected knob only???
    sboxes[2]->draw_knob(0, thrs.gate_setting[GA_THRESHOLD]);
    sboxes[2]->draw_knob(1, thrs.gate_setting[GA_DECAY]);    
  }

	// FX2 Effect (Chorus/Flanger/Phaser/Tremolo) ----------------------------------
  if( (maskCUpdate & maskFxUnit) || (maskCUpdate & maskFxUnitEn) )
  {
    switch( thrs.effecttype )
    {
      case CHORUS:
        if( last_updated_sbox != 3 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 3;
          sboxes[3]->draw();
        }

        if( maskCUpdate & maskFxUnitEn ) { sboxes[3]->setEnabled(thrs.unit[EFFECT]); }

        if( maskCUpdate & maskFxUnit )
        {
          sboxes[3]->draw_knob(0, thrs.effect_setting[CHORUS][CH_SPEED]);
          sboxes[3]->draw_knob(1, thrs.effect_setting[CHORUS][CH_DEPTH]);
          sboxes[3]->draw_knob(2, thrs.effect_setting[CHORUS][CH_PREDELAY]);
          sboxes[3]->draw_knob(3, thrs.effect_setting[CHORUS][CH_FEEDBACK]);
          sboxes[3]->draw_knob(4, thrs.effect_setting[CHORUS][CH_MIX]);
        }
      break;

      case FLANGER: 
        if( last_updated_sbox != 4 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 4;
          sboxes[4]->draw();
        }

        if( maskCUpdate & maskFxUnitEn ) { sboxes[4]->setEnabled(thrs.unit[EFFECT]); }

        if( maskCUpdate & maskFxUnit )
        {
          sboxes[4]->draw_knob(0, thrs.effect_setting[FLANGER][FL_SPEED]);
          sboxes[4]->draw_knob(1, thrs.effect_setting[FLANGER][FL_DEPTH]);    
          sboxes[4]->draw_knob(2, thrs.effect_setting[FLANGER][FL_MIX]);       
        }
      break;

      case PHASER:
        if( last_updated_sbox != 5 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 5;
          sboxes[5]->draw();
        }

        if( maskCUpdate & maskFxUnitEn ) { sboxes[5]->setEnabled(thrs.unit[EFFECT]); }

        if( maskCUpdate & maskFxUnit )
        {
          sboxes[5]->draw_knob(0, thrs.effect_setting[PHASER][PH_SPEED]);
          sboxes[5]->draw_knob(1, thrs.effect_setting[PHASER][PH_FEEDBACK]);    
          sboxes[5]->draw_knob(2, thrs.effect_setting[PHASER][PH_MIX]);       
        }
      break;		

      case TREMOLO:
        if( last_updated_sbox != 6 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 6;
          sboxes[6]->draw();
        }

        if( maskCUpdate & maskFxUnitEn ) { sboxes[6]->setEnabled(thrs.unit[EFFECT]); }

        if( maskCUpdate & maskFxUnit )
        {
          sboxes[6]->draw_knob(0, thrs.effect_setting[TREMOLO][TR_SPEED]);
          sboxes[6]->draw_knob(1, thrs.effect_setting[TREMOLO][TR_DEPTH]);    
          sboxes[6]->draw_knob(2, thrs.effect_setting[TREMOLO][TR_MIX]);       
        }
      break;
    } // of switch(effecttype)
  }

  // FX3 Echo (Tape Echo/Digital Delay)
  if( (maskCUpdate & maskEcho) || (maskCUpdate & maskEchoEn) )
  {
    switch( thrs.echotype )
    {
      case TAPE_ECHO:
        if( last_updated_sbox != 7 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 7;
          sboxes[7]->draw();
        }

        if( maskCUpdate & maskEchoEn ) { sboxes[7]->setEnabled(thrs.unit[ECHO]); }

        if( maskCUpdate & maskEcho )
        {
          sboxes[7]->draw_knob(0, thrs.echo_setting[TAPE_ECHO][TA_TIME]);
          sboxes[7]->draw_knob(1, thrs.echo_setting[TAPE_ECHO][TA_FEEDBACK]);    
          sboxes[7]->draw_knob(2, thrs.echo_setting[TAPE_ECHO][TA_BASS]);       
          sboxes[7]->draw_knob(3, thrs.echo_setting[TAPE_ECHO][TA_TREBLE]);       
          sboxes[7]->draw_knob(4, thrs.echo_setting[TAPE_ECHO][TA_MIX]);       
        }
      break;

      case DIGITAL_DELAY:
        if( last_updated_sbox != 8 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 8;
          sboxes[8]->draw();
        }

        if( maskCUpdate & maskEchoEn ) { sboxes[8]->setEnabled(thrs.unit[ECHO]); }

        if( maskCUpdate & maskEcho )
        {
          sboxes[8]->draw_knob(0, thrs.echo_setting[DIGITAL_DELAY][DD_TIME]);
          sboxes[8]->draw_knob(1, thrs.echo_setting[DIGITAL_DELAY][DD_FEEDBACK]);    
          sboxes[8]->draw_knob(2, thrs.echo_setting[DIGITAL_DELAY][DD_BASS]);       
          sboxes[8]->draw_knob(3, thrs.echo_setting[DIGITAL_DELAY][DD_TREBLE]);       
          sboxes[8]->draw_knob(4, thrs.echo_setting[DIGITAL_DELAY][DD_MIX]);       
        } 
      break;
    }	// of switch(echotype)
  }

 	// FX4 Reverb (Spring/Room/Plate/Hall)
  if( (maskCUpdate & maskReverb) || (maskCUpdate & maskReverbEn) )
  {
    switch( thrs.reverbtype )
    {
      case SPRING:
        if( last_updated_sbox != 9 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 9;
          sboxes[9]->draw();
        }
        if( maskCUpdate & maskReverbEn ) { sboxes[9]->setEnabled(thrs.unit[REVERB]); }

        if( maskCUpdate & maskReverb )
        {
          sboxes[9]->draw_knob(0, thrs.reverb_setting[SPRING][SP_REVERB]);
          sboxes[9]->draw_knob(1, thrs.reverb_setting[SPRING][SP_TONE]);    
          sboxes[9]->draw_knob(2, thrs.reverb_setting[SPRING][SP_MIX]);           
        }
      break;

      case ROOM:
        if( last_updated_sbox != 12 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 12;
          sboxes[12]->draw();
        }

        if( maskCUpdate & maskReverbEn ) { sboxes[12]->setEnabled(thrs.unit[REVERB]); }

        if( maskCUpdate & maskReverb )
        {
          sboxes[12]->draw_knob(0, thrs.reverb_setting[ROOM][RO_DECAY]);
          sboxes[12]->draw_knob(1, thrs.reverb_setting[ROOM][RO_PREDELAY]);    
          sboxes[12]->draw_knob(2, thrs.reverb_setting[ROOM][RO_TONE]);           
          sboxes[12]->draw_knob(3, thrs.reverb_setting[ROOM][RO_MIX]);           
        }
      break;

      case PLATE:
        if( last_updated_sbox != 10 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 10;
          sboxes[10]->draw();
        }

        if( maskCUpdate & maskReverbEn ) { sboxes[10]->setEnabled(thrs.unit[REVERB]); }

        if( maskCUpdate & maskReverb )
        {
          sboxes[10]->draw_knob(0, thrs.reverb_setting[PLATE][PL_DECAY]);
          sboxes[10]->draw_knob(1, thrs.reverb_setting[PLATE][PL_PREDELAY]);    
          sboxes[10]->draw_knob(2, thrs.reverb_setting[PLATE][PL_TONE]);           
          sboxes[10]->draw_knob(3, thrs.reverb_setting[PLATE][PL_MIX]);           
        }
      break;

      case HALL:
        if( last_updated_sbox != 11 )
        {
          sboxes[last_updated_sbox]->erase();
          last_updated_sbox = 11;
          sboxes[11]->draw();
        }

        if( maskCUpdate & maskReverbEn ) { sboxes[11]->setEnabled(thrs.unit[REVERB]); }

        if( maskCUpdate & maskReverb )
        {
          sboxes[11]->draw_knob(0, thrs.reverb_setting[HALL][HA_DECAY]);
          sboxes[11]->draw_knob(1, thrs.reverb_setting[HALL][HA_PREDELAY]);    
          sboxes[11]->draw_knob(2, thrs.reverb_setting[HALL][HA_TONE]);           
          sboxes[11]->draw_knob(3, thrs.reverb_setting[HALL][HA_MIX]);           
        }
      break;
    }	// of switch(reverbtype)
  }
  maskCUpdate = 0; // Display has been updated
}
