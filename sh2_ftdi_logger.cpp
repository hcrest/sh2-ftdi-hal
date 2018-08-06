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

#define _CRT_SECURE_NO_WARNINGS

// =================================================================================================
// INCLUDE FILES
// =================================================================================================
#include "TimerService.h"
#include "DsfLogger.h"
#include "LoggerApp.h"

#ifdef _WIN32
#include "FtdiHalWin.h"
#else
#include "FtdiHalRpi.h"
#include "signal.h"
#endif

#include <string.h>
#include <iomanip>
#include <iostream>

// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================

// =================================================================================================
// DATA TYPES
// =================================================================================================

// =================================================================================================
// LOCAL VARIABLES
// =================================================================================================
static LoggerApp loggerApp_;
#ifdef _WIN32
static TimerSrvWin timer_;
static FtdiHalWin ftdiHal_;
#else
static TimerSrvRpi timer_;
static FtdiHalRpi ftdiHal_;
#endif
static DsfLogger logger_;
static bool runApp_ = true;

// =================================================================================================
// LOCAL FUNCTIONS
// =================================================================================================
#ifndef _WIN32
void breakHandler(int) {
    fprintf(stderr, "break!\n");
    if (!runApp_) {
        fprintf(stderr, "force quit\n");
        exit(0);
    }
    runApp_ = false;
}
#endif

void usage(const char* myname) {
    fprintf(stderr,
            "Usage: %s <out.dsf> [--deviceNumber=<id>] [--rate=<rate>] [--raw] [--calibrated] "
            "[--uncalibrated] [--mode=<9agm,6ag,6am,6gm,3a,3g,3m,all>] [--dcdAutoSave] [--calEnable=0x<mask>]"
            "[--orientation=enu,ned][--config]\n"
            "   out.dsf              - output dsf file\n"
            "   --deviceNumber=<id>  - which device to open.  0 for a single device.\n"
            "   --rate=<rate>        - requested sampling rate for all sensors.  Default: 100Hz\n"
            "   --raw                - include raw data and use SAMPLE_TIME for timing\n"
            "   --calibrated         - include calibrated output sensors\n"
            "   --uncalibrated       - include uncalibrated output sensors\n"
            "   --mode=<mode>        - sensors types to log.  9agm, 6ag, 6am, 6gm, 3a, 3g, 3m or all.\n"
            "   --step               - include step detector\n"
            "   --pac                - include activity classifier\n"
            "   --dcdAutoSave        - enable DCD auto saving.  No dcd save by default.\n"
            "   --clearDcd           - clear DCD and reset upon startup.\n"
            "   --calEnable=0x<mask> - cal enable mask.  Bits: Planar, A, G, M.  Default 0x8\n"
            "   --orientation=<orientation> - system orientation. enu, ned. Default: ned\n",
            "   --config             - get the list of sensors from sensorList.cfg file \n",
            myname);
}

