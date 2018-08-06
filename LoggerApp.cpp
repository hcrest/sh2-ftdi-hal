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

#include "LoggerApp.h"
#include "DsfLogger.h"
#include "FtdiHal.h"
#include "TimerService.h"

#include "math.h"
#include <string.h>
#include <iomanip>
#include <iostream>


// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================
enum class State {
    Idle,
    Reset,
    Startup,
    Run,
};

#ifdef _WIN32
#define RESET_TIMEOUT_ 1000000
#else
#define RESET_TIMEOUT_ 1000
#endif

// =================================================================================================
// DATA TYPES
// =================================================================================================
struct frsString_s {
    uint16_t recordId;
    char const* name;
};

// =================================================================================================
// LOCAL FUNCTION PROTOTYPES
// =================================================================================================
static int Sh2HalOpen(sh2_Hal_t* self);
static void Sh2HalClose(sh2_Hal_t* self);
static int Sh2HalRead(sh2_Hal_t* self, uint8_t* pBuffer, unsigned len, uint32_t* t_us);
static int Sh2HalWrite(sh2_Hal_t* self, uint8_t* pBuffer, unsigned len);
static uint32_t Sh2HalGetTimeUs(sh2_Hal_t* self);

// =================================================================================================
// LOCAL VARIABLES
// =================================================================================================
static TimerSrv* timer_;
static DsfLogger* logger_;
static FtdiHal* ftdiHal_;

static State state_ = State::Idle;

// Timestamp
static bool useSampleTime = false;
static double startTime = 0;
static double currTime = 0;
static double lastSampleTime = 0;

static uint64_t sensorEventsReceived_ = 0;

static sh2_Hal_t sh2Hal_ = {
	Sh2HalOpen, //
	Sh2HalClose,
	Sh2HalRead,
	Sh2HalWrite,
	Sh2HalGetTimeUs,
};

frsString_s bno080Frs_[] = {
        // { STATIC_CALIBRATION_AGM, "scd" },
        {NOMINAL_CALIBRATION, "nominal_scd"},
        {DYNAMIC_CALIBRATION, "dcd"},
        {STATIC_CALIBRATION_SRA, "sra_scd"},
        {NOMINAL_CALIBRATION_SRA, "sra_nominal_scd"},

        {ME_POWER_MGMT, "motion_engine_power_management"},
        {SYSTEM_ORIENTATION, "system_orientation"},
        {ACCEL_ORIENTATION, "accelerometer_orientation"},
        {SCREEN_ACCEL_ORIENTATION, "sra_accelerometer_orientation"},
        {GYROSCOPE_ORIENTATION, "gyroscope_orientation"},
        {MAGNETOMETER_ORIENTATION, "magnetometer_orientation"},
        {ARVR_STABILIZATION_RV, "arvr_rotation_vector"},
        {ARVR_STABILIZATION_GRV, "arvr_game_rotation_vector"},

        // {TAP_DETECT_CONFIG, "tap_detector_configuration"},
        {SIG_MOTION_DETECT_CONFIG, "significant_motion_detector_configuration"},
        {SHAKE_DETECT_CONFIG, "shake_detector_configuration"},
        {MAX_FUSION_PERIOD, "maximum_fusion_period"},
        {SERIAL_NUMBER, "serial_number"},
        {ES_PRESSURE_CAL, "pressure_calibration"},
        {ES_TEMPERATURE_CAL, "temperature_calibration"},
        {ES_HUMIDITY_CAL, "humidity_calibration"},
        {ES_AMBIENT_LIGHT_CAL, "ambient_light_calibration"},
        {ES_PROXIMITY_CAL, "proximity_calibration"},
        {ALS_CAL, "ambient_light_special_calibration"},
        {PROXIMITY_SENSOR_CAL, "proximity_special_calibration"},
        {PICKUP_DETECTOR_CONFIG, "pickup_detector_configuration"},
        {FLIP_DETECTOR_CONFIG, "flip_detector_configuration"},
        {STABILITY_DETECTOR_CONFIG, "stability_detector_configuration"},
        {ACTIVITY_TRACKER_CONFIG, "activity_tracker_configuration"},
        {SLEEP_DETECTOR_CONFIG, "sleep_detector_configuration"},
        {TILT_DETECTOR_CONFIG, "tilt_detector_configuration"},
        {POCKET_DETECTOR_CONFIG, "pocket_detector_configuration"},
        {CIRCLE_DETECTOR_CONFIG, "circle_detector_configuration"},
        {USER_RECORD, "user_record"},
        {ME_TIME_SOURCE_SELECT, "motion_engine_time_source_selection"},
        {UART_FORMAT, "uart_output_format_selection"},
        {GYRO_INTEGRATED_RV_CONFIG, "gyro_integrated_rotation_vector_configuration"},
};


