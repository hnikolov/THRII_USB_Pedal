#include "stompBoxes.h"

std::vector <String> param_amp = {" Gain ", " Master ", " Bass ", " Mid ", " Treble "};
std::vector <String> param_comprs = {" Sustain ", " Level "};
std::vector <String> param_nsgate = {" Threshold ", " Decay "};

std::vector <String> param_chorus  = {" Speed ", " Depth ",  " Pre-Dly ", " FeedBk ", " Mix "};
std::vector <String> param_flanger = {" Speed ", " Depth ",  " Mix "};
std::vector <String> param_phaser  = {" Speed ", " FeedBk ", " Mix "};
std::vector <String> param_tremolo = {" Speed ", " Depth ",  " Mix "};

std::vector <String> param_tape = {" Time ", " Feedback ", " Bass ", " Treble ", " Mix "};
std::vector <String> param_ddly = {" Time ", " Feedback ", " Bass ", " Treble ", " Mix "};

std::vector <String> param_spring = {" Reverb ", " Tone ", " Mix "};
std::vector <String> param_room   = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};
std::vector <String> param_plate  = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};
std::vector <String> param_hall   = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};

AmpBox abox("AMP", param_amp, TFT_THRAMP, TFT_THRDIMAMP);

StompBox sbox1("CMPRESSOR", param_comprs, TFT_THRWHITE, TFT_THRDARKGREY);
StompBox sbox2("NOISE GATE", param_nsgate, TFT_THRYELLOW, TFT_THRDIMYELLOW);

StompBox sbox3("CHORUS", param_chorus, TFT_THRFORESTGREEN, TFT_THRDIMFORESTGREEN);
StompBox sbox4("FLANGER", param_flanger, TFT_THRLIME, TFT_THRDIMLIME);
StompBox sbox5("PHASER", param_phaser, TFT_THRLEMON, TFT_THRDIMLEMON);
StompBox sbox6("TREMOLO", param_tremolo, TFT_THRMANGO, TFT_THRDIMMANGO);

StompBox sbox7("TAPE ECHO", param_tape, TFT_THRROYALBLUE, TFT_THRDIMROYALBLUE);
StompBox sbox8("DIGITAL DELAY", param_ddly, TFT_THRSKYBLUE, TFT_THRDIMSKYBLUE);

StompBox sbox9("SPRING REVB", param_spring, TFT_THRRED, TFT_THRDIMRED);
StompBox sbox10("ROOM REVERB", param_room, TFT_THRMAGENTA, TFT_THRDIMMAGENTA);
StompBox sbox11("PLATE REVERB", param_plate, TFT_THRPURPLE, TFT_THRDIMPURPLE);
StompBox sbox12("HALL REVERB", param_hall, TFT_THRVIOLET, TFT_THRDIMVIOLET);

std::vector <StompBox*> sboxes = {&abox, &sbox1, &sbox2, &sbox3, &sbox4, &sbox5, &sbox6, &sbox7, &sbox8, &sbox9, &sbox10, &sbox11, &sbox12};
