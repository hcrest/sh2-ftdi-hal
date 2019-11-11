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

#ifndef FTDI_HAL_RPI_H
#define FTDI_HAL_RPI_H

#include "FtdiHal.h"

 // =================================================================================================
 // DATA TYPES
 // =================================================================================================
class TimerSrv;

// =================================================================================================
// CLASS DEFINITON - FtdiHalRpi
// =================================================================================================
class FtdiHalRpi : public FtdiHal {
public:
	explicit FtdiHalRpi() : FtdiHal() {};
	virtual ~FtdiHalRpi() {};

	// inherit from FtdiHal
	virtual int open();
	virtual void close();
	
    virtual int init(int deviceIdx, TimerSrv* timer);
    virtual int init(const char* device, TimerSrv* timer);

protected:
    const char* device_;

private:
	virtual int ReadBytesToDevice(void);

	virtual BOOL WriteBytesToDevice(LPVOID lpBuffer,
		DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten);

    int RpiUartRead(uint8_t* buf, uint32_t buffer_size);
	
	int deviceDescriptor_;
};

#endif // FTDI_HAL_RPI_H
