// =================================================================================================
//
// Copyright (c) 2016 Hillcrest Laboratories, Inc.  All rights reserved.
// HILLCREST PROPRIETARY/CONFIDENTIAL
//
// Rfc1662Framer.cpp
//
// =================================================================================================

// =================================================================================================
// INCLUDE FILES
// =================================================================================================
#include "Rfc1662Framer.h"

// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================

// =================================================================================================
// CLASS VARIABLES
// =================================================================================================

// =================================================================================================
// CLASS CONSTANTS
// =================================================================================================
const uint8_t Rfc1662Framer::FLAG = 0x7E;
const uint8_t Rfc1662Framer::ESC = 0x7D;
const uint8_t Rfc1662Framer::XOR = 0x20;
const int Rfc1662Framer::ERR_DEST_NULL = -1;
const int Rfc1662Framer::ERR_DEST_LEN_ZERO = -2;
const int Rfc1662Framer::ERR_DEST_OVERFLOW = -3;
const unsigned int Rfc1662Framer::NUM_LEN_BYTES = 2;

// =================================================================================================
// CLASS DEFINITION
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// PUBLIC METHODS
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// Rfc1662Framer::Rfc1662Framer
// -------------------------------------------------------------------------------------------------
Rfc1662Framer::Rfc1662Framer(void) : state_(HUNT_), dest_(0), destLen_(0) {
}

// -------------------------------------------------------------------------------------------------
// Rfc1662Framer::encode
// -------------------------------------------------------------------------------------------------
int Rfc1662Framer::encode(uint8_t* dest, const uint8_t* src, size_t len, BlockEncode_e be) {
    size_t i;
    uint8_t* start;

    start = dest;

    if (dest == 0 || src == 0 || len == 0) {
        return 0;
    }

    // Handle opening flag
    if (be == COMPLETE || be == FIRST) {
        *dest = FLAG;
        ++dest;
    }

    // Encode buffer
    for (i = 0; i < len; ++i) {
        if (*src == FLAG || *src == ESC) {
            *dest = ESC;
            ++dest;
            *dest = *src ^ XOR;
        } else {
            *dest = *src;
        }
        ++dest;
        ++src;
    }

    // Handle closing flag
    if (be == COMPLETE || be == LAST) {
        *dest = FLAG;
        ++dest;
    }

    return dest - start;
}

// -------------------------------------------------------------------------------------------------
// Rfc1662Framer::decodeInit
// -------------------------------------------------------------------------------------------------
void Rfc1662Framer::decodeInit(uint8_t* dest, size_t len) {
    dest_ = dest;
    destLen_ = len;
    state_ = HUNT_;
}

// -------------------------------------------------------------------------------------------------
// Rfc1662Framer::decode
// -------------------------------------------------------------------------------------------------
int Rfc1662Framer::decode(const uint8_t* src, size_t len) {
    int msgCnt = 0;
    uint16_t destLen;
    size_t i;

    if (dest_ == 0) {
        return ERR_DEST_NULL;
    }
    if (destLen_ == 0) {
        return ERR_DEST_LEN_ZERO;
    }

    if (state_ == HUNT_ || state_ == START_) {
        // Decoding new message
        destCursor_ = dest_ + NUM_LEN_BYTES; // Leave room for length bytes
        destLenStore_ = dest_;
    } else {
        // Move the in progress message to the front of the dest buffer
        size_t inProgLen = destCursor_ - destLenStore_;
        uint8_t* fromPtr = destLenStore_;
        uint8_t* toPtr = dest_;
        for (i = 0; i < inProgLen; ++i) {
            *toPtr = *fromPtr;
            ++toPtr;
            ++fromPtr;
        }
        destLenStore_ = dest_;
        destCursor_ = toPtr;
    }

    // This loop decodes each byte in the source buffer. It has early returns to terminate buffer
    // overflow cases
    for (i = 0; i < len; ++i) {
        switch (state_) {
            case HUNT_:
                if (*src == FLAG) {
                    state_ = START_;
                }
                break;

            case START_:
                if (*src == ESC) {
                    state_ = DECODE_ESC_;
                } else if (*src != FLAG) {
                    state_ = DECODE_;
                    if (destCursor_ == dest_ + destLen_) {
                        return ERR_DEST_OVERFLOW;
                    }
                    *destCursor_ = *src;
                    ++destCursor_;
                }
                break;

            case DECODE_:
                if (*src == FLAG) {
                    state_ = START_;
                    destLen = destCursor_ - destLenStore_ - NUM_LEN_BYTES;
                    destLenStore_[0] = destLen;
                    destLenStore_[1] = destLen >> 8;
                    ++msgCnt;
                    // Prepare for next message
                    destLenStore_ = destCursor_;
                    destCursor_ += NUM_LEN_BYTES;
                } else if (*src == ESC) {
                    state_ = DECODE_ESC_;
                } else {
                    if (destCursor_ == dest_ + destLen_) {
                        return ERR_DEST_OVERFLOW;
                    }
                    *destCursor_ = *src;
                    ++destCursor_;
                }
                break;

            case DECODE_ESC_:
                if (*src == FLAG) {
                    // drop message, reset cursor to start of current decode destination
                    destCursor_ = destLenStore_ + NUM_LEN_BYTES;
                    state_ = START_;
                } else {
                    if (destCursor_ == dest_ + destLen_) {
                        return ERR_DEST_OVERFLOW;
                    }
                    *destCursor_ = *src ^ XOR;
                    ++destCursor_;
                    state_ = DECODE_;
                }
                break;
        }
        ++src;
    }

    return msgCnt;
}

// -------------------------------------------------------------------------------------------------
// PRIVATE METHODS
// -------------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------------
// Rfc1662Framer::<method name>
// -------------------------------------------------------------------------------------------------
