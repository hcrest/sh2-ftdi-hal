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

#include "FtdiHalWin.h"
#include "ftd2xx.h"
#include "TimerService.h"
#include <math.h>
#include <stdio.h>

// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================
#define TRACE_IO 0

// =================================================================================================
// DATA TYPES
// =================================================================================================

// =================================================================================================
// LOCAL CONST VARIABLES
// =================================================================================================
static const DWORD BAUD_RATE = 3000000;
static const UCHAR LATENCY_TIMER = 1;
static const UCHAR LATENCY_TIMER_STARTUP = 10;

// =================================================================================================
// LOCAL VARIABLES
// =================================================================================================
static HANDLE commEvent;

// =================================================================================================
// LOCAL FUNCTIONS PROTOTYPES
// =================================================================================================

// =================================================================================================
// PUBLIC FUNCTIONS - FtdiHal
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHalWin::init
// -------------------------------------------------------------------------------------------------
int FtdiHalWin::init(int deviceIdx, TimerSrv* timer) {

    anyRx_ = false;
	return FtdiHal::init(deviceIdx, timer);
}

// -------------------------------------------------------------------------------------------------
// FtdiHalWin::open
// -------------------------------------------------------------------------------------------------
int FtdiHalWin::open() {
    FT_STATUS status;

    status = FT_Open(deviceIdx_, &ftHandle_);
    if (status != FT_OK) {
        fprintf(stderr, "Unable to find an FTDI COM port\n");
        return -1;
    }

    LONG comPort = -1;
    status = FT_GetComPortNumber(ftHandle_, &comPort);
    fprintf(stderr, "FTDI device found on COM%d\n", comPort);

    status = FT_SetBaudRate(ftHandle_, BAUD_RATE);
    if (status != FT_OK) {
        fprintf(stderr, "Unable to set baud rate to: %d\n", BAUD_RATE);
        return -1;
    }

    status = FT_SetFlowControl(ftHandle_, FT_FLOW_NONE, 0, 0);
    if (status != FT_OK) {
        fprintf(stderr, "Failed to set flow control\n");
    }

    status = FT_SetDataCharacteristics(ftHandle_, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    if (status != FT_OK) {
        fprintf(stderr, "Unable to set data characteristics\n");
        return -1;
    }

    // status = FT_ClrDtr(ftHandle_);
    // status = FT_SetDtr(ftHandle_);

    status = FT_SetLatencyTimer(ftHandle_, LATENCY_TIMER_STARTUP);
    if (status != FT_OK) {
        fprintf(stderr, "Unable to set latency timer to: %d\n", LATENCY_TIMER_STARTUP);
        return -1;
    }

    status = FT_SetTimeouts(ftHandle_, 1000, 3000);
    if (status != FT_OK) {
        fprintf(stderr, "Unable to set timeouts.\n");
        return -1;
    }

    commEvent = CreateEvent(NULL, false, false, "");
    FT_SetEventNotification(ftHandle_, FT_EVENT_RXCHAR, commEvent);

    // Issue Soft reset which triggers SensorHub to send the advertise response
    softreset();

    return 0;
}

// -------------------------------------------------------------------------------------------------
// FtdiHalWin::close
// -------------------------------------------------------------------------------------------------
void FtdiHalWin::close() {
    FT_Close(ftHandle_);
}


// =================================================================================================
// PRIVATE FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHalWin::WriteBytesToDevice
// -------------------------------------------------------------------------------------------------
BOOL FtdiHalWin::WriteBytesToDevice(LPVOID lpBuffer,
                                 DWORD nNumberOfBytesToWrite,
                                 LPDWORD lpNumberOfBytesWritten) {

	return FT_Write(ftHandle_, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten) == FT_OK;
}

// -------------------------------------------------------------------------------------------------
// FtdiHalWin::ReadBytesToDevice
// -------------------------------------------------------------------------------------------------
int FtdiHalWin::ReadBytesToDevice(void) {
	int nMsg = 0;

	DWORD EventDWord;
	DWORD TxBytes;
	DWORD RxBytes;

	CHAR rxBuffer[1024];
	DWORD MAX_READ = sizeof(rxBuffer);

	DWORD bytesRead = 0;

	FT_GetStatus(ftHandle_, &RxBytes, &TxBytes, &EventDWord);

	if (RxBytes > 0) {
#if TRACE_IO
		fprintf(stderr, "RxBytes=%d   %d  %d\n", RxBytes, TxBytes, EventDWord);
#endif

		if (FT_Read(ftHandle_, rxBuffer, (DWORD)fmin(RxBytes, MAX_READ), &bytesRead) == FT_OK) {
#if TRACE_IO
			fprintf(stderr, "bytes read: ");
			PrintBytes((uint8_t*)rxBuffer, bytesRead);
#endif
			nMsg = framer_.decode((uint8_t*)rxBuffer, bytesRead);
		}

		if (!anyRx_) {
			FT_SetLatencyTimer(ftHandle_, LATENCY_TIMER);
			anyRx_ = true;
		}
	}

	return nMsg;
}
