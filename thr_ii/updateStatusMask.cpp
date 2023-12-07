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

// Locally supplied fonts
// #fonts/include "Free_Fonts.h"
#include "fonts/NotoSansBold15.h"
#include "fonts/NotoSansBold36.h"
#include "fonts/NotoSansMonoSCB20.h"
#include "fonts/Latin_Hiragana_24.h"
//#include "fonts/Final_Frontier_28.h"

// The font names are arrays references, thus must NOT be in quotes ""
#define AA_FONT_SMALL NotoSansBold15
//#define AA_FONT_LARGE NotoSansBold36
#define AA_FONT_LARGE Latin_Hiragana_24
#define AA_FONT_MONO  NotoSansMonoSCB20 // NotoSansMono-SemiCondensedBold 20pt

extern std::vector <String> libraryPatchNames;

extern uint16_t npatches;
extern int16_t presel_patch_id;   	 // ID of actually pre-selected patch (absolute number)
extern int16_t active_patch_id;   	 // ID of actually selected patch     (absolute number)
extern bool send_patch_now; // pre-select patch to send (false) or send immediately (true)

extern byte maskUpdate;
/////////////////////////

ampSelectModes amp_select_mode = COL; // Currently used in updateStatusMask()

void drawPatchID(uint16_t fgcolour, int patchID)
{
 	int x = 0, y = 0, w = 80, h = 80;
	uint16_t bgcolour = TFT_THRVDARKGREY;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, bgcolour, TFT_BLACK);
	spr.setTextFont(7);
	spr.setTextSize(1);
	spr.setTextDatum(MR_DATUM);
	spr.setTextColor(TFT_BLACK);
	spr.drawNumber(88, w-7, h/2);
	spr.setTextColor(fgcolour);
	spr.drawNumber(patchID, w-7, h/2);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawPatchIconBank(int presel_patch_id, int active_patch_id, int8_t active_user_setting)
{
	uint16_t iconcolour = 0;
	int banksize = 5;
	int first = ((presel_patch_id-1)/banksize)*banksize+1; // Find number for 1st icon in row
	
	switch( _uistate ) 
  {
		case UI_home_amp:
			for(int i = 1; i <= 5; i++)
			{
				if( i == active_user_setting + 1 )
				{
          // Note: if user preset is selected and parameter is changed via THRII, then getActiveUserSetting() returns -1
					iconcolour = TFT_SKYBLUE; // Highlight active user preset patch icon
				}
				else if( i == presel_patch_id )
				{
					iconcolour = ST7789_ORANGE; // Highlight pre-selected patch icon
				}
				else
				{
					iconcolour = TFT_THRBROWN; // Colour for unselected patch icons
				}
				if( i > npatches )
				{
					iconcolour = TFT_BLACK;
				}
				drawPatchIcon(60 + 20*i, 0, 20, 20, iconcolour, i);
			}
		break;
		
		case UI_home_patch:
			for(int i = first; i < first+banksize; i++)
			{
				if( i == active_patch_id )
				{
					iconcolour = TFT_THRCREAM; // Highlight active patch icon
				}
				else if( i == presel_patch_id )
				{
					iconcolour = ST7789_ORANGE; // Highlight pre-selected patch icon
				}
				else
				{
					iconcolour = TFT_THRBROWN; // Colour for unselected patch icons
				}
				if( i > npatches )
				{
					iconcolour = TFT_BLACK;
				}
				drawPatchIcon(60 + 20*(i-first+1), 0, 20, 20, iconcolour, i);
			}
		break;

		default:
		break;
	}
}