// ================================================================================================
// MAIN
// ================================================================================================
int main(int argc, const char* argv[]) {
    bool argError = false;
    char const* outFilePath = NULL;
    LoggerApp::appConfig_s appConfig;

#ifndef _WIN32
    struct sigaction act;
    act.sa_handler = breakHandler;
    sigaction(SIGINT, &act, NULL);
#endif

    for (int i = 1, j = 0; i < argc; i++) {
        const char* arg = argv[i];
        if (arg[0] == '-') {
            if (strstr(arg, "--deviceNumber=") == arg) {
                const char* val = strchr(arg, '=') + 1;
                appConfig.deviceNumber = strtol(val, NULL, 10);
            } else if (strstr(arg, "--rate=") == arg) {
                const char* val = strchr(arg, '=') + 1;
                appConfig.rate = strtod(val, NULL);
            } else if (strcmp(arg, "--raw") == 0) {
                appConfig.outputRaw = true;
            } else if (strcmp(arg, "--calibrated") == 0) {
                appConfig.outputCalibrated = true;
            } else if (strcmp(arg, "--uncalibrated") == 0) {
                appConfig.outputUncalibrated = true;
            } else if (strstr(arg, "--mode=") == arg) {
                const char* val = strchr(arg, '=') + 1;
                if (strcmp(val, "9agm") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_9AGM;
                } else if (strcmp(val, "6ag") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_6AG;
                } else if (strcmp(val, "6am") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_6AM;
                } else if (strcmp(val, "6gm") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_6GM;
                } else if (strcmp(val, "3a") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_3A;
                } else if (strcmp(val, "3g") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_3G;
                } else if (strcmp(val, "3m") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_3M;
                } else if (strcmp(val, "all") == 0) {
                    appConfig.sensorMode = SENSOR_MODE_ALL;
                } else {
                    fprintf(stderr, "Unknown mode: %s\n", arg);
                    argError = true;
                }
            } else if (strcmp(arg, "--step") == 0) {
                appConfig.step = true;
            } else if (strcmp(arg, "--pac") == 0) {
                appConfig.pac = true;
            } else if (strcmp(arg, "--dcdAutoSave") == 0) {
                appConfig.dcdAutoSave = true;
            } else if (strcmp(arg, "--clearDcd") == 0) {
                appConfig.clearDcd = true;
            } else if (strstr(arg, "--calEnable=") == arg) {
                const char* val = strchr(arg, '=') + 1;
                appConfig.calEnableMask = (uint8_t)strtol(val, NULL, 0);
            } else if (strstr(arg, "--orientation=") == arg) {
                const char* val = strchr(arg, '=') + 1;
                if (strcmp(val, "enu") == 0) {
                    appConfig.orientationNed = false;
                } else {
                    appConfig.orientationNed = true;
                }
            } else if (strcmp(arg, "--config") == 0) {
                    appConfig.config = true;
            } else {
                fprintf(stderr, "Unknown argument: %s\n", arg);
                argError = true;
            }

            continue;
        }

        // positional arguments
        if (j == 0) {
            outFilePath = arg;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg);
            argError = true;
        }

        j++;
    }

    if (outFilePath == NULL) {
        argError = true;
    }

    if (argError) {
        usage(argv[0]);
        return -1;
    }

	// --------------------------------------------------------------------------------------------
	// Start Application
	// --------------------------------------------------------------------------------------------
	runApp_ = true;

	// Initialize DSF Logger
    bool rv = logger_.init(outFilePath, appConfig.orientationNed);
    if (!rv) {
        std::cout << "ERROR: Unable to open file:  \"" << outFilePath << "\"" << std::endl;
        return -1;
    }

    // Initialize Timer
    timer_.init();

    // Initialze FTDI HAL
    int status;
    FtdiHal* pFtdiHal = &ftdiHal_;
    status = pFtdiHal->init(appConfig.deviceNumber, &timer_);
	// status = ftdiHal_.init(appConfig.deviceNumber, &timer_);
    if (status != 0) {
        std::cout << "ERROR: Initialize FTDI HAL failed!\n";
        return -1;
    }

    // Initialize the LoggerApp
    status = loggerApp_.init(&appConfig, &timer_, &ftdiHal_, &logger_);
    if (status != 0) {
        std::cout << "ERROR: Initialize LoggerApp failed!\n";
        return -1;
    }

#ifdef _WIN32
    HANDLE hstdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hstdin, &mode);
    SetConsoleMode(hstdin, 0);

    FlushConsoleInputBuffer(hstdin);
    std::cout << "\nPress a key to exit . . ." << std::endl;
#else

	std::cout << "\nPress Ctrl-C to exit . . ." << std::endl;
#endif

	std::cout << "\nProcessing Sensor Reports . . ." << std::endl;

	uint64_t curTime_us_ = timer_.getTimestamp_us();
	uint64_t lastChecked_us_ = curTime_us_;

    while (runApp_) {

#ifdef _WIN32
		curTime_us_ = timer_.getTimestamp_us();
		
		if (curTime_us_ - lastChecked_us_ > 200000) {
            lastChecked_us_ = curTime_us_;
            INPUT_RECORD event;
            DWORD count;

            PeekConsoleInput(hstdin, &event, 1, &count);
            if (count) {
                ReadConsoleInput(hstdin, &event, 1, &count);
                if ((event.EventType == KEY_EVENT) && !event.Event.KeyEvent.bKeyDown) {
					runApp_ = false;
					break;
                }
            }
        }
#endif

        loggerApp_.service();
    }

    std::cout << "\nINFO: Shutting down" << std::endl;

    loggerApp_.finish();

#ifdef _WIN32
    SetConsoleMode(hstdin, mode);
#endif
    return 0;
}
