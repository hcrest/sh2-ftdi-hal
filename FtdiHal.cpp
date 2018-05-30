/*
 * Copyright 2018 Hillcrest Laboratories, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License and
 * any applicable agreements you may have with Hillcrest Laboratories, Inc.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FtdiHal.h"
#include "TimerService.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================
#define TRACE_IO 0

// =================================================================================================
// DATA TYPES
// =================================================================================================

// =================================================================================================
// LOCAL VARIABLES
// =================================================================================================

// =================================================================================================
// LOCAL FUNCTIONS PROTOTYPES
// =================================================================================================

// =================================================================================================
// PUBLIC FUNCTIONS - FtdiHal
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHal::init
// -------------------------------------------------------------------------------------------------
int FtdiHal::init(int deviceIdx, TimerSrv* timer) {

    deviceIdx_ = deviceIdx; // TODO
    timer_ = timer;
    nRemainMsg_ = 0;

    framer_.decodeInit(decodeBuf_, sizeof(decodeBuf_));

    return 0;
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::softreset
// -------------------------------------------------------------------------------------------------
void FtdiHal::softreset() {
    UCHAR cmd1[] = {0x01, 0x05, 0x00, 0x01, 1, 0x01};
    WriteData(cmd1, sizeof(cmd1));
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::open
// -------------------------------------------------------------------------------------------------
int FtdiHal::open() {
    return 0;
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::close
// -------------------------------------------------------------------------------------------------
void FtdiHal::close() {
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::read
// -------------------------------------------------------------------------------------------------
int FtdiHal::read(uint8_t* pBuffer, unsigned len, uint32_t* t_us) {

	int rtnLen = 0;

	// Return the buffered decoded messages
	rtnLen = GetNextMessage(pBuffer, len, t_us);
	if (rtnLen) {
		return rtnLen;
	}

	int nMsg = ReadBytesToDevice();
    
    if (nMsg) {
        nRemainMsg_ = nMsg;
        pNextMsg_ = decodeBuf_;
        lastSampleTime_us_ = (uint32_t)timer_->getTimestamp_us();
    }

	return GetNextMessage(pBuffer, len, t_us);
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::write
// -------------------------------------------------------------------------------------------------
int FtdiHal::write(uint8_t* pBuffer, unsigned len) {

    // Do nothing if len is zero
    if (len == 0) {
        return 0;
    }

    UCHAR headerData[] = {0x1}; // SHTP over UART header byte
    DWORD headerLength = sizeof(headerData);
    UCHAR* dataWithHeader = (UCHAR*)malloc(headerLength + len);

    memcpy(dataWithHeader, headerData, headerLength);
    memcpy(dataWithHeader + headerLength, pBuffer, len);

    WriteData(dataWithHeader, headerLength + len);

    free(dataWithHeader);

    return len;
}


// =================================================================================================
// PRIVATE FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHal::PrintBytes
// -------------------------------------------------------------------------------------------------
void FtdiHal::PrintBytes(uint8_t* bytes, DWORD len) {
    for (DWORD i = 0; i < len; ++i) {
        fprintf(stderr, "%02x ", bytes[i]);
    }
    fprintf(stderr, "\n");
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::WriteData
// -------------------------------------------------------------------------------------------------
// send some data to the device.  data will be encoded/framed.
// -------------------------------------------------------------------------------------------------
void FtdiHal::WriteData(UCHAR* bytes, DWORD length) {
    UCHAR* encodedFrame = (UCHAR*)malloc((length * 2 + 2) * sizeof(UCHAR));
    size_t encodedLength = 0;

    encodedLength = framer_.encode((uint8_t*)encodedFrame, (uint8_t*)bytes, length);

    WriteEncodedFrame(encodedFrame, (DWORD)(encodedLength));

    free(encodedFrame);
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::WriteEncodedFrame
// -------------------------------------------------------------------------------------------------
// writes an already-encoded frame to the device
// -------------------------------------------------------------------------------------------------
void FtdiHal::WriteEncodedFrame(UCHAR* bytes, DWORD length) {
    DWORD bytesToWrite = length;
    DWORD bytesWritten = 0;

#if TRACE_IO
    fprintf(stderr, "write bytes: ");
    PrintBytes(bytes, length);
#endif

    // write bytes individually so hub can keep up
    for (DWORD i = 0; i < length; i++) {
        BOOL status = WriteBytesToDevice(&bytes[i], 1, &bytesWritten);
        if (!status) {
            fprintf(stderr, "WriteBytesToDevice failed!\n");
        }
    }
}

// -------------------------------------------------------------------------------------------------
// FtdiHal::GetNextMessage
// -------------------------------------------------------------------------------------------------
int FtdiHal::GetNextMessage(uint8_t* pBuffer, unsigned len, uint32_t* t_us) {
    int payloadLen = 0;

    if (nRemainMsg_) {
        payloadLen = ((*(pNextMsg_ + 1) << 8) | *(pNextMsg_)) - 1;
        memcpy(pBuffer, pNextMsg_ + 3, payloadLen);
        *t_us = lastSampleTime_us_;

        nRemainMsg_--;
        if (nRemainMsg_) {
            pNextMsg_ += (3 + payloadLen);
        }
    }
    return payloadLen;
}