void drawPatchIcon(int x, int y, int w, int h, uint16_t colour, int patchID)
{
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawNumber(patchID, 10, 11);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawPatchSelMode(uint16_t colour)
{
  int x = 180, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawString("NOW", w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawAmpSelMode(uint16_t colour, String string)
{
  int x = 230, y = 0, w = 50, h = 20;
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w-1, h-1, 3, colour, TFT_BLACK);
  spr.loadFont(AA_FONT_SMALL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_BLACK, colour);
  spr.drawString(string, w/2, h/2+1);
  spr.pushSprite(x, y);
  spr.deleteSprite();
}

void drawConnIcon(bool THRconnected)
{
  spr.createSprite(40, 20);
  if( THRconnected ) // Connected: green
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_GREEN, TFT_BLACK); // Bg
    spr.drawWideLine(1, 9, 14, 9, 3, TFT_WHITE, TFT_GREEN);         // Left wire
    spr.drawWideLine(26, 9, 38, 9, 3, TFT_WHITE, TFT_GREEN);        // Right wire
    spr.fillSmoothRoundRect(12, 2, 8, 15, 3, TFT_WHITE, TFT_GREEN); // Plug
    spr.fillRect(16, 2, 4, 15, TFT_WHITE);                          // Plug
    spr.fillSmoothRoundRect(21, 2, 8, 15, 3, TFT_WHITE, TFT_GREEN); // Socket
    spr.fillRect(21, 2, 4, 15, TFT_WHITE);                          // Socket
    spr.fillRect(16, 4, 5, 3, TFT_WHITE);                           // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_WHITE);                          // Lower prong
  }
  else // Disconnected: red 
  {
    spr.fillSmoothRoundRect(0, 0, 40, 19, 3, TFT_RED, TFT_BLACK); // Bg
    spr.drawWideLine(1, 9, 10, 9, 3, TFT_WHITE, TFT_RED);         // Left wire
    spr.drawWideLine(30, 9, 38, 9, 3, TFT_WHITE, TFT_RED);        // Right wire
    spr.fillSmoothRoundRect(8, 2, 8, 15, 3, TFT_WHITE, TFT_RED);  // Plug
    spr.fillRect(12, 2, 4, 15, TFT_WHITE);                        // Plug
    spr.fillSmoothRoundRect(24, 2, 8, 15, 3, TFT_WHITE, TFT_RED); // Socket
    spr.fillRect(24, 2, 4, 15, TFT_WHITE);                        // Socket
    spr.fillRect(16, 4, 5, 3, TFT_WHITE);                         // Upper prong
    spr.fillRect(16, 12, 5, 3, TFT_WHITE);                        // Lower prong
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
  int x = 80, y = 20, w = 240, h = 60;
  uint16_t fg_colour = fgcolour;
  uint16_t bg_colour = TFT_THRVDARKGREY;
  if( inverted )
  {
    fg_colour = TFT_THRVDARKGREY;
    bg_colour = fgcolour;
  }
  spr.createSprite(w, h);
  spr.fillSmoothRoundRect(0, 0, w, h-1, 3, bg_colour, TFT_BLACK);
  spr.loadFont(AA_FONT_LARGE);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fg_colour, bg_colour);

  if( spr.textWidth(patchname) > w )
  {
    uint8_t pos = getSplitPos( patchname );
    String line1 = patchname.substring(0, pos); // TODO: Split at space char
    String line2 = patchname.substring(pos, patchname.length());
    spr.drawString(line1, w/2, h/2-11);
    spr.drawString(line2, w/2, h/2+12);
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
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour);
  spr.drawString(label, w/2-1, 11);
  spr.fillRect(pad, h-(h-tpad-pad)*barh/100, w-1-2*pad, (h-tpad-pad)*barh/100, fgcolour);
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
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fgcolour, bgcolour);
  spr.drawString(label, w/2, 11);
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
	spr.setTextDatum(ML_DATUM);
	spr.setTextColor(bgcolour, fgcolour);
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
	int tpad = 30; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = 0; int barh = grh/2;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h-bpad, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2, 16);
	spr.fillRect(grx, gry, grw, grh, fgcolour);	// Draw graph area
	barw = grw * utilparams[0]/100;
	spr.fillRect(grx, gry, barw, barh, bgcolour);
	barw = grw * utilparams[1]/100;
	spr.fillRect(grx, gry+barh, barw, barh+1, bgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawFXUnit(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int nbars, double FXparams[5], int selectedFXparam)
{
	int tpad = 30; int pad = 2;
	int grx = pad; int gry = tpad; int grw = w-1-2*pad; int grh = h-1-tpad-pad;
	int barw = grw / nbars;	// Calculate bar width
	int barh = 0;
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);	// Draw FX unit
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2, 16);	// Write label
	spr.fillRect(grx, gry, grw, grh, fgcolour);	// Draw graph area
	for(int i = 0; i < nbars; i++)
	{
		barh = grh * FXparams[i] / 100;	// calculate bar height
		if( nbars == 4 ) 
    {
			barw = grw / nbars + 1;
		}
		if( i == nbars - 1 ) { // Widen last bar by 1px
			spr.fillRect(grx + i * barw, gry + grh - barh, barw+1, barh, bgcolour);
		}
    else 
    {
			spr.fillRect(grx + i * barw, gry + grh - barh, barw, barh, bgcolour);
		}
	}
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawPPChart(int x, int y, int w, int h, uint16_t bgcolour, uint16_t fgcolour, String label, int ped1, int ped2)
{
  // Used to represent Guitar Volume and Audio Volume
	int tpad = 20; int pad = 0;
	int ped1height = ped1*(h-tpad-pad)/100; // For guitar and audio volume
	int ped2height = ped2*(h-tpad-pad)/100; // 
	spr.createSprite(w, h);
	spr.fillSmoothRoundRect(0, 0, w-1, h, 3, bgcolour, TFT_BLACK);
	spr.loadFont(AA_FONT_SMALL);
	spr.setTextDatum(MC_DATUM);
	spr.setTextColor(fgcolour, bgcolour);
	spr.drawString(label, w/2-1, 11);
	spr.fillRect(pad, h-ped1height, w/2-1-2*pad, ped1height, fgcolour);
	spr.fillRect(w/2+pad, h-ped2height, w/2-1-2*pad, ped2height, fgcolour);
	spr.pushSprite(x, y);
	spr.unloadFont();
	spr.deleteSprite();
}

