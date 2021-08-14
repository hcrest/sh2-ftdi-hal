/*
 * Copyright 2018-21 CEVA, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License and
 * any applicable agreements you may have with CEVA, Inc.
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

#ifndef FTDI_HAL_H
#define FTDI_HAL_H

#ifdef _WIN32
#include <Windows.h>
#else
#include "WinTypes.h"
#endif

#include "Rfc1662Framer.h"

// =================================================================================================
// DATA TYPES
// =================================================================================================
class TimerSrv;

// =================================================================================================
// CLASS DEFINITON - FtdiHal
// =================================================================================================
class FtdiHal {
public:
    explicit FtdiHal() : deviceIdx_(0){};
    virtual ~FtdiHal(){};

    /**
    * @brief Initialize the FTDI HAL
    *
    * @param  deviceIdx The device index.  Must be 0 if only one device is attached. For multiple
    * devices 1, 2 etc.
    * @return 0 on success.  Negative value on error.
    */
    virtual int init(int deviceIdx, TimerSrv* timer);

    // send soft reset command
    virtual void softreset();

    virtual int open();

    virtual void close();

    virtual int read(uint8_t* pBuffer, unsigned len, uint32_t* t_us);

    virtual int write(uint8_t* pBuffer, unsigned len);

    // these functions allow read/write data including header
    virtual int writeData(uint8_t* pBuffer, unsigned len);
    virtual int readData(uint8_t* pBuffer, unsigned len, uint32_t* t_us);


protected:
    int deviceIdx_;
    TimerSrv* timer_;
    Rfc1662Framer framer_;

    uint8_t decodeBuf_[1024 + 512]; // TODO Adjust the buffer size
    uint8_t nRemainMsg_;
    uint8_t* pNextMsg_;
    uint32_t lastSampleTime_us_;
    uint8_t bridgeHostInterfaceId_;

    virtual int ReadMessage(uint8_t* pBuffer, unsigned len, uint32_t* t_us, uint8_t stripHeaderLen);
    virtual int GetNextMessage(uint8_t* pBuffer, unsigned len, uint32_t* t_us, uint8_t stripHeaderLen);

    virtual int ReadBytesToDevice(void) = 0;

    
    virtual void WriteEncodedFrame(UCHAR* bytes, DWORD length);
    virtual BOOL WriteBytesToDevice(LPVOID lpBuffer,
                                    DWORD nNumberOfBytesToWrite,
                                    LPDWORD lpNumberOfBytesWritten) = 0;

    // Helper Function
    virtual void PrintBytes(uint8_t* bytes, DWORD len);
};

#endif // FTDI_HAL_H
