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

#ifndef LOGGER_APP_H
#define LOGGER_APP_H

#include <stdint.h>

// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================
enum SensorMode {
    SENSOR_MODE_9AGM,
    SENSOR_MODE_6AG,
    SENSOR_MODE_6AM,
    SENSOR_MODE_6GM,
    SENSOR_MODE_3A,
    SENSOR_MODE_3G,
    SENSOR_MODE_3M,
    SENSOR_MODE_ALL
};

// =================================================================================================
// DATA TYPES
// =================================================================================================
class TimerSrv;
class DsfLogger;
class FtdiHal;

// =================================================================================================
// CLASS DEFINITON - LoggerApp
// =================================================================================================
class LoggerApp {
public:
    LoggerApp(){};

    // ---------------------------------------------------------------------------------------------
    // DATA TYPES
    // ---------------------------------------------------------------------------------------------
    struct appConfig_s {
        int deviceNumber = 0;
        double rate = 100.0;
        bool outputRaw = false;
        bool outputCalibrated = false;
        bool outputUncalibrated = false;
        SensorMode sensorMode = SENSOR_MODE_9AGM;
        bool dcdAutoSave = false;
        bool clearDcd = false;
        uint8_t calEnableMask = 0x8; // TODO review data type
        bool orientationNed = true;
    };

    int init(appConfig_s* appConfig, TimerSrv* timer, FtdiHal* ftdiHal, DsfLogger* logger);

    int service();

    int finish();

private:
    bool firstReportReceived_ = false;
};

#endif // LOGGER_APP_H
