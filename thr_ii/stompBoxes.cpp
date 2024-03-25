#include "stompBoxes.h"

//*
std::vector <String> param_amp = {" Gain ", " Master ", " Bass ", " Mid ", " Treble "};
std::vector <String> param_comprs = {" Sustain ", " Level "};
std::vector <String> param_nsgate = {" Threshold ", " Decay "};

std::vector <String> param_chorus  = {" Speed ", " Depth ",  " Pre-Dly ", " FeedBk ", " Mix "};  // Depth=OK, Mix=OK, idx[0]==idx[3], idx[3]==idx[2], idx[2]==idx[0]
std::vector <String> param_flanger = {" Speed ", " Depth ",  " Mix "}; // Mix=OK, others swapped
std::vector <String> param_phaser  = {" Speed ", " FeedBk ", " Mix "}; // OK
std::vector <String> param_tremolo = {" Speed ", " Depth ",  " Mix "}; // OK

std::vector <String> param_tape = {" Time ", " Feedback ", " Bass ", " Treble ", " Mix "};
std::vector <String> param_ddly = {" Time ", " Feedback ", " Bass ", " Treble ", " Mix "};

std::vector <String> param_spring = {" Reverb ", " Tone ", " Mix "};
std::vector <String> param_room   = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};
std::vector <String> param_plate  = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};
std::vector <String> param_hall   = {" Decay ", " Pre-Dly ", " Tone ", " Mix "};
/*/

// Because we use indexes in the Edit mode, the order must be consistent with the THRII settings definitions
std::vector <String> param_amp = {" Gain ", " Master ", " Bass ", " Mid ", " Treble "};
std::vector <String> param_comprs = {" Sustain ", " Level "};
std::vector <String> param_nsgate = {" Threshold ", " Decay "};

std::vector <String> param_chorus  = {" FeedBk ", " Depth ",  " Speed ", " Pre-Dly ", " Mix "};
//enum THR30II_EFF_SET_CHOR { CH_FEEDBACK, CH_DEPTH, CH_SPEED, CH_PREDELAY, CH_MIX, CH_SYNCSELECT };
std::vector <String> param_flanger = {" Depth ",  " Speed ", " Mix "};
//enum THR30II_EFF_SET_FLAN { FL_DEPTH, FL_SPEED, FL_MIX };
std::vector <String> param_phaser  = {" Speed ", " FeedBk ", " Mix "};
//enum THR30II_EFF_SET_PHAS { PH_SPEED, PH_FEEDBACK, PH_MIX };
std::vector <String> param_tremolo = {" Speed ", " Depth ",  " Mix "};
//enum THR30II_EFF_SET_TREM { TR_SPEED, TR_DEPTH, TR_MIX };

std::vector <String> param_tape = {" Bass ", " Treble ", " Feedback ", " Time ", " Mix "};
//enum THR30II_ECHO_SET_TAPE { TA_BASS, TA_TREBLE, TA_FEEDBACK, TA_TIME, TA_MIX, TA_SYNCSELECT }; 
std::vector <String> param_ddly = {" Bass ", " Treble ", " Feedback ", " Time ", " Mix "};
//enum THR30II_ECHO_SET_DIGI { DD_BASS, DD_TREBLE, DD_FEEDBACK, DD_TIME, DD_MIX, DD_SYNCSELECT };

std::vector <String> param_spring = {" Reverb ", " Tone ", " Mix "};
//enum THR30II_REV_SET_SPRI { SP_REVERB, SP_TONE, SP_MIX };
std::vector <String> param_room   = {" Decay ", " Tone ", " Pre-Dly ", " Mix "};
//enum THR30II_REV_SET_ROOM { RO_DECAY, RO_TONE, RO_PREDELAY, RO_MIX };
std::vector <String> param_plate  = {" Decay ", " Tone ", " Pre-Dly ", " Mix "};
//enum THR30II_REV_SET_PLAT { PL_DECAY, PL_TONE, PL_PREDELAY, PL_MIX };
std::vector <String> param_hall   = {" Decay ", " Tone ", " Pre-Dly ", " Mix "};
//enum THR30II_REV_SET_HALL { HA_DECAY, HA_TONE, HA_PREDELAY, HA_MIX };
//*/

AmpBox abox("AMP", param_amp, TFT_THRAMP, TFT_THRDIMAMP);

StompBox sbox1("COMPRESSOR", param_comprs, TFT_THRWHITE, TFT_THRDARKGREY);
StompBox sbox2("NOISE GATE", param_nsgate, TFT_THRYELLOW, TFT_THRDIMYELLOW);

// enum THR30II_EFF_TYPES { PHASER, TREMOLO, FLANGER, CHORUS }
StompBox sbox3("CHORUS", param_chorus, TFT_THRFORESTGREEN, TFT_THRDIMFORESTGREEN);
StompBox sbox4("FLANGER", param_flanger, TFT_THRLIME, TFT_THRDIMLIME);
StompBox sbox5("PHASER", param_phaser, TFT_THRLEMON, TFT_THRDIMLEMON);
StompBox sbox6("TREMOLO", param_tremolo, TFT_THRMANGO, TFT_THRDIMMANGO);

// enum THR30II_ECHO_TYPES { TAPE_ECHO, DIGITAL_DELAY };
StompBox sbox7("TAPE ECHO", param_tape, TFT_THRROYALBLUE, TFT_THRDIMROYALBLUE);
StompBox sbox8("DIGITAL DELAY", param_ddly, TFT_THRSKYBLUE, TFT_THRDIMSKYBLUE);

//enum THR30II_REV_TYPES { SPRING, PLATE, HALL, ROOM };
StompBox sbox9("SPRING REVERB", param_spring, TFT_THRRED, TFT_THRDIMRED);
StompBox sbox10("PLATE REVERB", param_plate, TFT_THRPURPLE, TFT_THRDIMPURPLE);
StompBox sbox11("HALL REVERB", param_hall, TFT_THRVIOLET, TFT_THRDIMVIOLET);
StompBox sbox12("ROOM REVERB", param_room, TFT_THRMAGENTA, TFT_THRDIMMAGENTA);

std::vector <StompBox*> sboxes = {&abox, &sbox1, &sbox2, &sbox3, &sbox4, &sbox5, &sbox6, &sbox7, &sbox8, &sbox9, &sbox10, &sbox11, &sbox12};