// =================================================================================================
// LOCAL FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// myEventCallback
// -------------------------------------------------------------------------------------------------
void myEventCallback(void* cookie, sh2_AsyncEvent_t* pEvent) {

    switch (state_) {
        case State::Reset:
            if (pEvent->eventId == SH2_RESET) {
                std::cout << "\nINFO: Reset Complete" << std::endl;
                state_ = State::Startup;
            }
            break;
        default:
            break;
    }

    // Log ansync events
    logger_->logAsyncEvent(pEvent, currTime);
}

// -------------------------------------------------------------------------------------------------
// mySensorCallback
// -------------------------------------------------------------------------------------------------
void mySensorCallback(void* cookie, sh2_SensorEvent_t* pEvent) {
    sh2_SensorValue_t value;
    sh2_decodeSensorEvent(&value, pEvent);

    if (useSampleTime) {
        switch (value.sensorId) {
        case SH2_RAW_ACCELEROMETER:
            currTime = value.un.rawAccelerometer.timestamp * 1e-6;
            lastSampleTime = currTime;
            break;
        case SH2_RAW_GYROSCOPE:
            currTime = value.un.rawGyroscope.timestamp * 1e-6;
            lastSampleTime = currTime;
            break;
        case SH2_RAW_MAGNETOMETER:
            currTime = value.un.rawMagnetometer.timestamp * 1e-6;
            lastSampleTime = currTime;
            break;
        default:
            currTime = lastSampleTime;
            break;
        }
    } else {
        currTime = value.timestamp * 1e-6;
    }

    if (startTime == 0) {
        startTime = currTime;
    }
    ++sensorEventsReceived_;

    // Log sensor data
    logger_->logSensorValue(&value, currTime);
}


