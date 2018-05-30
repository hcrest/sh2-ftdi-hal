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

#include "FtdiHalRpi.h"
#include "ftd2xx.h"
#include "Rfc1662Framer.h"
#include "TimerService.h"

#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>

#define DEBUG_BUFFER 0
#define USE_SELECT
#define PPP_FLAG 0x7E
#define BYTE_TX_MIN_SPACE_US 200


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
static int deviceDescriptor = -1;
static EVENT_HANDLE commEvent;

// =================================================================================================
// LOCAL FUNCTIONS PROTOTYPES
// =================================================================================================
void uart_errno_printf(const char* s, ...) {
    va_list vl;
    va_start(vl, s); // initializes vl with arguments after s. accessed by int=va_arg(vl,int)
    vfprintf(stderr, s, vl);
    fprintf(stderr, " errno = %d (%s)\n", errno, strerror(errno));
    fflush(stderr);
    va_end(vl);
}

int uart_rpi_read(uint8_t* buf, uint32_t buffer_size);


// =================================================================================================
// PUBLIC FUNCTIONS - FtdiHalRpi
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHalRpi::open
// -------------------------------------------------------------------------------------------------
int FtdiHalRpi::open() {
    FT_STATUS status;

    struct termios tty;
    speed_t baud = B3000000;
    char device[15];
    snprintf(device, 15, "/dev/ttyUSB%d", deviceIdx_);

    // we dont know how many bytes to read. blocked by interrupt poll
    if ((deviceDescriptor = ::open(device, O_RDWR | O_NOCTTY | O_NONBLOCK)) == -1) {
        uart_errno_printf("uart_connect: OPEN %s:", device);
        return -1;
    }

    if (tcgetattr(deviceDescriptor, &tty) < 0) {
        fprintf(stderr, "unable to read port attributes");
        return -1;
    }

    tty.c_iflag &=
            ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF); //| IGNPAR
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;
    tty.c_cflag |= CS8 | CREAD | CLOCAL;

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    if (tcsetattr(deviceDescriptor, TCSANOW, &tty) < 0) {
        fprintf(stderr, "unable to set port attributes");
        return -1;
    }

    fsync(deviceDescriptor);

    // TODO Attemp to resolve the start / long frame issue.
    usleep(2000000);

    if (tcflush(deviceDescriptor, TCIOFLUSH) == 0) {
        printf("FLUSHED! \n");
    } else {
        printf("QUESTION! \n");
    }

    // Issue Soft reset which triggers SensorHub to send the advertise response
    softreset();

    return 0;
}

// -------------------------------------------------------------------------------------------------
// FtdiHalRpi::close
// -------------------------------------------------------------------------------------------------
void FtdiHalRpi::close() {
    ::close(deviceDescriptor);
}


// =================================================================================================
// PRIVATE FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// FtdiHalRpi::WriteBytesToDevice
// -------------------------------------------------------------------------------------------------
BOOL FtdiHalRpi::WriteBytesToDevice(LPVOID lpBuffer,
                                 DWORD nNumberOfBytesToWrite,
                                 LPDWORD lpNumberOfBytesWritten) {

    usleep(BYTE_TX_MIN_SPACE_US);
    return (::write(deviceDescriptor, lpBuffer, 1) == 1);
}

// -------------------------------------------------------------------------------------------------
// FtdiHalRpi::ReadBytesToDevice
// -------------------------------------------------------------------------------------------------
int FtdiHalRpi::ReadBytesToDevice(void) {
    uint8_t rxBuffer[1024];
    uint16_t MAX_READ = sizeof(rxBuffer);
    size_t bytesRead = RpiUartRead(rxBuffer, MAX_READ);

    if ((bytesRead > 0) && (bytesRead <= MAX_READ)) {

#if TRACE_IO
        fprintf(stderr, "bytes read: ");
        printBytes(rxBuffer, bytesRead);
#endif

        int nMsg;
        nMsg = framer_.decode((uint8_t*)rxBuffer, bytesRead);

        return nMsg;
        
    } else {
        return 0;
    }

}

// -------------------------------------------------------------------------------------------------
// FtdiHalRpi::RpiUartRead
// -------------------------------------------------------------------------------------------------
int FtdiHalRpi::RpiUartRead(uint8_t* buf, uint32_t buffer_size) {

    if (deviceDescriptor <= 0) {
        return -1;
    }

    if (buffer_size == 0) {
        return buffer_size;
    }

#ifdef USE_SELECT

    fd_set fds;
    struct timeval timeout;
    int status;

    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    FD_ZERO(&fds);
    FD_SET(deviceDescriptor, &fds);
    // lseek(deviceDescriptor, 0, SEEK_SET);
    status = select(deviceDescriptor + 1, &fds, NULL, NULL, &timeout);

    if ((status < 0)) { // && (errno != EINTR) && (errno != EBADF)
#if DEBUG_BUFFER
        fprintf(stderr, "select errno = %d \n", errno); // (%s) , strerror(errno)
#endif
        return status;

    } else if (status == 0) {
#if DEBUG_BUFFER
        fprintf(stderr, "select zero\n");
#endif
        return 0;

    } else if (status > 0) {
#endif

        int ready = 0;
        ready = ::read(deviceDescriptor, buf, buffer_size);
#if DEBUG_BUFFER
        if (ready < 0)
            fprintf(stderr, "read errno = %d \n", errno); // (%s) , strerror(errno)
        else if (ready == 0)
            fprintf(stderr, "no data in buffer\n");
        else if (ready > buffer_size)
            fprintf(stderr, "more than %d bytes in buffer\n", buffer_size);
        else if ((buf[0] != PPP_FLAG))
            fprintf(stderr, "first byte of a uart message mismatch to %d %d\n", PPP_FLAG, buf[0]);
        else if ((buf[ready - 1] != PPP_FLAG))
            fprintf(stderr,
                    "last byte of a uart message mismatch to %d %d\n",
                    PPP_FLAG,
                    buf[ready - 1]);
#endif
        return ready;

#ifdef USE_SELECT
    }
    return -1;
#endif
}
