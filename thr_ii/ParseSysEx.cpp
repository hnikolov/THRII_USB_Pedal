//#include <stdexcept> // exceptions are problematic on Arduino / Teensy we avoid them
#include "THR30II_Pedal.h"
#include "Globals.h"
#include "trace.h"
#include <CRC32.h> // https://github.com/bakercp/CRC32

extern uint32_t maskCUpdate;

extern uint32_t maskPatchID;
extern uint32_t maskPatchIconBank;
extern uint32_t maskPatchSelMode;
extern uint32_t maskPatchSet;
extern uint32_t maskAmpSelMode;
extern uint32_t maskConnIcon; // Not used
extern uint32_t maskPatchName;
extern uint32_t maskGainMaster;
extern uint32_t maskVolumeAudio;
extern uint32_t maskAmpUnit;
extern uint32_t maskCompressor;
extern uint32_t maskCompressorEn;
extern uint32_t maskNoiseGate;
extern uint32_t maskNoiseGateEn;
extern uint32_t maskFxUnit;
extern uint32_t maskFxUnitEn;
extern uint32_t maskEcho;
extern uint32_t maskEchoEn;
extern uint32_t maskReverb;
extern uint32_t maskReverbEn;
extern uint32_t maskAll;

extern uint8_t selected_sbox; // Used in Edit mode

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function walks through an incoming MIDI-SysEx-Message and parses it's meaning
// cur[] : the buffer
// cur_len : length of the buffer
// returns a message String, describing the result (for writing to Serial Monitor)
// If a valid message is found, appropriate actions are taken (setting values, responding messages to follow the protocol)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
String THR30II_Settings::ParseSysEx(const byte cur[], int cur_len)  
{
    if( cur_len < 5 ) { return "SysEx too short!"; }

    std::map<String, uint16_t> &glob = Constants::glo; // Reference to the symbol table (received in the forefield)

    sendChangestoTHR = false; // Do not send THR-caused setting changes back to THRII

    // Parsing a valid message
    String result = "";
    
    if( cur[2] == 0x01 && cur[3] == 0x0c && cur[4] == 0x24 ) // Valid SysEx start : THR30II (LINE6 MIDI-ID)
    {
        // NOTE: Perhaps check here if counter is 0x00 or number of last received THR30II-SysEx

        TRACE_THR30IIPEDAL(result += "\n\rTHR30_II:";)

        // NOTE: Assign this in every call. Instead, do it just once?
        //       Use F_ID and M_NR in the subsequent communication
        F_ID = cur[4]; // e.g., 0x24
        M_NR = cur[5]; // e.g., 0x01 (THR10II-W)
        //Serial.println("Family ID: " + String(F_ID, HEX) + ", Model Nr: " + String(M_NR, HEX));
        // ----------------------------------------------------------

        // uint16_t familyID = (uint16_t)(((uint16_t)cur[5] << 8) + (uint16_t)cur[6]); // Should be 0x014d (THR10II-W) or 0x024d (THR30II-W)
        // 4d or 7a in conjunction with G10T??????
        // Serial.println(String(familyID, HEX));

        byte writeOrrequest = cur[7]; // 01 for request, 00 for write
        byte sameFrameCounter = cur[9];
        uint16_t payloadSize = (uint16_t)(cur[10] * 16 + cur[11] + 1u);
        byte blockCount = (byte)(payloadSize / 7); // e.g. 21/7 = 3 Blocks with 7 Bytes
        byte remainder = (byte)(payloadSize % 7);  // e.g. 20%7 = 6 Bytes
        byte msgbytes[payloadSize];
        size_t msgValsLength=payloadSize / 4 + (payloadSize % 4 != 0 ? 1 : 0);
        uint32_t msgVals[msgValsLength];

        //String res = "ParseSysEx: ";
        //for (int ii = 0; ii < cur_len; ii++) { res += (" " + String(cur[ii], HEX)); }
        //Serial.println(res);

        // cur[5], cur[6]
//        if( familyID == (M_NR << 8 ) + 0x4d ) // Valid Message received (0x014d or 0x024d)
// NOTE: familyID is not needed since we check if cur[5] == cur[5]! Therefore, check only if cur[6] == 0x4d
        if( cur[6] == 0x4d ) // Valid Message received (0x014d or 0x024d), NOTE: 4d or 7a in conjunction with G10T?
        {
            // Any valid incoming message proofs, that THRII MIDI-Interface activated!
            MIDI_Activated = true;
            TRACE_THR30IIPEDAL(result += "\n\rMIDI activated\n\r";)
            // Unpack Bitbucket encoding
            for( int i = 0; i <= blockCount; i++ )
            {
                if( 12 + 8 * i >= cur_len )
                {
                    // Just to set a breakpoint for debugging
                }
                byte bitbucket = cur[12 + 8 * i];

                int last = (i == blockCount) ? remainder : 7;

                for( int j = 0; j < last; j++ )
                {
                    if( 13 + 8 * i + j >= cur_len )
                    {
                        // Just to set a breakpoint for debugging
                    }
                    byte v1 = cur[13 + 8 * i + j]; // Fetch next byte from message
                    byte bucketmask = (byte)(1 << (6 - j));
                    byte res = (byte)(bitbucket & bucketmask);

                    msgbytes[j + 7 * i] = res != 0 ? (byte)(v1 | 0x80) : v1;
                }
            }

            // Split message into 4-Byte valuesSeconds
            for( int i = 0; i < payloadSize; i += 4 )
            {
                if( payloadSize > i + 3 ) // At least 4 Bytes remaining
                {
                    msgVals[i / 4] = *((uint32_t*) (msgbytes+i)); // Read out 4-Byte-Value as an int
                }
                else // No complete 4-Byte-value left in block
                {    // Construct a single value from remaining bytes (combine 1, 2, 3 bytes to one value)
                    msgVals[i / 4] = ( (uint32_t)msgbytes[i] << 24 )
                                   + ((i + 1) < payloadSize ? msgbytes[i + 1] << 16 : 0x00u)
                                   + ((i + 2) < payloadSize ? msgbytes[i + 2] <<  8 : 0x00u);
                }
            }
        } // cur[6] == 0x4d
        else // Frame begins like a normal THR30II-Answer but not the normal 02 4d follows (should be the 2nd answer to universal inquiry)
        {   // cur[5], cur[6]
//            if( familyID == (M_NR << 8 ) + 0x7E ) // (0x017E or 0x027E): 01 7e + (7f 06 02) : not a family ID but the 2nd answer frame to identity request
// NOTE: familyID is not needed since we check if cur[5] == cur[5]! Therefore, check only if cur[6] == 0x7E
            if( cur[6] == 0x7E ) // (0x017E or 0x027E): 01 7e + (7f 06 02) : not a family ID but the 2nd answer frame to identity request
            {
                if( cur[7] == 0x7F && cur[8] == 0x06 && cur[9] == 0x02 ) // Is it really the 2nd Answer to univ. identity request?
                {
                    String txt = "";
                    int i = 9; // First pos is cur[10]
                    while( i++ < 100 ) // Copy Identity string part 1
                    {
                        if( cur[i] == 0 ) { break; }
                        txt += ((char)cur[i]);
                    }
                    txt += ("\r\n\t");
                    while( i++ < 100 ) // Copy Identity string part 2
                    {
                        if( cur[i] == 0 ) { break; }
                        txt += ((char)cur[i]);
                    }
                    TRACE_THR30IIPEDAL(result += " : " + txt;)

                    outqueue.getHeadPtr()->_answered = true; // Mark question as answered
                }
            } // cur[6] == 0x7E
            payloadSize = 0xffff; // Marker for unknown msg-Type
        } // End of "not a normal THR30II-message but perhaps response to univ. inquiry)

        if( (payloadSize <= 24) && dumpInProgress )  // A normal length frame can't belong to a dump (?) => finish it, if running
        {                                            // not sure about this! Does not work for 1.40.0a any more
                                                     // because last frame of symbol table dump is only 0x0c in length
            TRACE_THR30IIPEDAL(result += "\n\rFinished dump.\n\r";)
        }

        TRACE_THR30IIPEDAL(result += "PayloadSize: " + String(payloadSize) + "\r\n";)

        switch( payloadSize ) // Switch Message-Type by length of payload (perhaps not the best method?)
        {
            case 9: // Answer-Message
                TRACE_THR30IIPEDAL(result += " 9-Byte-Message";)
                // msgVals: The Message: [0]:Opcode, [1]:Length, [2]:Block-Key/TargetType
                if( msgVals[0] == 0x00000001 && msgVals[1] == 0x00000001 ) // Seems to be an answer
                {
                    switch( msgVals[2] ) // The 3rd Data-Value of the 9-Byte Message is just one byte in the SysEx-Data
                    {                    // but here it is packed to 4-Byte msgVal as the MSB!
                        case 0x00000000:
                        case 0x01000000:
                        {   
                            int id = 0;
                            if( outqueue.itemCount() > 0 )
                            {
                                id = outqueue.getHeadPtr()->_id;

                                if( id == 7 ) // Only react, if it was the question #7 from the boot-up dialog ("have user settings changed?")
                                {
                                    outqueue.getHeadPtr()->_answered = true;

                                    // Here we react depending on the answer: If settings have changed: We need the actual settings
                                    // and the five user presets
                                    // If settings have not changed (a User-Preset is active and not modified):
                                    // We only need the 5 User Presets and ask which one is active

                                    if( msgVals[2] != 0 ) // If settings have changed: We need the actual settings
                                    {
                                        userSettingsHaveChanged = true;
                                        activeUserSetting = -1; // So -no- User Preset is active right now
                                    }
                                    else // A unchanged User-Setting is active
                                    {
                                        userSettingsHaveChanged = false;
                                        // NOTE: We request the number of the active user setting later : #S10!
                                    }

                                    // #S8 Request actual user settings (Expect several frames - settings dump)
                                    // Answer will be the settings dump for actual settings
                                    byte tmp_s8 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x3c, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s8, 29), 8, false, true));

                                    // #S9 Header for a System Question
                                    // it is a header, no ack, no answer
                                    byte tmp_s9 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x04, 0x00, 0x00, 0x07, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s9, 29), 9, false, false));

                                    // #S10 System Question body: Opcode "0x00" number of  -active-  User-Setting
                                    // Answer will be the  n u m b e r  of the active user setting
                                    byte tmp_s10 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x05, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s10, 21), 10, false, true));

                                    // #S11 Request name of User-Setting #1
                                    // Answer will be the name of user-setting 1
                                    byte tmp_s11 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x03, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s11, 29), 11, false, true));

                                    // #S12 Request name of User-Setting #2
                                    // Answer will be the name of user-setting 2
                                    byte tmp_s12 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x04, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s12, 29), 12, false, true));

                                    // #S13 Request name of User-Setting #3
                                    // Answer will be the name of user-setting 3
                                    byte tmp_s13 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x05, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s13, 29), 13, false, true));

                                    // #S14 Request name of User-Setting #4
                                    // Answer will be the name of user-setting 4
                                    byte tmp_s14 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x06, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s14, 29), 14, false, true));

                                    // #S15 Request name of User-Setting #5
                                    // Answer will be the name of user-setting 5
                                    byte tmp_s15 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x07, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s15, 29), 15, false, true));

                                    // TODO: Request user settings 1..5 (?)

                                    // #S16 Header 0D for Syst.Read G10T
                                    byte tmp_s16 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x08, 0x00, 0x00, 0x07, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s16, 29), 16, false, false)); // Header!

                                    // #S17 Syst. Read G10T Value 0B (after Header 0D)
                                    byte tmp_s17 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x09, 0x00, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s17, 21), 17, false, true)); // Answer will be 01, 0c, 00, 02, 00=extracted / 02=plugged in

                                    // #S18 Header 0D for Syst.Read Front-LED state
                                    byte tmp_s18 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x2a, 0x00, 0x00, 0x07, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s18, 29), 18, false, false)); // Header!;

                                    // #S19 Syst. Read Front-LED Value 02 (after Header 0D)
                                    byte tmp_s19 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x2b, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s19, 21), 19, false, true));

                                    // #S20 Header 09 for Global Param Read, 8 Byte follow
                                    byte tmp_s20 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x06, 0x00, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s20, 29), 20, false, false)); // Header!

                                    // #S21 Global Param Read TunerEnable Value FFFFFFFF 0000014D (after Header 09)
                                    uint16_t key  = glob["TunerEnable"];
                                    byte tmp_s21 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x07, 0x00, 0x00, 0x07, 0x78, 0x7f, 0x7f, 0x7f, 0x7f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    tmp_s21[17] = (byte)(key % 256); // Insert key for "TunerEnable" into message data
                                    tmp_s21[18] = (byte)(key / 256);
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s21, 29), 21, false, true)); // Tuner enabled? Answer will be 01, 0c, 00, 03, 00=inactive/01=enabled

                                    // THR: 									    #R19
                                    // f0 00 01 0c 24 02 4d 00 05 00 01 03 00 01 00 00 00 0c 00 00 00 00 | 00 00 00 00 03 00 00 00 00 00 00 00 00 | 00 f7       //

                                    // #S22 Header 0D for Syst.Read Speaker-Tuner state
                                    byte tmp_s22 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x2c, 0x00, 0x00, 0x07, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s22, 29), 22, false, false)); // Header!

                                    // #S23 Syst. Read Speaker-Tuner Value 0e (after Header 0D)                                                0e=Speaker-Tuner
                                    byte tmp_s23 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x2d, 0x00, 0x00, 0x03, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s23, 21), 23, false, true)); // Answer will be 01, 0c, 00, 02, 00=open/01=focus;

                                    // Expect THR-reply:
                                    // 0000   f0 00 01 0c 24 02 4d 00 14 00 01 03 00 01 00 00   01 = Reply
                                    // 0010   00 0c 00 00 00 00|00 00 00 00 02 00 00 00 00 01   02 = type enum   01 = Focus
                                    // 0020   00 00 00|00 f7

                                    // #S24 Header 09 for Global Param Read, 8 Byte follow
                                    byte tmp_s24 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x06, 0x00, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s24, 29), 24, false, false)); // Header!;

                                    // #S25 Global Param Read GuitarVolume Value FFFFFFFF 00000155 (after Header 09)
                                    key = glob["GuitarVolume"];
                                    byte tmp_s25[29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x07, 0x00, 0x00, 0x07, 0x78, 0x7f, 0x7f, 0x7f, 0x7f, 0x55, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    tmp_s25[17] = (byte)(key % 256);
                                    tmp_s25[18] = (byte)(key / 256);
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s25, 29), 25, false, true)); // GuitarVolume? Answer will be 01, 0c, 00, 04, GuitarVolume

                                    // #S26 Header 09 for Global Param Read, 8 Byte follow
                                    byte tmp_s26 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x06, 0x00, 0x00, 0x07, 0x00, 0x09, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s26, 29), 26, false, false)); // Header!

                                    // #S27 Global Param Read AudioVolume Value FFFFFFFF 0000014B (after Header 09)
                                    key = glob["AudioVolume"];
                                    byte tmp_s27[29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x07, 0x00, 0x00, 0x07, 0x78, 0x7f, 0x7f, 0x7f, 0x7f, 0x4b,0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                    tmp_s27[17] = (byte)(key % 256);
                                    tmp_s27[18] = (byte)(key / 256);
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s27, 29), 27, false, true)); // AudioVolume? Answer will be 01, 0c, 00, 04, AudioVolume
                                } // of "only react, if it was the question #7 from the boot-up dialog ("have user settings changed?")"
                            } // if item_count in outqueue > 0
                            TRACE_THR30IIPEDAL(result += " Answer to 0F-00-Question #" + String(id) + ": User-Setting was " + String(msgVals[2] == 0 ? "not " : "") + "changed.";)
                        }
                        break;

                        default:
                            TRACE_THR30IIPEDAL(result += " -unknown value " + String(msgVals[2]) + ".";)
                        break;
                    } // of "switch msgVals[2]"
                } // of "if seems to be an answer"
            break; // of "message 9 bytes"

            case 12: // e.g. Acknowledge
                TRACE_THR30IIPEDAL(result += " 12-Byte-Message";)
                if( msgVals[0] == 0x0001 && msgVals[1] == 0x0004 )
                {
                    switch( msgVals[2] ) // Block-Key / TargetTypeKey
                    {
                        int id;
                        case 0x00000000ul: // This is an acknowledge message
                        {  
                            id = outqueue.getHeadPtr()->_id;
                            TRACE_THR30IIPEDAL(result += " Acknowledge for Message #" + String(id);)
                            outqueue.getHeadPtr()->_acknowledged = true;
                            // Function on_ack_reactions(uint16t id)

                            std::tuple<uint16_t, Outmessage, bool * , bool > tp;
                            
                            while( !on_ack_queue.isEmpty() ) // Are there "action packs" on the list?
                            {  
                                tp = *on_ack_queue.getHeadPtr();                                
                                if( std::get<0>(tp) == id ) // Do they belong to the last outmessage, that is acknowledged now?
                                {
                                    TRACE_THR30IIPEDAL(Serial.println("working o on_ack_queue, because upload of actual setting was acknowledged.");)

                                    if( std::get<1>(tp)._msg.getSize() != 0 ) // Is there a message to send out?
                                    {
                                      TRACE_THR30IIPEDAL(Serial.println("Sending out an outmessage from on_ack_queue.");)
                                      outqueue.enqueue(std::get<1>(tp));
                                    }

                                    if( std::get<2>(tp) != nullptr ) // Is there a flag to set?
                                    {
                                      TRACE_THR30IIPEDAL(Serial.println("Setting a flag from on_ack_queue.");)
                                      *std::get<2>(tp) = std::get<3>(tp); // Set the value of the flag
                                    }
                                    
                                    // Now read actual settings
                                    on_ack_queue.dequeue();    
                                }
                            }

                            if( id == 999 )
                            {
                                TRACE_THR30IIPEDAL(Serial.println(" Switch User Setting acknowledged. (999)");) 
                                // Now read actual settings

                                // #S8 Request actual user settings (Expect several frames - settings dump)
                                byte tmp_s8 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x3c, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s8, 29), 8, false, true)); // Answer will be the settings dump for actual settings
                            }
                        }
                        break;
                        
                        case 0x00000080ul: // Message with unknown meaning from Init-Dialog
                        {
                            id = outqueue.getHeadPtr()->_id;
                            outqueue.getHeadPtr()->_answered = true;
                            TRACE_THR30IIPEDAL(result += " Answer "+ String(msgVals[2], HEX) + " to 05-Msg #" + String(id) + " : ??";)
                        }
                        break;

                        case 0x01300063ul: // Very very old firmware version 1.30.0c
                        case 0x0131006Bul: // Very old firmware 1.31.0k
                        case 0x01400061ul: // Old firmware 1.40.0a
                        case 0x01420067ul: // Firmware 1.42.0g
                        case 0x01430062ul: // Firmware 1.43.0b
                        case 0x01440061ul: // Latest Firmware 1.44.0a
                            id = outqueue.getHeadPtr()->_id;
                            TRACE_THR30IIPEDAL(result += " Answer to 01-Msg #" + String(id) + ": Firmware-Version " + String(msgVals[2], HEX);)
                            Firmware = msgVals[2];

                            if( writeOrrequest == 1 && MIDI_Activated ) // Followed a "Req. Firmware" with the "01"-Marker
                            {   // Use the 2nd "Req. Firmware"
                                // The first one is used for calculating the correct magic key

                                if( id == 5 )
                                {
                                    outqueue.getHeadPtr()->_answered = true; // This request is answered
                                }

                                // Now ask for the symbol table
                                // Answer will be the symbol dump, we can not go on without it - await answer
                                // Question requires no following frame
                                TRACE_THR30IIPEDAL(result += "\n\rAsking for Symbol table:\n\r";)
                                byte tmp_s777 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s777, 29), 777, false, true));
                            }
                            else if( writeOrrequest == 0 ) // Followed a "Req. Firmware" with the "00"-Marker
                            {
                                if( id == 2 ) // Was the answer to 1st "ask firmware version" in boot-Up
                                {
                                    outqueue.getHeadPtr()->_answered = true; // This request is answered
                                }

                                // #S3 + #S4 invoke #R4 (seems always the same) "MIDI activate" (works only after ID_Req!)
                                // it is a header, so no ack and no answer
                                byte tmp_s3 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x01, 0x00, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s3, 29), 3, false, false));
                                
                                // The magic key is the CRC32 value of the symbol table, considering the byte endianess and Yamaha 'Bitbucketing'
                                // #S4  (at least continue to this frame, to activate THR30II MIDI-Interface )
                                if( Firmware == 0x01300063 ) // Firmware 1.30.0c
                                {
                                    // Firmware 1.30.0c Magic key: 60 6b 3e 6f 68
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x60, 0x6b, 0x3e, 0x6f, 0x68, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }
                                else if( Firmware == 0x0131006B ) // Firmware 1.31.1k
                                {
                                    // Firmware 1.31.0k Magic key: 28 24 6b 09 18
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x28, 0x24, 0x6b, 0x09, 0x18, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }
                                else if( Firmware == 0x01400061 ) // Firmware 1.40.0a
                                {
                                    // Firmware 1.40.0a Magic key: 10 5c 61 06 79
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x10, 0x5c, 0x61, 0x06, 0x79, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }
                                else if( Firmware == 0x01420067 ) // Firmware 1.42.0g
                                {
                                    // Firmware 1.42.0g Magic key: 28 72 4d 54 5d
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x28, 0x72, 0x4d, 0x54, 0x5d, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }
                                else if( Firmware == 0x01430062 ) // Firmware 1.43.0b
                                {
                                    // Firmware 1.43.0b (same magic key as 1.42.0.g): 28 72 4d 54 5d
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x28, 0x72, 0x4d, 0x54, 0x5d, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }
                                else if( Firmware == 0x01440061 ) // Firmware 1.44.0a
                                {
                                    // Firmware 1.44.0a (same magic key as 1.42.0.g and 1.43.0b): 28 72 4d 54 5d
                                    // Note: the 5 bytes are derived from the Symbol table CRC32 = 0x_DD_54_CD_72, considering the byte endianess and Yamaha 'Bitbucketing'
                                    // Send Midi activation and await ack
                                    byte tmp_s4 [21] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x02, 0x00, 0x00, 0x03, 0x28, 0x72, 0x4d, 0x54, 0x5d, 0x00, 0x00, 0x00, 0xf7};
                                    outqueue.enqueue(Outmessage(SysExMessage(tmp_s4, 21), 4, true, false));
                                }

                                // #S5 Request Firmware-Version(?) (Answ. always the same) like #S2, but with "01" in head
                                // Answer is firmware version (this is "1" -version of the question message)
                                byte tmp_s5 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s5, 29), 5, false, true));
                            }
                            break;

                        case (uint32_t)0xFFFFFFF9ul:
                            TRACE_THR30IIPEDAL(result += " New firmware FFFFFFF9-Not Acknowledge (-7 = wrong MIDI-activate-Code)";)
                            // What to do here ???
                            break;
                        case (uint32_t)0xFFFFFFFFul:
                            TRACE_THR30IIPEDAL(result += " Not Acknowledge (-1)";)
                            // What to do here ???
                            break;
                        default:
                            TRACE_THR30IIPEDAL(result += " -unknown " + String(msgVals[2]);)
                            break;
                    }
                }
            break; // of "message 12 Bytes"
            
            case 16: // AMP- and UNIT-Mode changes
                TRACE_THR30IIPEDAL(result += " 16-Byte-Message";)
                if( cur_len > 0x1a )
                {
                    // msgVals: The Message: [0]:Opcode, [1]:Length, [2]:Block-Key/TargetType
                    if( msgVals[0] == 0x00000003 && msgVals[1] == 0x00000008 )
                    {
                        if( msgVals[2] == THR30II_UNITS_VALS[CONTROL].key ) // == 0x010a  "Amp"
                        {
                            col_amp ca {CLASSIC, CLEAN};
                        
                            if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, CLEAN}] ) // 0x004A : "THR10C_Deluxe"
                            {
                                TRACE_THR30IIPEDAL(result += " CLEAN CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, CRUNCH}] ) // 0x0071 : "THR10C_Mini"
                            {
                                TRACE_THR30IIPEDAL(result += " CRUNCH BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, SPECIAL}] ) // 0x0078 : "THR10X_Brown1"
                            {
                                TRACE_THR30IIPEDAL(result += " SPECIAL CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, HI_GAIN}] ) // 0x007F : "THR10_Modern"
                            {
                                TRACE_THR30IIPEDAL(result += " HIGHGAIN CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, SPECIAL}] ) // 0x0084 : "THR10X_South"
                            {
                                TRACE_THR30IIPEDAL(result += " SPECIAL BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, CRUNCH}] ) // 0x0088 : "THR10C_DC30"
                            {
                                TRACE_THR30IIPEDAL(result += " CRUNCH CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, HI_GAIN}] ) // 0x008B : "THR10X_Brown2"
                            {
                                TRACE_THR30IIPEDAL(result += " HIGAIN MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, FLAT}] ) // 0x008F : "THR10_Flat"
                            {
                                TRACE_THR30IIPEDAL(result += " FLAT CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, ACO}] ) // 0x0093 : "THR10_Aco_Condenser1"
                            {
                                TRACE_THR30IIPEDAL(result += " ACOUSTIC CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, ACO}] ) // 0x0097 : "THR10_Aco_Dynamic1"
                            {
                                TRACE_THR30IIPEDAL(result += " ACOUSTIC MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, ACO}] ) // 0x0098 : "THR10_Aco_Tube1"
                            {
                                TRACE_THR30IIPEDAL(result += " ACOUSTIC BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, LEAD}] ) // 0x0099 : "THR10_Lead"
                            {
                                TRACE_THR30IIPEDAL(result += " LEAD CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, SPECIAL}] ) // 0x009C : "THR30_Stealth"
                            {
                                TRACE_THR30IIPEDAL(result += " SPECIAL MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, CRUNCH}] ) // 0x00A3 : "THR30_SR101"
                            {
                                TRACE_THR30IIPEDAL(result += " CRUNCH MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, LEAD}] ) // 0x00A8 : "THR30_Blondie"
                            {
                                TRACE_THR30IIPEDAL(result += " LEAD BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, BASS}] ) // 0x000AB :"THR10_Bass_Mesa"
                            {
                                TRACE_THR30IIPEDAL(result += " BASS-Model BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, HI_GAIN}] ) // 0x00AF : "THR30_FLead"
                            {
                                TRACE_THR30IIPEDAL(result += " HIGHGAIN BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, CLEAN}] ) // 0x00B2 : "THR30_Carmen"
                            {
                                TRACE_THR30IIPEDAL(result += " CLEAN MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, FLAT}] ) // 0x00B4 : "THR10_Flat_B"
                            {
                                TRACE_THR30IIPEDAL(result += " FLAT BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, FLAT}] ) // 0x00B5 : "THR10_Flat_V"
                            {
                                TRACE_THR30IIPEDAL(result += " FLAT MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{BOUTIQUE, CLEAN}] ) // 0x00B6 : "THR10C_BJunior2"
                            {
                                TRACE_THR30IIPEDAL(result += " CLEAN BOUTIQUE";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{CLASSIC, BASS}] ) // 0x00B7 : "THR10_Bass_Eden_Marcus"
                            {
                                TRACE_THR30IIPEDAL(result += " BASS-Model CLASSIC";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, BASS}] ) // 0x00B8 : "THR30_JKBass2"
                            {
                                TRACE_THR30IIPEDAL(result += " BASS-Model MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else if( msgVals[3] == THR30IIAmpKeys[col_amp{MODERN, LEAD}] ) // 0x00BB : "THR10_Brit"
                            {
                                TRACE_THR30IIPEDAL(result += " LEAD MODERN";)
                                ca = THR30IIAmpKey_ToColAmp((uint16_t)msgVals[3]);
                                SetColAmp(ca.c, ca.a);
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " " + String(msgVals[3]) + " in AMP-Message ";)
                            }
                            TRACE_THR30IIPEDAL(result += " " + String(msgVals[3]) + " in AMP-Message ";)
                            maskCUpdate |= maskAmpUnit;
                        } // of Block AMP
                        else if( msgVals[2] == THR30II_UNITS_VALS[REVERB].key ) // == 0x0112 Block Reverb ("FX4")
                        {
                            if( msgVals[3] == THR30II_REV_TYPES_VALS[SPRING].key ) // 0x00F1 : "StandardSpring"
                            {
                                TRACE_THR30IIPEDAL(result += " Reverb: Mode SPRING selected ";)
                                ReverbSelect(SPRING);
                                selected_sbox = 9;
                            }
                            else if( msgVals[3] == THR30II_REV_TYPES_VALS[HALL].key ) // 0x00FC : "ReallyLargeHall"
                            {
                                TRACE_THR30IIPEDAL(result += " Reverb: Mode HALL selected ";)
                                ReverbSelect(HALL);
                                selected_sbox = 11;
                            }
                            else if( msgVals[3] == THR30II_REV_TYPES_VALS[PLATE].key ) // 0x00F7 : "LargePlate1"
                            {
                                TRACE_THR30IIPEDAL(result += " Reverb: Mode PLATE selected ";)
                                ReverbSelect(PLATE);
                                selected_sbox = 10;
                            }
                            else if( msgVals[3] == THR30II_REV_TYPES_VALS[ROOM].key ) // 0x00FE : "SmallRoom1"
                            {
                                TRACE_THR30IIPEDAL(result += " Reverb: Mode Room selected ";)
                                ReverbSelect(ROOM);
                                selected_sbox = 12;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += String(cur[0x1a], HEX) + " Reverb: unknown Mode selected";)
                            }
                            maskCUpdate |= maskReverb;
                        } // of Block Reverb
                        else if ( msgVals[2] == THR30II_UNITS_VALS[EFFECT].key ) // == 0x010c Block Effect  ("FX2")
                        {
                            if( msgVals[3] == THR30II_EFF_TYPES_VALS[PHASER].key ) // 0x00D0 : "Phaser"
                            {
                                TRACE_THR30IIPEDAL(result += " Effect: Mode PHASER selected ";)
                                EffectSelect(PHASER);
                                selected_sbox = 5;
                            }
                            else if( msgVals[3] == THR30II_EFF_TYPES_VALS[TREMOLO].key ) // 0x00DC : "BiasTremolo"
                            {
                                TRACE_THR30IIPEDAL(result += " Effect: Mode TREMOLO selected ";)
                                EffectSelect(TREMOLO);
                                selected_sbox = 6;
                            }
                            else if( msgVals[3] == THR30II_EFF_TYPES_VALS[FLANGER].key ) // 0x00E2 : "L6Flanger"
                            {
                                TRACE_THR30IIPEDAL(result += " Effect: Mode FLANGER selected ";)
                                EffectSelect(FLANGER);
                                selected_sbox = 4;
                            }
                            else if( msgVals[3] == THR30II_EFF_TYPES_VALS[CHORUS].key ) // 0x00E6 : "StereoSquareChorus"
                            {
                                TRACE_THR30IIPEDAL(result += " Effect: Mode CHORUS selected ";)
                                EffectSelect(CHORUS);
                                selected_sbox = 3;
                            }
                            else 
                            {   
                                 TRACE_THR30IIPEDAL(result += " Effect: unknown Mode selected  0x" + String(msgVals[3], HEX);)
                            }
                            maskCUpdate |= maskFxUnit;
                        } // of Block Effect
                        else if( msgVals[2] == THR30II_UNITS_VALS[COMPRESSOR].key ) // == 0x0107 Block Compressor ("FX1")
                        {                                                           // Normally this should not occure
                            if( msgVals[3] == glob["RedComp"] )                     // because compressor can only be configured by the App
                            {
                                TRACE_THR30IIPEDAL(result += " Compressor: Mode RedComp selected ";)
                                // At the moment there is no other Compressor unit anyway!
                            }
                            else if( msgVals[3] == glob["Red Comp"] ) // There is one global key "RedComp" and another "Red Comp" with a space!
                            {
                                TRACE_THR30IIPEDAL(result += " Compressor: Mode Red Comp selected ";)
                                // At the moment there is no other Compressor unit anyway!
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " Compressor: unknown ";)
                            }
                            // maskCUpdate |= maskCompressor;
                            // selected_sbox = 0; // No need to select the compressor in the GUI (Edit mode)
                        }
                        else if( msgVals[2] == THR30II_UNITS_VALS[GATE].key ) // == 0x013C  Block "GuitarProc"
                        {                                                     // Normally this should not occure
                            if( msgVals[3] == glob["Gate"] )                  // because Noise Gate can only be configured by the App
                            {
                                TRACE_THR30IIPEDAL(result += " GuitarProc: Mode Gate selected ";)
                                // At the moment there is no other subunit anyway!
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " Gate: unknown ";)
                            }                                                                                              
                            // maskCUpdate |= maskNoiseGate;
                            // selected_sbox = 2; No need to select the noise gate in the GUI (Edit mode)

                        }  // Since firmware 1.40.0a Block Echo could eventually appear (Types TapeEcho/DigitalDelay)
                        else if(msgVals[2] == THR30II_UNITS_VALS[ECHO].key ) // 0x010F Block ECHO ("FX3")
                        {                                                    // NOTE: It seems not to be sent out by THR
                            if( msgVals[3] == glob["TapeEcho"] )
                            {
                                TRACE_THR30IIPEDAL(result += " UnitEcho: Mode TapeEcho selected ";)
                                //EchoSelect(TAPE_ECHO);
                                //selected_sbox = 7;
                            }
                            else if( msgVals[3] == glob["L6DigitalDelay"] )
                            {
                                TRACE_THR30IIPEDAL(result += " UnitEcho: Mode DigitalDelay selected ";)
                                //EchoSelect(DIGITAL_DELAY);
                                //selected_sbox = 8;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " Echo: unknown ";)
                            }
                            // maskCUpdate |= maskEcho;
                        }
                        else
                        {
                            TRACE_THR30IIPEDAL(result += " UNIT: unknown Block selected  0x" + String(msgVals[2], HEX);)
                        }
                    } // of OPCODE 0x03, Length=0x08
                    else
                    {
                        TRACE_THR30IIPEDAL(result += " OPCODE 3 but not expected LEN 8  0x" + String(msgVals[3], HEX);)
                    }
                }
            break; // of "message 16 Bytes"
            
            case 20: // Answers to System Mess-Questions and Status Messages
                TRACE_THR30IIPEDAL(result += " 20-Byte-Message ";)
                // msgVals: The Message: [0]:Opcode, [1]:Length, [2]:Block-Key/TargetType
                if( msgVals[0] == 1 && msgVals[1] == 0x0c ) // Answer to System-Question
                {
                    TRACE_THR30IIPEDAL(result += " (Answer) ";)
                    int id = outqueue.getHeadPtr()->_id;
                    switch( msgVals[3] )
                    {
                        case 0x0002:
                            TRACE_THR30IIPEDAL(result += " Answer to System Question (enum): 0x" + String(msgVals[4], HEX);)
                            if( outqueue.itemCount() > 0 )
                            {
                                if( id == 10 ) // Only react if it came from boot-up-question #S10 (number of active user setting)
                                {
                                    if( !userSettingsHaveChanged )
                                    {
                                        activeUserSetting = (int)msgVals[4];
                                    }
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += "\n\r Mark Question #" + String(id) + " (Nr. of active Preset) as answered.";)
                                }
                                else if( id == 17 ) // This reaction only if it came from boot-up-question #S17 (G10T-state)
                                {   // G10T Status Answer
                                    TRACE_THR30IIPEDAL(result += "(G10T) 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)

                                    if( msgVals[4] == 0x02 )
                                    {
                                        TRACE_THR30IIPEDAL(result += "Inserted. ";)
                                        // Perhaps add representation in GUI here
                                    }
                                    if( msgVals[4] == 0x00 )
                                    {
                                        TRACE_THR30IIPEDAL(result += "Extracted. ";)
                                        // Perhaps add representation in GUI here
                                    }
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += " Mark Question #" + String(id) + " (G10T-State) as answered.";)
                                }
                                else if( id == 19 ) // This reaction only if it came from boot-up-question #S19 (Front-LED-State)
                                {   // Front-LED Status Answer
                                    TRACE_THR30IIPEDAL(result += "(Front-LED) 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)

                                    if( msgVals[4] == 0x7f )
                                    {
                                        TRACE_THR30IIPEDAL(result += " is on.";)
                                        // Light = true;  // Perhaps add state var "Light" in class for GUI
                                    }
                                    if( msgVals[4] == 0x00 )
                                    {
                                        TRACE_THR30IIPEDAL(result += " is off.";)
                                        // Light = false; // Perhaps add state var "Light" in class for GUI
                                    }
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += " Mark Question #"+String(id)+" (Front-LED-State) as answered.";)
                                }
                                else if( id == 23 ) // This reaction only if it came from boot-up-question #S23 (Speaker-Tuning-State)
                                {   // Speaker-Tuner Status Answer
                                    TRACE_THR30IIPEDAL(result += "(Speaker-Tuning) 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)

                                    if( msgVals[4] == 0x01 )
                                    {
                                        TRACE_THR30IIPEDAL(result += " Focus.";)
                                        // SpeakerTuning = true;  // Perhaps add state var "SpeakerTuning" in class for GUI
                                    }
                                    if( msgVals[4] == 0x00 )
                                    {
                                        TRACE_THR30IIPEDAL(result += " Open.";)
                                        // SpeakerTuning = false; // Perhaps add state var "SpeakerTuning" in class for GUI
                                    }
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += " Mark Question #" + String(id) + " (Speaker-Tuning-State) as answered.";)
                                }
                            } // of outqueue item count > 0
                            break;

                        case 0x0003:
                            if( outqueue.item_count() > 0 )
                            {
                                int id = outqueue.getHeadPtr()->_id;
                                TRACE_THR30IIPEDAL(result += "(Tuner Enabled?) 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)
                                if( id == 21 ) // This reaction only if it came from boot-up-question #S21 (Glob.Read:TunerEnabled)
                                {              // Otherwise, we get a 24-Byte glob.param change report, no 20-Byte answer!
                                    // THR: 									    #R19
                                    // f0 00 01 0c 24 02 4d 00 05 00 01 03 00 01 00 00 00 0c 00 00 00 00 | 00 00 00 00 03 00 00 00 00 00 00 00 00 | 00 f7
                                    if( msgValsLength > 4 )
                                    {
                                        if( msgVals[4] != 0 )
                                        {
                                            // Perhaps show TunerState  "ON" in GUI here
                                        }
                                        else
                                        {
                                            // Perhaps show TunerState "OFF" in GUI here
                                        }
                                    }
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += "\n\rMark Question #" + String(id) + " (Tuner-State) as answered.";)
                                }
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += "\n\rAnswer to Global Read (bool): " + String((msgVals[4] != 0 ? "Yes" : "No") );)
                            }
                            break;

                        case 0x0004:
                            TRACE_THR30IIPEDAL(result += "\n\rAnswer to System Question (int): " + String(NumberToVal(msgVals[4]));)
                            if( outqueue.item_count() > 0 )
                            {
                                int id = outqueue.getHeadPtr()->_id;
                                double val;
                                if( id == 25 ) // Only react if it came from boot-up-question #S25 (GuitarVolume)
                                {   // GuitarVolume Answer
                                    TRACE_THR30IIPEDAL(result += " GUITAR-VOLUME ";)
                                    val = NumberToVal(msgVals[4]);
                                    TRACE_THR30IIPEDAL(result += String(val, 1);)
                                    // Perhaps show this in GUI
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += "\n\r Mark Question #" + String(id) + " (GuitarVolume) as answered.";)
                                }
                                else if( id == 27 ) // Only react if it came from boot-up-question #S27 (AudioVolume)
                                {   // AudioVolume Answer
                                    TRACE_THR30IIPEDAL(result += " AUDIO-VOLUME ";)
                                    val = NumberToVal(msgVals[4]);
                                    TRACE_THR30IIPEDAL(result += String(val, 1);)
                                    // Perhaps show this in GUI
                                    outqueue.getHeadPtr()->_answered = true;
                                    TRACE_THR30IIPEDAL(result += "\n\rMark Question #" + String(id) + " (AudioVolume) as answered.";)
                                }
                            }
                            break;

                        default:
                            TRACE_THR30IIPEDAL(result += "\n\rAnswer to System Question (unknown): 0x" + String(msgVals[4],HEX);)
                            break;
                    }
                }
                // msgVals: The Message: [0]:Opcode, [1]:Length, [2]:Block-Key/TargetType
                if( msgVals[0] == 6 && msgVals[1] == 0x0c ) // Status Message
                {
                    TRACE_THR30IIPEDAL(result += " (Status) ";)
                    if( msgVals[2] == 0x0001 ) // Normal for the "Ready-Message"
                    {
                        switch( msgVals[3] )
                        {
                            case 0x0002:
                                TRACE_THR30IIPEDAL(result += "(enum): " + String((msgVals[4] == 0x0001 ? "ready" : "unknown"));)
                                break;
                            default:
                                TRACE_THR30IIPEDAL(result += "(unknown type): 0x" + String(msgVals[4], HEX);)
                                break;
                        }
                    }
                    else if( msgVals[2] == 0x000B ) // G10T Status Message
                    {
                        TRACE_THR30IIPEDAL(result += "(G10T) 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)
                        if( msgVals[4] == 0x02 )
                        {
                            TRACE_THR30IIPEDAL(result += " Inserted. ";)
                        }
                        if( msgVals[4] == 0x00 )
                        {
                            TRACE_THR30IIPEDAL(result += " Extracted. ";)
                        }
                    }
                    else
                    {
                        TRACE_THR30IIPEDAL(result += "(unknown) 2:0x" + String(msgVals[2], HEX) + " 3:0x" + String(msgVals[3], HEX) + " 4:0x" + String(msgVals[4], HEX);)
                    }
                } // Status message
            break; // of message 20 Bytes
            
            case 24: // Message for Parameter changes

                if( cur_len >= 0x1a ) // 00 01 07 - messages have to be longer than 0x2c bytes (failure otherwise)
                {
                    TRACE_THR30IIPEDAL(result += " 24-Byte-Message ";) // Message for Parameter changes

                    double val = 0.0;
                    // msgVals: The Message: [0]:Opcode, [1]:Length, [2]:Block-Key/TargetType
                    if( msgVals[0] == 2 && msgVals[1] == 0x10 ) // User-Setting change report
                    {
                        if( msgVals[3] < 5 ) // User setting was changed on THR (pressed one of knobs "1"..."5")
                        {
                            TRACE_THR30IIPEDAL(result += "\n\rUSER Setting " + String(msgVals[3] + 1) + " is activated. Following values: 0x" + String(msgVals[4], HEX) + " and 0x" + String(msgVals[5], HEX);)
                            activeUserSetting = msgVals[3] > 4 ? -1 : (int)msgVals[3]; // if value would be ushort FFFF (actual setting) we have to cast to int -1
                            userSettingsHaveChanged = false;
                            thrSettings = false; // Important only during the first user setting activation (by pressing one of the user memory buttond on the amp)
                            // THR-Remote asks "have user settings changed?" in this case (why?) and requests actual settings, if they did not change.
                            // We request actual settings without asking!

                            // Request actual user settings (Expect several frames - settings dump)

                            // ZWE:->should be a different id than #8 here, because in this case the request does not come from settings dialogue

// DEBUG ASK FOR USER SETTINGS #4
// TODO                       byte tmp_s88 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
//                            outqueue.enqueue(Outmessage(SysExMessage(tmp_s88, 29), 88, false, true)); // Answer will be the settings dump for actual settings

                            byte tmp_s88 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x3c, 0x00, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x00, 0xf7};
                            outqueue.enqueue(Outmessage(SysExMessage(tmp_s88, 29), 88, false, true)); // Answer will be the settings dump for actual settings
                        }
                        else if( msgVals[3] == 0xFFFFFFFF ) // User setting was dumped to the PC (followed by a request)
                        {
                            TRACE_THR30IIPEDAL(result += "\n\rActual user setting was dumped to PC. Following values: 0x" + String(msgVals[4], HEX) + " and 0x" + String(msgVals[5], HEX);)
                            if( outqueue.itemCount() > 0 )
                            {
                                int id = outqueue.getHeadPtr()->_id;
                                if( id == 8 ) // if it was request #S8 from boot-up dialog ("request actual user settings")
                                {
                                    outqueue.getHeadPtr()->_answered = true;
                                    _uistate = UI_home_amp; // State "initial actual settings" reached
                                }
                                if( id == 88 ) // if it was request #88 from outside boot-up dialog ("request actual user settings")
                                {
                                    outqueue.getHeadPtr()->_answered = true;
                                    // Do not change _uistate, if settings dump is received outside init-sync
                                }
                            }
                        }
                        else
                        {
                            TRACE_THR30IIPEDAL(result += "\n\rSelected invalid USER Setting " + String(msgVals[3] + 1);)
                        }
                    }
                    else if( msgVals[0] == 4 && msgVals[1] == 0x10 ) // Parameter change report
                    {
                        TRACE_THR30IIPEDAL(result += "\n\rParameter change report: ";)
                        // Select from msgVals[2] // Unit
                        if( msgVals[2] == THR30II_UNITS_VALS[GATE].key ) // 0x013C : Block Mix/Gate (GuitarProc)
                        {
                            userSettingsHaveChanged = true;
                            activeUserSetting = -1;

                            // Select from msgVals[3] (the value - here the parameter key)
                            if( msgVals[3] == THR30II_CAB_COMMAND ) // 0x0105:
                            {
                                uint16_t cab = (NumberToVal(msgVals[5]) / 100.0f); // Values 0...16 come in as floats
                                TRACE_THR30IIPEDAL(result += " CAB: " + THR30II_CAB_NAMES[(THR30II_CAB) constrain(cab, 0x00, 0x10)];)
                                SetCab((THR30II_CAB)cab);
                                maskCUpdate |= maskAmpUnit;
                            }
                            else if( msgVals[3] == THR30II_INFO_PHAS[PH_MIX].sk ) // 0x010E:
                            {
                                TRACE_THR30IIPEDAL(result += " MIX (EFFECT) ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(THR30II_INFO_EFFECT[effecttype]["MIX"].sk, val);
                                maskCUpdate |= maskFxUnit;
                                selected_sbox = 3 + effecttype;
                            }
                            else if( msgVals[3] == THR30II_INFO_TAPE[TA_MIX].sk ) // 0x0111:
                            {
                                TRACE_THR30IIPEDAL(result += " MIX (ECHO) ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EchoSetting(THR30II_INFO_ECHO[echotype]["MIX"].sk, val);
                                maskCUpdate |= maskEcho;
                                selected_sbox = 7 + echotype;
                            }
                            else if( msgVals[3] == THR30II_INFO_SPRI[SP_MIX].sk ) // 0x0128:
                            {
                                TRACE_THR30IIPEDAL(result += (" MIX (REVERB) ");)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                ReverbSetting(THR30II_INFO_REVERB[reverbtype]["MIX"].sk, val);
                                maskCUpdate |= maskReverb;
                                selected_sbox = 9 + reverbtype;
                            }

                            else if( msgVals[3] == THR30II_UNIT_ON_OFF_COMMANDS[GATE] ) // 0x0102:
                            {
                                TRACE_THR30IIPEDAL(result += " UNIT GATE " + String((msgVals[5] != 0 ? "On" : "Off"));)
                                Switch_On_Off_Gate_Unit(msgVals[5] != 0);
                                if( _uistate != UI_edit )
                                {
                                  maskCUpdate |= maskNoiseGate;
                                  // selected_sbox = 2; // NOTE: Do not switch to the noise gate
                                  // NOTE: In Edit mode, when switching amps from THRII, the last sent parameter update from THRII is
                                  //       about the noise gate unit, however, the selected sbox is the Amp unit
                                }
                            }
                            else if( msgVals[3] == THR30II_UNIT_ON_OFF_COMMANDS[ECHO] ) // 0x012C:
                            {
                                TRACE_THR30IIPEDAL(result += " UNIT ECHO " + String((msgVals[5] != 0 ? "On" : "Off"));)
                                Switch_On_Off_Echo_Unit(msgVals[5] != 0);
                                maskCUpdate |= (maskEcho | maskEchoEn);
                                selected_sbox = 7 + echotype;
                            }
                            else if( msgVals[3] == THR30II_UNIT_ON_OFF_COMMANDS[EFFECT] ) // 0x012D:
                            {
                                TRACE_THR30IIPEDAL(result += " UNIT EFFECT " + String((msgVals[5] != 0 ? "On" : "Off"));)
                                Switch_On_Off_Effect_Unit(msgVals[5] != 0);
                                maskCUpdate |= (maskFxUnit | maskFxUnitEn);
                                selected_sbox = 3 + effecttype;
                            }
                            else if( msgVals[3] == THR30II_UNIT_ON_OFF_COMMANDS[COMPRESSOR] ) // 0x012E:
                            {
                                TRACE_THR30IIPEDAL(result += " UNIT COMPRESSOR " + String((msgVals[5] != 0 ? "On" : "Off"));)
                                Switch_On_Off_Compressor_Unit(msgVals[5] != 0);
                                maskCUpdate |= (maskCompressor | maskCompressorEn);
                                selected_sbox = 0;
                            }
                            else if( msgVals[3] == THR30II_UNIT_ON_OFF_COMMANDS[REVERB] ) // 0x0130:
                            {
                                TRACE_THR30IIPEDAL(result += " UNIT REVERB " + String((msgVals[5] != 0 ? "On" : "Off"));)
                                Switch_On_Off_Reverb_Unit(msgVals[5] != 0);
                                maskCUpdate |= (maskReverb | maskReverbEn);
                                selected_sbox = 9 + reverbtype;
                            }

                            else if( msgVals[3] == THR30II_GATE_VALS[GA_THRESHOLD] ) // 0x0069:
                            {
                                TRACE_THR30IIPEDAL(result += " GATE-THRESHOLD ";)
                                val = THR30II_Settings::NumberToVal_Threshold(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                GateSetting(GA_THRESHOLD, val);
                                if( _uistate != UI_edit ) // Do not select the noise gate in the GUI (Edit mode)!
                                {
                                    maskCUpdate |= maskNoiseGate;
                                }
                            }
                            else if( msgVals[3] == THR30II_GATE_VALS[GA_DECAY]) // 0x006B:
                            {
                                TRACE_THR30IIPEDAL(result += (" GATE-DECAY ");)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                GateSetting(GA_DECAY, val);
                                if( _uistate != UI_edit ) // Do not select the noise gate in the GUI (Edit mode)!
                                {
                                    maskCUpdate |= maskNoiseGate;
                                }
                            }
                            else
                            {      
                                TRACE_THR30IIPEDAL(result += "\n\runknown " + String(msgVals[3], HEX) + " in Block Mix/Gate";)
                            }
                        }
                        else if( msgVals[2] == THR30II_UNITS_VALS[REVERB].key ) // 0x0112: Block Reverb
                        {
                            userSettingsHaveChanged = true;
                            activeUserSetting = -1;
                            selected_sbox = 9 + reverbtype;

                            // Select from  msgVals[3]  (the value - here the parameter key)
                            if( msgVals[3] == THR30II_INFO_PLAT[PL_DECAY].sk ) // 0x006B:
                            {
                                TRACE_THR30IIPEDAL(result += " REVERB-DECAY ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                ReverbSetting(reverbtype, THR30II_INFO_PLAT[PL_DECAY].sk, val); // 0x006B, val);
                                maskCUpdate |= maskReverb;
                            }   
                            else if( msgVals[3] == THR30II_INFO_SPRI[SP_REVERB].sk ) // 0x00ED:
                            {
                                TRACE_THR30IIPEDAL(result += " REVERB-REVERB ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                ReverbSetting(reverbtype, THR30II_INFO_SPRI[SP_REVERB].sk, val); // 0x00ED, val);
                                maskCUpdate |= maskReverb;
                            }
                            else if( msgVals[3] == THR30II_INFO_PLAT[PL_TONE].sk ) // 0x00F4:
                            {
                                TRACE_THR30IIPEDAL(result += " REVERB-TONE ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                ReverbSetting(reverbtype, THR30II_INFO_PLAT[PL_TONE].sk, val); // 0x00F4, val);
                                maskCUpdate |= maskReverb;
                            }
                            else if( msgVals[3] == THR30II_INFO_PLAT[PL_PREDELAY].sk ) // 0x00FA:
                            {
                                TRACE_THR30IIPEDAL(result += " REVERB-PRE DELAY ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                ReverbSetting(reverbtype, THR30II_INFO_PLAT[PL_PREDELAY].sk, val ); // 0x00FA, val);
                                maskCUpdate |= maskReverb;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " REVERB Unknown: " + String(msgVals[5], HEX);)
                            }
                        } // of Block Reverb
                        else if( msgVals[2] == THR30II_UNITS_VALS[CONTROL].key ) // 0x010a: Block Amp
                        {
                            TRACE_THR30IIPEDAL(result += String("Amp: ");)
                            userSettingsHaveChanged = true;
                            activeUserSetting = -1;
                            selected_sbox = 1;
                            maskCUpdate |= maskAmpUnit;

                            // Select from msgVals[3] (the value - here the parameter key)
                            if( msgVals[3] == THR30II_CTRL_VALS[CTRL_GAIN] ) // 0x58:
                            {
                                TRACE_THR30IIPEDAL(result += (" GAIN ");)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                SetControl(THR30II_CTRL_SET::CTRL_GAIN, val);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                maskCUpdate |= maskGainMaster;
                            }
                            else if( msgVals[3] == THR30II_CTRL_VALS[CTRL_MASTER] ) // 0x4C:
                            {
                                TRACE_THR30IIPEDAL(result += " MASTER ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                SetControl(CTRL_MASTER, val);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                maskCUpdate |= maskGainMaster;
                            }
                            else if( msgVals[3] == THR30II_CTRL_VALS[CTRL_BASS] ) // 0x54:
                            {
                                TRACE_THR30IIPEDAL(result += " TONE-BASS ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                SetControl(CTRL_BASS, val);
                                maskCUpdate |= maskGainMaster;
                            }
                            else if( msgVals[3] == THR30II_CTRL_VALS[CTRL_MID] ) // 0x56:
                            {
                                TRACE_THR30IIPEDAL(result += (" TONE-MID ");)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                SetControl(CTRL_MID, val);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                maskCUpdate |= maskGainMaster;
                            }
                            else if( msgVals[3] == THR30II_CTRL_VALS[CTRL_TREBLE] ) // 0x57:
                            {
                                TRACE_THR30IIPEDAL(result += " TONE-TREBLE ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                SetControl(CTRL_TREBLE, val);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                maskCUpdate |= maskGainMaster;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " AMP Unknown " + String(msgVals[5], HEX);)
                            }
                        } // of Block Amp
                        else if( msgVals[2] == THR30II_UNITS_VALS[EFFECT].key ) // 0x010c: Block Effect
                        {
                            userSettingsHaveChanged = true;
                            activeUserSetting = -1;
                            selected_sbox = 3 + effecttype;

                            // Select from msgVals[3] (the value - here the parameter key)
                            if( msgVals[3] == THR30II_INFO_EFFECT[PHASER]["SPEED"].sk ) // 0x00D4:
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT SPEED (Phaser/Tremolo) ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(effecttype, msgVals[3], val);
                                maskCUpdate |= maskFxUnit;
                            }
                            else if( msgVals[3] == THR30II_INFO_EFFECT[PHASER]["FEEDBACK"].sk ) // 0x00D6:
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT FEEDBACK ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(effecttype, msgVals[3], val);
                                maskCUpdate |= maskFxUnit;
                            }
                            else if( msgVals[3] == THR30II_INFO_EFFECT[CHORUS]["DEPTH"].sk ) // 0x00E0:
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT DEPTH (Flanger/Chorus/Tremolo) ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(effecttype, msgVals[3], val);
                                maskCUpdate |= maskFxUnit;
                            }
                            else if( msgVals[3] == THR30II_INFO_EFFECT[FLANGER]["SPEED"].sk ) // 0x00E4:
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT SPEED (Flanger / Chorus) ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(effecttype, msgVals[3], val);
                                maskCUpdate |= maskFxUnit;
                            }
                            else if( msgVals[3] == THR30II_INFO_EFFECT[CHORUS]["PREDELAY"].sk ) // 0x00E8:
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT PRE-DELAY ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EffectSetting(effecttype, msgVals[3], val);
                                maskCUpdate |= maskFxUnit;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " EFFECT Unknown: " + String(msgVals[3], HEX);)
                            }
                        } // of Block Effect
                        else if( msgVals[2] == THR30II_UNITS_VALS[ECHO].key ) // 0x010f: Block Echo
                        {
                            userSettingsHaveChanged = true;
                            activeUserSetting = -1;
                            selected_sbox = 7 + echotype;
                            
                            // Select from msgVals[3] (the value - here the parameter key)
                            if( msgVals[3] == THR30II_INFO_ECHO[TAPE_ECHO]["BASS"].sk ) // 0x0054:
                            {
                                TRACE_THR30IIPEDAL(result += " ECHO-BASS ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EchoSetting(echotype, msgVals[3], val);
                                maskCUpdate |= maskEcho;
                            }
                            else if( msgVals[3] == THR30II_INFO_ECHO[TAPE_ECHO]["TREBLE"].sk ) // 0x0057:
                            {
                                TRACE_THR30IIPEDAL(result += " ECHO-TREBLE ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EchoSetting(echotype, msgVals[3], val);
                                maskCUpdate |= maskEcho;
                            }
                            else if( msgVals[3] == THR30II_INFO_ECHO[TAPE_ECHO]["FEEDBACK"].sk ) // 0x00D6:
                            {
                                TRACE_THR30IIPEDAL(result += " Echo FEEDBACK ";)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EchoSetting(echotype, msgVals[3], val);
                                maskCUpdate |= maskEcho;
                            }
                            else if( msgVals[3] == THR30II_INFO_ECHO[TAPE_ECHO]["TIME"].sk ) // 0x00ED:
                            {
                                TRACE_THR30IIPEDAL(result += (" ECHO-TIME ");)
                                val = THR30II_Settings::NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                EchoSetting(echotype, msgVals[3], val);
                                maskCUpdate |= maskEcho;
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " ECHO Unknown: " + String(msgVals[5], HEX);)
                            }
                            
                        } // of "Block Echo"
                        else if( msgVals[2]== 0xFFFFFFFF ) // Global Parameter Settings
                        {
                            // Select from msgVals[3] (the value - here the Extended setting's key)
                            if( msgVals[3] == glob["GuitInputGain"] ) // 0x0147:
                            {
                                TRACE_THR30IIPEDAL(result += (" Guitar Input Gain ");)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += (String(val, 0));)
                                if( val > 99.0 )
                                {
                                    // Perhaps show this in GUI "GuitarMute Off"
                                }
                                else
                                {
                                    // Perhaps show this in GUI "GuitarMute On"
                                }
                            }
                            else if(msgVals[3] == glob["TunerEnable"] )
                            {
                                TRACE_THR30IIPEDAL(result += (" Tuner Enable ");)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += (String(val,0));)
                                if(val>99.0)
                                {
                                    //Perhaps show this in GUI "Tuner On"
                                }
                                else
                                {
                                    //Perhaps show this in GUI "Tuner Off"
                                }
                            }
                            else if( msgVals[3] == glob["GuitarVolume"] ) // 0x0155:
                            {
                                TRACE_THR30IIPEDAL(result += " Guitar Volume ";)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += (String(val, 0));)
                                SetGuitarVol((uint16_t)val); // Show this in GUI
                                maskCUpdate |= maskVolumeAudio;
                            }
                            else if( msgVals[3] == glob["AudioVolume"] )
                            {
                                TRACE_THR30IIPEDAL(result += " Audio Volume ";)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)

                                if( _uistate == UI_edit )
                                {
                                  setParbyAudioVolKnob(val); // NOTE: Very important in Edit mode
                                }
                                else
                                {
                                  SetAudioVol((uint16_t)val); // Show this in GUI
                                  maskCUpdate |= maskVolumeAudio;
                                }
                            }
                            else if( msgVals[3] == glob["GuitProcInputGain"] )
                            {
                                TRACE_THR30IIPEDAL(result += " GuitProcInputGain ";)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                // Perhaps show this in GUI
                            }
                            else if( msgVals[3] == glob["GuitProcOutputGain"] )
                            {
                                TRACE_THR30IIPEDAL(result += " GuitProcOutputGain ";)
                                val = NumberToVal(msgVals[5]);
                                TRACE_THR30IIPEDAL(result += String(val, 0);)
                                // Perhaps show this in GUI
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " Global Parameter Settings Unknown: 0x" + String(msgVals[3], HEX) + " 0x" + String(msgVals[5], HEX);)
                            }
                        }
                    } // of if 0x04 0x10 - message
                } // of if cur_len >= 0x1a
            break; // of message 24 Bytes

            default: // Message length other than 9, 12, 16, 20, 24 bytes (dumps)
                if( memcmp(cur, THR30II_IDENTIFY, 10) == 0 ) // Is it the identification string? (not a regular THR-frame)
                {
                    // We reacted to this this already above!!!
                }
                else // Not the identification string
                {
                    if( payloadSize <= 0x100 ) // Frame len other than 9, 12, 16, 20, 24 Bytes but shorter than 0x100 Bytes (will be a dump)
                    {
                        if( sameFrameCounter == 0 ) // Should be the first frame of the dump in this case
                        {
                            TRACE_THR30IIPEDAL(result += "\n\rChunk 1 of a dump : Size of first chunk's payload= " + String(payloadSize - 1, HEX);)
                            // int id = outqueue.getHeadPtr()->_id;

                            if( msgVals[0] == 0x01 && msgVals[2] == 0x00 && msgVals[3] == 0x00 && msgVals[4] == 0x01 ) // Patch dump
                            {
                                TRACE_THR30IIPEDAL(result += ".. it is a Patch-Dump. ";)
                                patchdump = true;
                                symboldump = false;
                                dumpInProgress = true;
                                dumpFrameNumber = 0;
                                dumpByteCount = 0;

                                dumplen = msgVals[1]; // Total lenght (including the 4 32-Bit-values 0 0 1 0)
                                TRACE_THR30IIPEDAL(result += "\n\rTotal patch length: " + String(dumplen);)
                                dump = new byte[dumplen - 16];
                                dump_len = dumplen - 16; // Netto patch lenght without the 4 leading 32-Bit-values 0 0 1 0
                               
                                if( dump == nullptr )
                                {
                                    TRACE_THR30IIPEDAL(Serial.println("\n\rAllocation Error for patch dump!");)
                                }

                                if( dumplen <= payloadSize - 8 ) // Complete patch fits in this one message
                                {
                                    dumpInProgress = false; // Because first frame is last frame
                                    patchdump = false;
                                    // Perhaps this (very short) patch dump was requested, if an outmessage is pending unanswered then release it
                                    int id = -1;
                                    if( outqueue.item_count() > 0 && outqueue.getHeadPtr()->_needs_answer )
                                    {
                                       id = outqueue.getHeadPtr()->_id;
                                       outqueue.getHeadPtr()->_answered = true;
                                    }
                                    TRACE_THR30IIPEDAL(result += "\n\rDump finished with Chunk 1. Request #" + String(id) + " is answered";)
                                }
                            }
                            else if( msgVals[0] == 0x01 && msgVals[2] == 0x00 ) // Patch name dump
                            {
                                TRACE_THR30IIPEDAL(result += " .. it is a Patch-Name-Dump. ";)
                                dumpInProgress = false; // Patch name dump always fits to one frame!
                                patchdump = false;
                                symboldump = false;
                                dumpFrameNumber = 0;
                                dumpByteCount = 0;
                                dumplen = msgVals[1];
                                TRACE_THR30IIPEDAL(result += " Total patch length: " + String(dumplen);)
                                dump = nullptr;
                                // No need to reserve a dump buffer "dump[]" , because we get all data instantly

                                uint32_t len = msgVals[3];
                                
                                if( dumplen >= len + 8 ) // Must be true for valid patchname message
                                {
                                    char bu[65];
                                    memcpy(bu, msgbytes + 16, len);
                                    String patchname = String(bu);
                                    TRACE_THR30IIPEDAL(result += String(" Answer: Patch name is \"") + patchname + String("\" ");)
                                    
                                    // Perhaps this one frame patch-name dump was requested; If a outmessage is pending unanswered then release it
                                    int id = -1;
                                    byte pnr = 0; // User-Preset number to which this name belongs
                                    if( outqueue.item_count() > 0 && outqueue.getHeadPtr()->_needs_answer )
                                    {
                                        id = outqueue.getHeadPtr()->_id;
                                        outqueue.getHeadPtr()->_answered = true;
                                        pnr = outqueue.getHeadPtr()->_msg.getData()[0x16]; // Fetch user preset nr.
                                        TRACE_THR30IIPEDAL(result += "\n\rpnr (raw)= " + String(pnr) + String("\n\r");)
                                        if( pnr > 4 ) // For "actual" byte [0x16] will be 0x7F
                                        {
                                            pnr = 0; // 0 is "actual"
                                        }
                                        else
                                        {
                                            pnr++;
                                        }
                                        TRACE_THR30IIPEDAL(result += " Dump finished with Chunk 1.\n\rRequest #" + String(id) + " is answered.\n\r";)
                                    }
                                    SetPatchName(patchname, (int)pnr - 1 );
                                }
                            }
                            else if((msgVals[0] == 0x01) && (msgVals[2] != 0x00) && (writeOrrequest == 0x00)) // Symbol table
                            {
                                TRACE_THR30IIPEDAL(result += " It is a symbol table dump. \r\n";)
                                patchdump = false;
                                symboldump = true;
                                dumpInProgress = true;
                                dumpFrameNumber = 0;
                                dumpByteCount = 0;
                                dumplen = msgVals[1]; // Length of patch in bytes
                                uint32_t symCount = msgVals[2]; // Number of sybols in this table
                                uint32_t len = msgVals[3]; // In case of the symbol table it should be the same as dumplen
                                TRACE_THR30IIPEDAL(result += String(symCount) + " symbols in a total patch length of " + String(dumplen) + String(" bytes.\r\n");)

                                dump = new byte[dumplen];
                                dump_len = dumplen;
                                if( dump == nullptr )
                                {
                                    TRACE_THR30IIPEDAL(Serial.println("\n\rAllocation Error for symbol dump!");)
                                }

                                if((dumplen == len) && (dumplen > 0xFF)) // A symbol table dump is very long!
                                {
                                    TRACE_THR30IIPEDAL(result += " ...seems to be valid\r\n";)
                                }
                            }
                            else
                            {
                                TRACE_THR30IIPEDAL(result += " dump has different values in header than expected!! " + String(msgVals[0], HEX) + String(msgVals[2], HEX) + String(msgVals[3], HEX) + String(msgVals[4], HEX);)
                            }

                        } // of "if first frame of a dump" (sameframeCounter == 0)
                        else if( dumpInProgress && (sameFrameCounter == dumpFrameNumber + 1) ) // Found next frame of a dump
                        {
                            dumpFrameNumber++;
                            TRACE_THR30IIPEDAL(result += " Chunk " + String(dumpFrameNumber + 1) + " of a dump: Size of this chunk's payload = " + String(payloadSize - 1, HEX);)
                        }
                        else
                        {
                          TRACE_THR30IIPEDAL(Serial.println("\n\rERROR 1: Should not happen during dump!");)
                        }

                        // For any dump type (except patch-name) and for any of it's chunks we have to copy the content
                        if( dumpInProgress && (dump != nullptr) )
                        {
                            uint16_t i = 0;
                            if( dumpFrameNumber == 0 )
                            {
                                if( patchdump )
                                {
                                    i = 6 * 4; // Skip 6 values in first frame
                                }
                                else if( symboldump )
                                {
                                    i = 2 * 4; // Skip 4 values in first frame
                                }
                            }
                            
                            for(; i < payloadSize; i++) // msgbytes.Length
                            {
                                dump[dumpByteCount++] = msgbytes[i];
                            }
                        }

                        if( dumpInProgress && (dumpByteCount >= dump_len) ) // Fetched enough bytes for complete dump
                        {
                            TRACE_THR30IIPEDAL(result += " Dump finished in chunk " + String(dumpFrameNumber + 1);)

                            if( patchdump )
                            {
                                TRACE_THR30IIPEDAL(result += "\n\rDoing Patch Set All:\n\r";)
                                _state = States::St_idle;
                                // Set constans from symbol table
                                patch_setAll(dump, dump_len); // Using dump_len, that does not include the 4  32-Bit-values 0 0 1 0
                                userPresetDownloaded = true;  // Flag that this settings have to be copied to the local array of 5 user presets
                            }
                            else if( symboldump )
                            {
                                // byte test[98]={
                                //              0x03,0x00,0x00,0x00, 
                                //              0x62,0x00,0x00,0x00,
                                //              0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
                                //              0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
                                //              0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41,
                                //              0x50,0x4F,0x75,0x74,0x70,0x75,0x74,0x4C,0x69,0x6D,0x69,0x74,0x65,0x72,0x54,0x75,0x62,0x65,0x47,0x61,0x69,0x6E,0x00,
                                //              0x57,0x69,0x64,0x65,0x53,0x74,0x65,0x72,0x65,0x6F,0x57,0x69,0x64,0x74,0x68,0x00,
                                //              0x47,0x75,0x69,0x74,0x61,0x72,0x44,0x49,0x45,0x6E,0x61,0x62,0x6C,0x65,0x00
                                //              };
                                Constants::set_all(dump); // Now we can get the right keys for this firmware from the table
                                
                                Init_Dictionaries(); // Now use constants in this App's dictionaries
                                
                                TRACE_THR30IIPEDAL(Serial.println("\n\rDictionaries initialized:");)

                                // CRC32 calculation over the symbol table, used to calculate the 'Magic Key' =================================================
                                uint32_t checksum = CRC32::calculate(dump, dumplen);

                                std::array<byte, 4>  raw_crc32 = {}; // 4 bytes CRC2
		                            std::array<byte, 5>  magic_key = {}; // 5 bytes Yamaha 'Bitbucketed' CRC32
                                
                                raw_crc32[0] = (byte) (checksum & 0xFF);
                                raw_crc32[1] = (byte)((checksum & 0xFF00)     >>  8);
                                raw_crc32[2] = (byte)((checksum & 0xFF0000)   >> 16);
                                raw_crc32[3] = (byte)((checksum & 0xFF000000) >> 24);

                                Enbucket(magic_key, raw_crc32, raw_crc32.end());

                                TRACE_THR30IIPEDAL(Serial.println("Symbol table CRC32 = 0x" + String(checksum, HEX) + " *******************************************************");)
                                TRACE_THR30IIPEDAL(Serial.println("0x" + String(magic_key[0], HEX) + " 0x" + String(magic_key[1], HEX) + " 0x" + String(magic_key[2], HEX) + " 0x" + String(magic_key[3], HEX) + " 0x" + String(magic_key[4], HEX));)
                                // ============================================================================================================================

                                int id = 0;
                                if( outqueue.itemCount() > 0 )
                                {
                                    id = outqueue.getHeadPtr()->_id;
                                    if( id == 777 ) // if this was the answer to the "Request symbol table from the boot up"
                                    {
                                        outqueue.getHeadPtr()->_answered = true; // This request is now answered
                                        TRACE_THR30IIPEDAL(result += " Request 777 answered.";)
                                    }
                                }

                                // #S6 (05-Message "01"-version") Request unknown reason (Answ. always the same: expext 0x00000080)
                                // Answer will be a number (seems to be always 0x80)
                                byte tmp_s6 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x01, 0x01, 0x00, 0x00, 0x07, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s6, 29), 6, false, true));

                                // #S7 (0F-Message) Ask: Have User-Settings changed?
                                // Answer will we changed (1) or not changed(0)
                                // After that we should react dependent of the answer: If settings have changed: We need the actual settings
                                // and the five user presets
                                // If settings have not changed (a User-Preset is active and not modified):
                                // We only need the 5 User Presets
                                byte tmp_s7 [29] = {0xf0, 0x00, 0x01, 0x0c, F_ID, M_NR, 0x4d, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
                                outqueue.enqueue(Outmessage(SysExMessage(tmp_s7, 29),  7, false, true));
                            } //end of "is SymbolDump"
                            
                            dumpInProgress = false;  
                            patchdump = false;
                            symboldump = false;

                            delete[] dump;

                        } // of "fetched enough bytes for complete dump"
                    } // of "if it seems to be a dump (  24 < len < 0x100  )
                    else // No dump
                    {
                        TRACE_THR30IIPEDAL(Serial.println("\n\rERROR: Should not happen during dump!");)
                        dumpInProgress = false;
                        patchdump = false;
                        symboldump = false;
                        delete[] dump;
                        dumpFrameNumber = 0;
                        TRACE_THR30IIPEDAL(result += " Unknown message payload size 0x" + String(payloadSize - 1, HEX) + "\r\n";)
                    }
                } // Not the identification string (but a dump or unknown long message)
                
            break; // of "message other than 9, 12, 16, 20 or 24 bytes (e.g. a patch)"
        } // Switch message payload size

    } // Regular THR30II - SysEx beginning with 01 0c 24
    else
    {
        if( memcmp(cur, THR30II_RESP_TO_UNIV_SYSEX_REQ, 9) == 0 ) // 0xF0, 0x7E, 0x7F, 0x06, 0x02, 0x00, 0x01, 0x0c, 0x24 (+, 0x00, 0x02, 0x00, 0x63, 0x00, 0x1E, 0x01, 0xF7)
        {
            uint16_t famID = 0;
            if( cur_len > 9 ) { famID = (cur[8] + 256 * cur[9]); }
            uint16_t modNr = 0;
            if( cur_len > 11 ) { modNr=cur[10] + 256 * cur[11]; }
            ConnectedModel = (famID << 16) + modNr;
            TRACE_THR30IIPEDAL(result += " Reply to Universal SysEx-Request: Family-ID " + String(famID, HEX) + ", Model-Nr: " + String(modNr, HEX);)
            outqueue.getHeadPtr()->_acknowledged = true;
        }
        else
        {
            TRACE_THR30IIPEDAL(result += " Other SysEx: {";)
            for(int i=0; i<cur_len; i++)
            {
                TRACE_THR30IIPEDAL(result += cur[i] < 16 ? String(0) + String(cur[i], HEX) : String(cur[i], HEX) + String(" ");)
            }
            TRACE_THR30IIPEDAL(result += "}";)
        }
    }

    sendChangestoTHR = true; // End of section, where changes are not send (back) to THR
    
    return result;
} // Parse SysEx (THR30II)

/////////////////////////////////////////////////////////////
// Convert the 32-Bit parameter value to 0..100 Slider value
/////////////////////////////////////////////////////////////
double THR30II_Settings::NumberToVal(uint32_t num, bool cut) // bool cut = true  (set default argument value)
{
    float val;
    
    if( num >= 998277251 ) // = 0.00392156979069, cut hard to 0 if too small
    {
        memcpy(&val, &num, 4);
        val = 100.0 * val;
    }
    else
    {
        val = 0.0;
    }
    return cut ? floor(val) : val; // Cut decimals if requested (default)
}

///////////////////////////////////////////////////////////////
// Convert the 32-Bit parameter value to a 0..100 Slider value
///////////////////////////////////////////////////////////////
double THR30II_Settings::NumberToVal_Threshold(uint32_t num, bool cut) // bool cut = true  (set default argument value)
{  
    float val;
    //0xC2C00000 read as float -> -96
    //0xBE500000 read as float -> -0.203125
    if( num >= 0xBE500000 && num < 0xC2C00000 )
    {
        //val= *((float*) &num);
        memcpy(&val, &num, 4);
        val = 100.0 + 100.0/96 * val; // Change range from [-96dB ... 0dB] to [0...100]
    }
    else if( num >= 0xC2C00000 ) { val =   0.0; }
    else                         { val = 100.0; }

    return cut ? floor(val) : val; // Cut decimals if requested (default)
}

uint32_t THR30II_Settings::ValToNumber(double val) // Convert a 0..100 Slider value to a 32Bit parameter value 
{
    uint32_t number;
    float v= val/(double)100.0;

    memcpy(&number, &v, 4);
            
    if( number < 999277251 )
    {
        number = 0u;
    }
    return number;
}

uint32_t THR30II_Settings::ValToNumber_Threshold(double val) //convert a 0..100 Slider value to a 32Bit parameter value (for internal -96dB to 0dB range)
{   
    uint32_t number;
    // val 0   -> -96dB
    // val 100 ->   0dB
    // -96       float read as uint32_t -> 0xC2C00000
    // -0.203125 float read as uint32_t -> 0xBE500000
    
    float v = -(100.0-val) * 96.0 / 100.0 + 0.4; // Change range from [0...100] to [-96dB ... 0dB]

    memcpy(&number, &v, 4);

    // Limit number to allowed range
    if( number < 0xBE500000 ) { number = 0u; }
    if( number > 0xC2C00000 ) { number = 0xC2C00000; }

    return number;
}

// TODO: Move it in another file?
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Use the audio volume knob on the THRII to set parameter values in Edit mode
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stompBoxes.h"
extern std::vector <StompBox*> sboxes;

// Call this function to actualy send changed parameters to the amp (a couple of times per second)
// sendEditToTHR must be set to true
void THR30II_Settings::send_to_thrii()
{
  if( selected_sbox_glob == 0 )
  {
    CompressorSetting((THR30II_COMP)idx_par_glob, val_glob); // Set (again) and send
  }
  else if( selected_sbox_glob == 1 )
  {
    SetControl(idx_par_glob, val_glob); // Set (again) and send
  }
  else if( selected_sbox_glob == 2 )
  {
    GateSetting((THR30II_GATE)idx_par_glob, val_glob); // Set (again) and send
  }
  else if( selected_sbox_glob >= 3 && selected_sbox_glob <= 6 )
  {
    uint8_t effect_type = selected_sbox_glob - 3; // Excluding Amp, Comp, NGate
    //EffectSetting((THR30II_EFF_TYPES)effect_type, (uint16_t)idx_par_glob, val_glob); // FIXME: Does not work, therefore, the code below
    if(effect_type == 0)      { SendParameterSetting((un_cmd) { THR30II_INFO_CHOR[(THR30II_EFF_SET_CHOR)idx_par_glob].uk, THR30II_INFO_CHOR[(THR30II_EFF_SET_CHOR)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); } // Send settings change to THR
    else if(effect_type == 1) { SendParameterSetting((un_cmd) { THR30II_INFO_FLAN[(THR30II_EFF_SET_FLAN)idx_par_glob].uk, THR30II_INFO_FLAN[(THR30II_EFF_SET_FLAN)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
    else if(effect_type == 2) { SendParameterSetting((un_cmd) { THR30II_INFO_PHAS[(THR30II_EFF_SET_PHAS)idx_par_glob].uk, THR30II_INFO_PHAS[(THR30II_EFF_SET_PHAS)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
    else if(effect_type == 3) { SendParameterSetting((un_cmd) { THR30II_INFO_TREM[(THR30II_EFF_SET_TREM)idx_par_glob].uk, THR30II_INFO_TREM[(THR30II_EFF_SET_TREM)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
  }
  else if( selected_sbox_glob >= 7 && selected_sbox_glob <= 8 ) // enum THR30II_ECHO_TYPES { TAPE_ECHO, DIGITAL_DELAY }
  {
    uint8_t echo_type = selected_sbox_glob - 7; // Excluding Amp, Comp, NGate, PHASER, TREMOLO, FLANGER, CHORUS
    //EchoSetting((THR30II_ECHO_TYPES)echo_type, (uint16_t)idx_par_glob, val_glob); // FIXME: Does not work, therefore, the code below
    if(echo_type == 0)      { SendParameterSetting((un_cmd) { THR30II_INFO_TAPE[(THR30II_ECHO_SET_TAPE)idx_par_glob].uk, THR30II_INFO_TAPE[(THR30II_ECHO_SET_TAPE)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); } // Send settings change to THR
    else if(echo_type == 1) { SendParameterSetting((un_cmd) { THR30II_INFO_DIGI[(THR30II_ECHO_SET_DIGI)idx_par_glob].uk, THR30II_INFO_DIGI[(THR30II_ECHO_SET_DIGI)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
  }
  else if( selected_sbox_glob >= 9 && selected_sbox_glob <= 12 ) // enum THR30II_REV_TYPES { SPRING, PLATE, HALL, ROOM }
  {
    uint8_t reverb_type = selected_sbox_glob - 9; // Excluding Amp, Comp, NGate, PHASER, TREMOLO, FLANGER, CHORUS, TAPE_ECHO, DIGITAL_DELAY
    //ReverbSetting((THR30II_REV_TYPES)reverb_type, (uint16_t)idx_par_glob, val_glob); // FIXME: Does not work, therefore, the code below
    if(reverb_type == 0)      { SendParameterSetting((un_cmd) { THR30II_INFO_SPRI[(THR30II_REV_SET_SPRI)idx_par_glob].uk, THR30II_INFO_SPRI[(THR30II_REV_SET_SPRI)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); } // Send settings change to THR
    else if(reverb_type == 1) { SendParameterSetting((un_cmd) { THR30II_INFO_PLAT[(THR30II_REV_SET_PLAT)idx_par_glob].uk, THR30II_INFO_PLAT[(THR30II_REV_SET_PLAT)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
    else if(reverb_type == 2) { SendParameterSetting((un_cmd) { THR30II_INFO_HALL[(THR30II_REV_SET_HALL)idx_par_glob].uk, THR30II_INFO_HALL[(THR30II_REV_SET_HALL)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
    else if(reverb_type == 3) { SendParameterSetting((un_cmd) { THR30II_INFO_ROOM[(THR30II_REV_SET_ROOM)idx_par_glob].uk, THR30II_INFO_ROOM[(THR30II_REV_SET_ROOM)idx_par_glob].sk}, (type_val<double>) {0x04, val_glob}); }
  }
}

// NOTE: For the parameters, alignment is important between stompBoxes, updateStatusMask, FSM9b_1, enums definitions (THR30II.h)
void THR30II_Settings::setParbyAudioVolKnob(double val)
{
  // NOTE: Sending updates back to thrii just few times a second, see gui_timing()
  //       Only set the values in this function but do not send them to THRII
  sendEditToTHR = false;

  uint8_t idx_par = sboxes[selected_sbox]->getFocus();

  //--------------------------------------------------
  selected_sbox_glob = selected_sbox;
  idx_par_glob = idx_par;
  val_glob = val;
  //--------------------------------------------------

  if( selected_sbox == 0 )
  {
    compressor_setting[(THR30II_COMP)idx_par] = constrain(val, 0, 100);
    maskCUpdate |= maskCompressor;
  }
  else if( selected_sbox == 1 )
  {
    control[idx_par] = val;
    maskCUpdate |= maskGainMaster;
  }
  else if( selected_sbox == 2 )
  {
    gate_setting[(THR30II_GATE)idx_par] = constrain(val, 0, 100);
    maskCUpdate |= maskNoiseGate;
  }
  else if( selected_sbox >= 3 && selected_sbox <= 6 )
  {
    uint8_t effect_type = selected_sbox - 3; // Excluding Amp, Comp, NGate
    TRACE_THR30IIPEDAL(Serial.println("Volume knob: " + String(effect_type) + "(" + String(effecttype) + ")" + ", " + String(idx_par) + ", " + String(val));)
    effect_setting[(THR30II_EFF_TYPES)effect_type][idx_par] = val;
    maskCUpdate |= maskFxUnit;
  }
  else if( selected_sbox >= 7 && selected_sbox <= 8 ) // enum THR30II_ECHO_TYPES { TAPE_ECHO, DIGITAL_DELAY }
  {
    uint8_t echo_type = selected_sbox - 7; // Excluding Amp, Comp, NGate, PHASER, TREMOLO, FLANGER, CHORUS
    TRACE_THR30IIPEDAL(Serial.println("Volume knob: " + String(echo_type) + "(" + String(echotype) + ")" + ", " + String(idx_par) + ", " + String(val));)
    echo_setting[(THR30II_ECHO_TYPES)echo_type][idx_par] = val;
    maskCUpdate |= maskEcho;
  }  
  else if( selected_sbox >= 9 && selected_sbox <= 12 ) // enum THR30II_REV_TYPES { SPRING, PLATE, HALL, ROOM }
  {
    uint8_t reverb_type = selected_sbox - 9; // Excluding Amp, Comp, NGate, PHASER, TREMOLO, FLANGER, CHORUS, TAPE_ECHO, DIGITAL_DELAY
    TRACE_THR30IIPEDAL(Serial.println("Volume knob: " + String(reverb_type) + "(" + String(reverbtype) + ")" + ", " + String(idx_par) + ", " + String(val));)
    reverb_setting[(THR30II_REV_TYPES)reverb_type][idx_par] = val;
    maskCUpdate |= maskReverb;
  }
  sendEditToTHR = true; // Notify the timing function that there is data to be sent to THRII
}