// =================================================================================================
// PUBLIC FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// LoggerApp::init
// -------------------------------------------------------------------------------------------------
int LoggerApp::init(appConfig_s* appConfig, TimerSrv* timer, FtdiHal* ftdiHal, DsfLogger* logger) {
    int status;

    timer_ = timer;
    ftdiHal_ = ftdiHal;
    logger_ = logger;

    // Open SH2/SHTP connection
    state_ = State::Reset;
    std::cout << "INFO: Open a session with a SensorHub \n";
    status = sh2_open(&sh2Hal_, myEventCallback, NULL);
    if (status != SH2_OK) {
        std::cout << "ERROR: Failed to open a SensorHub session : " << status << "\n";
        return -1;
    }

    int retryCnt = 0;
    while (true) {
		if (!WaitForResetComplete(RESET_TIMEOUT_)) {
            std::cout << "ERROR: Reset timeout. Retry ..\n";

            if (retryCnt++ >= 3) {
                return 1;
            }
            ftdiHal_->softreset();
        } else {
            break;
        }
    }

    // Set callback for Sensor Data
    status = sh2_setSensorCallback(mySensorCallback, NULL);
    if (status != SH2_OK) {
        std::cout << "ERROR: Failed to set Sensor callback\n";
        return -1;
    }

    if (appConfig->clearDcd) {
        std::cout << "INFO: Clear DCD and Reset\n";
        uint32_t dummy = 0;
        sh2_setFrs(DYNAMIC_CALIBRATION, &dummy, 0);

        state_ = State::Reset;
        sh2_clearDcdAndReset();
        if (!WaitForResetComplete(RESET_TIMEOUT_)) {
            std::cout << "ERROR: Failed to reset a SensorHub - Timeout \n";
            return -1;
        }
    }

    // Get Product IDs
    std::cout << "INFO: Get Product IDs\n";
    sh2_ProductIds_t productIds;
    status = sh2_getProdIds(&productIds);
	if (status != SH2_OK) {
        std::cout << "ERROR: Failed to get product IDs\n";
        return -1;
    }
    logger_->logProductIds(productIds);

    // Set DCD Auto Save
    std::cout << "INFO: Set DCD Auto Save\n";
    status = sh2_setDcdAutoSave(appConfig->dcdAutoSave);
    if (status != SH2_OK) {
        std::cout << "ERROR: Failed to set DCD Auto Save\n";
        return -1;
    }

    // Set Calibration Config
    std::cout << "INFO: Set Calibration Config\n";
    status = sh2_setCalConfig(appConfig->calEnableMask);
    if (status != SH2_OK) {
        std::cout << "ERROR: Failed to set calibration config\n";
        return -1;
    }

    // Get Device FRS records
    std::cout << "INFO: Get FRS Records\n";
    LogAllFrsBNO080();

    // Enable Sensors

    // Check if a config file is specified.
    if (appConfig->config) {
        ProcessConfigFile(&sensorsToEnable_, appConfig);
    }

    // Update the list of enabled sensors based on the mode and options
    if (!appConfig->config) {
        UpdateSensorList(&sensorsToEnable_, appConfig);
    }

    // Enable Sensors
    uint32_t reportInterval_us;
    reportInterval_us = static_cast<uint32_t>((1e6 / appConfig->rate) + 0.5);

    std::cout << "INFO: Enable Sensors\n";
    sh2_SensorConfig_t config;
    for (SensorList_t::iterator it = sensorsToEnable_.begin(); it != sensorsToEnable_.end(); ++it) {
        GetSensorConfiguration(*it, &config);
        config.reportInterval_us = reportInterval_us;
        std::cout << "INFO: Sensor ID : " << *it << "\n";
        sh2_setSensorConfig(*it, &config);
    }
    
    // Enable Activity Classify 
    if (appConfig->pac) {
        GetSensorConfiguration(SH2_PERSONAL_ACTIVITY_CLASSIFIER, &config);
        config.reportInterval_us = reportInterval_us;
        std::cout << "INFO: Sensor ID : " << SH2_PERSONAL_ACTIVITY_CLASSIFIER << "\n";
        sh2_setSensorConfig(SH2_PERSONAL_ACTIVITY_CLASSIFIER, &config);
    }

    // Enable Step Detector
    if (appConfig->step) {
        GetSensorConfiguration(SH2_STEP_DETECTOR, &config);
        config.reportInterval_us = reportInterval_us;
        std::cout << "INFO: Sensor ID : " << SH2_STEP_DETECTOR << "\n";
        sh2_setSensorConfig(SH2_STEP_DETECTOR, &config);
    }

    firstReportReceived_ = false;
    state_ = State::Run;

    return 0;
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::service
// -------------------------------------------------------------------------------------------------
int LoggerApp::service() {

    if (!firstReportReceived_) {
        firstReportReceived_ = true;
        lastReportTime_us_ = timer_->getTimestamp_us();
    }

    curReportTime_us_ = timer_->getTimestamp_us();

    if (curReportTime_us_ - lastReportTime_us_ >= 1000000) {
        double deltaT = currTime - startTime;
        int32_t h = static_cast<int32_t>(floor(deltaT / 60.0 / 60.0));
        int32_t m = static_cast<int32_t>(floor((deltaT - h * 60 * 60) / 60.0));
        double s = deltaT - h * 60 * 60 - m * 60;

        std::cout << "Samples: " << std::setfill(' ') << std::setw(10) << sensorEventsReceived_
                  << " Duration: " << h << ":" << std::setw(2) << std::setfill('0') << m << ":"
                  << std::setprecision(2) << s << " "
                  << " Rate: " << std::fixed << std::setprecision(2)
                  << sensorEventsReceived_ / deltaT << " Hz" << std::endl;

        lastReportTime_us_ = curReportTime_us_;
    }

    sh2_service();
    return 1;
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::finish
// -------------------------------------------------------------------------------------------------
int LoggerApp::finish() {

    // Turn off sensors
    std::cout << "INFO: Disable Sensors" << std::endl;
    sh2_SensorConfig_t config;
    memset(&config, 0, sizeof(config));
    config.reportInterval_us = 0;
    for (SensorList_t::iterator it = sensorsToEnable_.begin(); it != sensorsToEnable_.end(); ++it) {
        sh2_setSensorConfig(*it, &config);
    }

    /*
    Sleep(100);
    logFrs(DYNAMIC_CALIBRATION, "dcdPre_Reset");

    sh2_reinitialize();

    logFrs(DYNAMIC_CALIBRATION, "dcdPostReset");
    */

    std::cout << "INFO: Closing the SensorHub session" << std::endl;
    sh2_close();
    logger_->finish();

    std::cout << "INFO: Shutdown complete" << std::endl;

    return 1;
}


// =================================================================================================
// LOCAL FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// Sh2HalOpen
// -------------------------------------------------------------------------------------------------
static int Sh2HalOpen(sh2_Hal_t* self) {
    return ftdiHal_->open();
}

// -------------------------------------------------------------------------------------------------
// Sh2HalClose
// -------------------------------------------------------------------------------------------------
static void Sh2HalClose(sh2_Hal_t* self) {
    ftdiHal_->close();
}

// -------------------------------------------------------------------------------------------------
// Sh2HalRead
// -------------------------------------------------------------------------------------------------
static int Sh2HalRead(sh2_Hal_t* self, uint8_t* pBuffer, unsigned len, uint32_t* t_us) {
    return ftdiHal_->read(pBuffer, len, t_us);
}

// -------------------------------------------------------------------------------------------------
// Sh2HalWrite
// -------------------------------------------------------------------------------------------------
static int Sh2HalWrite(sh2_Hal_t* self, uint8_t* pBuffer, unsigned len) {
    return ftdiHal_->write(pBuffer, len);
}

// -------------------------------------------------------------------------------------------------
// Sh2HalGetTimeUs
// -------------------------------------------------------------------------------------------------
static uint32_t Sh2HalGetTimeUs(sh2_Hal_t* self) {
    return (uint32_t)timer_->getTimestamp_us();
}


// =================================================================================================
// PRIVATE METHODS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// LoggerApp::WaitForResetComplete
// -------------------------------------------------------------------------------------------------
bool LoggerApp::WaitForResetComplete(int loops) {
    std::cout << "INFO: Waiting for System Reset ";

    for (int resetTime = 0; state_ == State::Reset; ++resetTime) {
		// if (resetTime % 100 == 99) {
		if (resetTime % 10000 == 9999) {
			std::cout << ".";
        }
        if (resetTime >= loops) {
            std::cout << "\n";
            return false;
        }
        sh2_service();
    }
    return true;
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::ProcessConfigFile
// -------------------------------------------------------------------------------------------------
void LoggerApp::ProcessConfigFile(SensorList_t* sensorsToEnable, LoggerApp::appConfig_s* pConfig) {
    std::ifstream infile("sensorlist.cfg");
    if (infile.is_open()) {
        std::cout << "INFO: Extract Sensor list from sensorlist.cfg\n";

        sh2_SensorId_t sensorId;
        int id;
        while (infile >> id) {
            sensorId = (sh2_SensorId_t)id;
            if (sensorId <= SH2_MAX_SENSOR_ID) {
                std::cout << "INFO: (.cfg) Sensor ID " << id << " \n";

                if (sensorId == SH2_PERSONAL_ACTIVITY_CLASSIFIER) {
                    pConfig->pac = true;
                } else if (sensorId == SH2_STEP_DETECTOR) {
                    pConfig->step = true;
                } else {
                    sensorsToEnable_.push_back(sensorId);
                }
            }
        }
        sensorsToEnable_.sort();
        sensorsToEnable_.unique();

    } else {
        pConfig->config = false;
        std::cout << "WARNING: sensorList.cfg is NOT found.\n";
    }
    infile.close();
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::UpdateSensorList
// -------------------------------------------------------------------------------------------------
void LoggerApp::UpdateSensorList(SensorList_t* sensorsToEnable, LoggerApp::appConfig_s* pConfig) {

    if (pConfig->outputRaw) {
        useSampleTime = true;
        switch (pConfig->sensorMode) {
            default:
            case SENSOR_MODE_9AGM:
            case SENSOR_MODE_ALL:
                sensorsToEnable->push_back(SH2_RAW_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_RAW_GYROSCOPE);
                sensorsToEnable->push_back(SH2_RAW_MAGNETOMETER);
                break;
            case SENSOR_MODE_6AG:
                sensorsToEnable->push_back(SH2_RAW_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_RAW_GYROSCOPE);
                break;
            case SENSOR_MODE_6AM:
                sensorsToEnable->push_back(SH2_RAW_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_RAW_MAGNETOMETER);
                break;
            case SENSOR_MODE_6GM:
                sensorsToEnable->push_back(SH2_RAW_GYROSCOPE);
                sensorsToEnable->push_back(SH2_RAW_MAGNETOMETER);
                break;
            case SENSOR_MODE_3A:
                sensorsToEnable->push_back(SH2_RAW_ACCELEROMETER);
                break;
            case SENSOR_MODE_3G:
                sensorsToEnable->push_back(SH2_RAW_GYROSCOPE);
                break;
            case SENSOR_MODE_3M:
                sensorsToEnable->push_back(SH2_RAW_MAGNETOMETER);
                break;
        }
    }

    if (pConfig->outputCalibrated) {
        switch (pConfig->sensorMode) {
            default:
            case SENSOR_MODE_9AGM:
                sensorsToEnable->push_back(SH2_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_GYROSCOPE_CALIBRATED);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_CALIBRATED);
                break;
            case SENSOR_MODE_ALL:
                sensorsToEnable->push_back(SH2_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_GYROSCOPE_CALIBRATED);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_CALIBRATED);
                sensorsToEnable->push_back(SH2_LINEAR_ACCELERATION);
                sensorsToEnable->push_back(SH2_GRAVITY);
                break;
            case SENSOR_MODE_6AG:
                sensorsToEnable->push_back(SH2_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_GYROSCOPE_CALIBRATED);
                break;
            case SENSOR_MODE_6AM:
                sensorsToEnable->push_back(SH2_ACCELEROMETER);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_CALIBRATED);
                break;
            case SENSOR_MODE_6GM:
                sensorsToEnable->push_back(SH2_GYROSCOPE_CALIBRATED);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_CALIBRATED);
                break;
            case SENSOR_MODE_3A:
                sensorsToEnable->push_back(SH2_ACCELEROMETER);
                break;
            case SENSOR_MODE_3G:
                sensorsToEnable->push_back(SH2_GYROSCOPE_CALIBRATED);
                break;
            case SENSOR_MODE_3M:
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_CALIBRATED);
                break;
        }
    }

    if (pConfig->outputUncalibrated) {
        switch (pConfig->sensorMode) {
            default:
            case SENSOR_MODE_9AGM:
            case SENSOR_MODE_ALL:
                sensorsToEnable->push_back(SH2_GYROSCOPE_UNCALIBRATED);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_UNCALIBRATED);
                break;
            case SENSOR_MODE_6AG:
                sensorsToEnable->push_back(SH2_GYROSCOPE_UNCALIBRATED);
                break;
            case SENSOR_MODE_6AM:
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_UNCALIBRATED);
                break;
            case SENSOR_MODE_6GM:
                sensorsToEnable->push_back(SH2_GYROSCOPE_UNCALIBRATED);
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_UNCALIBRATED);
                break;
            case SENSOR_MODE_3A:
                break;
            case SENSOR_MODE_3G:
                sensorsToEnable->push_back(SH2_GYROSCOPE_UNCALIBRATED);
                break;
            case SENSOR_MODE_3M:
                sensorsToEnable->push_back(SH2_MAGNETIC_FIELD_UNCALIBRATED);
                break;
        }
    }

    switch (pConfig->sensorMode) {
        default:
        case SENSOR_MODE_9AGM:
            sensorsToEnable->push_back(SH2_ROTATION_VECTOR);
            break;
        case SENSOR_MODE_6AG:
            sensorsToEnable->push_back(SH2_GAME_ROTATION_VECTOR);
            break;
        case SENSOR_MODE_6AM:
            sensorsToEnable->push_back(SH2_GEOMAGNETIC_ROTATION_VECTOR);
            break;
        case SENSOR_MODE_ALL:
            sensorsToEnable->push_back(SH2_ROTATION_VECTOR);
            sensorsToEnable->push_back(SH2_GAME_ROTATION_VECTOR);
            sensorsToEnable->push_back(SH2_GEOMAGNETIC_ROTATION_VECTOR);
        case SENSOR_MODE_6GM:
        case SENSOR_MODE_3A:
        case SENSOR_MODE_3G:
        case SENSOR_MODE_3M:
            break;
    }
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::GetSensorConfiguration
// -------------------------------------------------------------------------------------------------
void LoggerApp::GetSensorConfiguration(sh2_SensorId_t sensorId, sh2_SensorConfig_t* pConfig) {
    memset(pConfig, 0, sizeof(sh2_SensorConfig_t));

    if (sensorId == SH2_PERSONAL_ACTIVITY_CLASSIFIER) {
        pConfig->sensorSpecific = 511;

    } else if (sensorId == SH2_STEP_DETECTOR) {
        pConfig->changeSensitivityEnabled = true;
    }
}


// -------------------------------------------------------------------------------------------------
// LoggerApp::LogFrs
// -------------------------------------------------------------------------------------------------
int LoggerApp::LogFrs(uint16_t recordId, char const* name) {
    uint32_t buffer[1024];
    memset(buffer, 0xAA, sizeof(buffer));
    uint16_t words = sizeof(buffer) / 4;
    int status = sh2_getFrs(recordId, buffer, &words);
    if (status != 0) {
        return 0;
    }
    if (words > 0) {
        logger_->logFrsRecord(name, buffer, words);
    }
    return words;
}

// -------------------------------------------------------------------------------------------------
// LoggerApp::LogAllFrsBNO080
// -------------------------------------------------------------------------------------------------
void LoggerApp::LogAllFrsBNO080() {
    if (LogFrs(STATIC_CALIBRATION_AGM, "scd") == 0) {
        logger_->logMessage("# No SCD present, logging nominal calibration as 'scd'.");
        LogFrs(NOMINAL_CALIBRATION, "scd");
    }

    frsString_s* pFrs;
    for (int i = 0; i < sizeof(bno080Frs_) / sizeof(frsString_s); i++) {
        pFrs = &bno080Frs_[i];
        LogFrs(pFrs->recordId, pFrs->name);
    }
}