void drawFXUnitEditMode()
{
}

void THR30II_Settings::updateConnectedBanner() // Show the connected Model 
{
	// //Display "THRxxII" in blue (as long as connection is established)

	// s1=THRxxII_MODEL_NAME();
	// if(ConnectedModel != 0)
	// {
	// 	tft.loadFont(AA_FONT_LARGE); // Set current font    
	// 	tft.setTextSize(1);
	// 	tft.setTextColor(TFT_BLUE, TFT_BLACK);
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
//void THR30II_Settings::updateStatusMask(uint8_t x, uint8_t y)
void updateStatusMask(uint8_t x, uint8_t y, THR30II_Settings &thrs)
{
	// Patch number
  drawPatchID(TFT_THRCREAM, active_patch_id);
	
	// Patch icon bank
	drawPatchIconBank(presel_patch_id, active_patch_id, thrs.getActiveUserSetting());

	// Patch select mode (pre-select/immediate) & UI mode
	switch( _uistate )
  {
		case UI_home_amp:
			if( send_patch_now ) { drawPatchSelMode(TFT_THRCREAM); }
			else                 { drawPatchSelMode(TFT_THRBROWN); }
		  break;

		case UI_home_patch:
			if( send_patch_now ) { drawPatchSelMode(TFT_THRCREAM); }
			else                 { drawPatchSelMode(TFT_THRBROWN); }
		  break;

		case UI_edit:
		  break;
		default:
		  break;
	}

	// Amp select mode (COL/AMP/CAB)
	switch( amp_select_mode )
	{
		case COL:
			drawAmpSelMode(TFT_THRCREAM, "COL");
		  break;

		case AMP:
			drawAmpSelMode(TFT_THRCREAM, "AMP");
		  break;

		case CAB:
			drawAmpSelMode(TFT_THRCREAM, "CAB");
		  break;
	}

	String FXtitle;
	uint16_t FXbgcolour  =  0;
	uint16_t FXfgcolour  =  0;
	double FXparams[5]   = {0};
	double utilparams[2] = {0};
	uint8_t FXx;
	uint8_t FXy;
	uint8_t FXw =  60;
	uint8_t FXh = 100;
	uint8_t nFXbars = 5;
	uint8_t selectedFXparam = 0;

	// Gain, Master, and EQ (B/M/T) ---------------------------------------------------------
  drawBarChart( 0, 80, 15, 160, TFT_THRBROWN, TFT_THRCREAM,  "G", thrs.control[CTRL_GAIN]);
  if( thrs.boost_activated )
  {
    drawBarChart(15, 80, 15, 160, TFT_THRDIMORANGE, TFT_THRORANGE,  "M", thrs.control[CTRL_MASTER]);
   	drawEQChart( 30, 80, 30, 160, TFT_THRDIMORANGE, TFT_THRORANGE, "EQ", thrs.control[CTRL_BASS], thrs.control[CTRL_MID], thrs.control[CTRL_TREBLE]);
  }
  else
  {
    drawBarChart(15, 80, 15, 160, TFT_THRBROWN, TFT_THRCREAM,  "M", thrs.control[CTRL_MASTER]);
   	drawEQChart( 30, 80, 30, 160, TFT_THRBROWN, TFT_THRCREAM, "EQ", thrs.control[CTRL_BASS], thrs.control[CTRL_MID], thrs.control[CTRL_TREBLE]);
  }

	// Amp/Cabinet ---------------------------------------------------------------
	switch( thrs.col )
	{
		case BOUTIQUE:
			tft.setTextColor(TFT_BLUE, TFT_BLACK);
		break;

		case MODERN:
			tft.setTextColor(TFT_GREEN, TFT_BLACK);
		break;

		case CLASSIC:
			tft.setTextColor(TFT_RED, TFT_BLACK);
		break;
	}
  drawAmpUnit(60, 80, 240, 60, TFT_THRCREAM, TFT_THRBROWN, "Amp", thrs.col, thrs.amp, thrs.cab);
	
	// FX1 Compressor -------------------------------------------------------------
	utilparams[0] = thrs.compressor_setting[CO_SUSTAIN];
	utilparams[1] = thrs.compressor_setting[CO_LEVEL];
  if(thrs.unit[COMPRESSOR]) { drawUtilUnit(60, 140, 60, 50, 1, TFT_THRWHITE, TFT_THRDARKGREY,  "Comp", utilparams); }
  else                      { drawUtilUnit(60, 140, 60, 50, 1, TFT_THRGREY,  TFT_THRVDARKGREY, "Comp", utilparams); }
	
 	// Gate -----------------------------------------------------------------------
	utilparams[0] = thrs.gate_setting[GA_THRESHOLD];
	utilparams[1] = thrs.gate_setting[GA_DECAY];
  if(thrs.unit[GATE]) {	drawUtilUnit(60, 190, 60, 50, 0, TFT_THRYELLOW,    TFT_THRDIMYELLOW, "Gate", utilparams); }
  else                { drawUtilUnit(60, 190, 60, 50, 0, TFT_THRDIMYELLOW, TFT_THRVDARKGREY, "Gate", utilparams); }

	// FX2 Effect (Chorus/Flanger/Phaser/Tremolo) ----------------------------------
	switch( thrs.effecttype )
	{
		case CHORUS:
			FXtitle = "Chor";	 // Set FX unit title
			if( thrs.unit[EFFECT] ) // FX2 activated
			{
				FXbgcolour = TFT_THRFORESTGREEN;
				FXfgcolour = TFT_THRDIMFORESTGREEN;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMFORESTGREEN;
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
				FXbgcolour = TFT_THRLIME;
				FXfgcolour = TFT_THRDIMLIME;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMLIME;
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
				FXbgcolour = TFT_THRLEMON;
				FXfgcolour = TFT_THRDIMLEMON;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMLEMON;
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
				FXbgcolour = TFT_THRMANGO;
				FXfgcolour = TFT_THRDIMMANGO;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMMANGO;
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
		
	FXx = 120; // Set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

  // FX3 Echo (Tape Echo/Digital Delay)
  switch( thrs.echotype )
	{
		case TAPE_ECHO:
			FXtitle = "Tape";	// Set FX unit title
			if( thrs.unit[ECHO] )  // FX3 activated
			{
				FXbgcolour = TFT_THRROYALBLUE;
				FXfgcolour = TFT_THRDIMROYALBLUE;
			}
			else // FX2 deactivated
			{
				FXbgcolour = TFT_THRDIMROYALBLUE;
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
				FXbgcolour = TFT_THRSKYBLUE;
				FXfgcolour = TFT_THRDIMSKYBLUE;
			}
			else // FX3 deactivated
			{
				FXbgcolour = TFT_THRDIMSKYBLUE;
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
	
	FXx = 180;	// set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

 	// FX4 Reverb (Spring/Room/Plate/Hall)
	switch( thrs.reverbtype )
	{
		case SPRING:
			FXtitle = "Spr";   // Set FX unit title
			if( thrs.unit[REVERB] ) // FX4 activated
			{
				FXbgcolour = TFT_THRRED;
				FXfgcolour = TFT_THRDIMRED;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMRED;
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
				FXbgcolour = TFT_THRMAGENTA;
				FXfgcolour = TFT_THRDIMMAGENTA;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMMAGENTA;
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
				FXbgcolour = TFT_THRPURPLE;
				FXfgcolour = TFT_THRDIMPURPLE;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMPURPLE;
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
				FXbgcolour = TFT_THRVIOLET;
				FXfgcolour = TFT_THRDIMVIOLET;
			}
			else // FX4 deactivated
			{
				FXbgcolour = TFT_THRDIMVIOLET;
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
		
	FXx = 240; // Set FX unit position
	FXy = 140;
	drawFXUnit(FXx, FXy, FXw, FXh, FXbgcolour, FXfgcolour, FXtitle, nFXbars, FXparams, selectedFXparam);

  // Show THRII Guitar and Audio volume values
  drawPPChart(300, 80, 20, 160, TFT_THRBROWN, TFT_THRCREAM, "VA", thrs.guitar_volume, thrs.audio_volume);

	String s2, s3;
	
	switch( _uistate )
	{
	  case UI_home_amp: // !patchActive
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
				drawPatchName(ST7789_ORANGERED, s2, thrs.boost_activated);
			}
			else
			{
				// Update GUI status line
				s2 = thrs.getPatchName();
				drawPatchName(TFT_SKYBLUE, s2, thrs.boost_activated);
			}
	 	break;

		case UI_home_patch:	// Patch is active
			// If unchanged User Memory setting is active:
			if( !thrs.getUserSettingsHaveChanged() )
			{
				if( presel_patch_id != active_patch_id )
				{
					s2 = libraryPatchNames[presel_patch_id - 1]; // libraryPatchNames is 0-indexed
					drawPatchName(ST7789_ORANGE, s2, thrs.boost_activated);
				}
				else
				{
					s2 = libraryPatchNames[active_patch_id - 1];
					drawPatchName(TFT_THRCREAM, s2, thrs.boost_activated);
				}
			}
			else
			{
				s2 = libraryPatchNames[active_patch_id - 1]; // libraryPatchNames is 0-indexed; "(*)" removed to save space
				drawPatchName(ST7789_ORANGERED, s2, thrs.boost_activated);
			}
		break;

		default:
		break;
	}
	maskUpdate = false; // Tell local GUI that mask has been updated
}
