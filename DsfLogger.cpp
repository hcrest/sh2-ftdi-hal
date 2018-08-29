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

#include "DsfLogger.h"
#include <iomanip>


// =================================================================================================
// CLASS DEFINITON - SampleIdExtender
// =================================================================================================
class SampleIdExtender {
public:
    SampleIdExtender() : empty_(true), seqMsb_(0), seqLsb_(0) {
    }

    uint64_t extend(uint8_t seq) {
        empty_ = false;
        if (seq < seqLsb_) {
            ++seqMsb_;
        }
        seqLsb_ = seq;
        return (seqMsb_ << 8) | seqLsb_;
    }

    uint64_t increment() {
        empty_ = false;
        return ++seqMsb_;
    }

    bool isEmpty() {
        return empty_;
    }

private:
    bool empty_;
    uint64_t seqMsb_;
    uint8_t seqLsb_;
};


// =================================================================================================
// PUBLIC FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// DsfLogger::init
// -------------------------------------------------------------------------------------------------
bool DsfLogger::init(char const* filePath, bool ned) {
    outFile_.open(filePath);
    orientationNed_ = ned;
    return (outFile_) ? true : false;
}

// -------------------------------------------------------------------------------------------------
// DsfLogger::finish
// -------------------------------------------------------------------------------------------------
void DsfLogger::finish() {
    if (outFile_) {
        outFile_.flush();
        outFile_.close();
    }
}

// -------------------------------------------------------------------------------------------------
// DsfLogger::logMessage
// -------------------------------------------------------------------------------------------------
void DsfLogger::logMessage(char const* msg) {
    outFile_ << msg << std::endl;
}

// -------------------------------------------------------------------------------------------------
// DsfLogger::logAsyncEvent
// -------------------------------------------------------------------------------------------------
void DsfLogger::logAsyncEvent(sh2_AsyncEvent_t* pEvent, double currTime) {

    outFile_ << "$ ";
    outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
    outFile_.unsetf(std::ios_base::floatfield);
    switch (pEvent->eventId) {
        case SH2_RESET:
            outFile_ << " reset(1)\n";
            break;
        case SH2_FRS_CHANGE:
            outFile_ << " frsChange(0x" << std::hex << pEvent->frsType << std::dec << ")\n";
            break;
        default:
            outFile_ << " unknownEvent(" << pEvent->eventId << ")\n";
            break;
    }
}


// -------------------------------------------------------------------------------------------------
// DsfLogger::logProductIds
// -------------------------------------------------------------------------------------------------
void DsfLogger::logProductIds(sh2_ProductIds_t ids) {
    for (uint32_t i = 0; i < ids.numEntries; ++i) {
        switch (ids.entry[i].resetCause) {
            default:
            case 0:
                break;
            case 1:
                outFile_ << "!RESET_CAUSE=\"PowerOnReset\"\n";
                break;
            case 2:
                outFile_ << "!RESET_CAUSE=\"InternalSystemReset\"\n";
                break;
            case 3:
                outFile_ << "!RESET_CAUSE=\"WatchdogTimeout\"\n";
                break;
            case 4:
                outFile_ << "!RESET_CAUSE=\"ExternalReset\"\n";
                break;
            case 5:
                outFile_ << "!RESET_CAUSE=\"Other\"\n";
                break;
        }
        outFile_ << "! PN." << i << "=\"";
        outFile_ << static_cast<uint32_t>(ids.entry[i].swPartNumber) << " ";
        outFile_ << static_cast<uint32_t>(ids.entry[i].swVersionMajor) << ".";
        outFile_ << static_cast<uint32_t>(ids.entry[i].swVersionMinor) << ".";
        outFile_ << static_cast<uint32_t>(ids.entry[i].swVersionPatch) << ".";
        outFile_ << static_cast<uint32_t>(ids.entry[i].swBuildNumber) << "\"\n";
    }
}

// -------------------------------------------------------------------------------------------------
// DsfLogger::logFrsRecord
// -------------------------------------------------------------------------------------------------
void DsfLogger::logFrsRecord(char const* name, uint32_t* buffer, uint16_t words) {
    outFile_ << "!" << name << "=\"";
    for (uint16_t w = 0; w < words; ++w) {
        for (uint8_t b = 0; b < 4; ++b) {
            outFile_ << std::hex << std::setw(2) << std::setfill('0')
                     << ((buffer[w] >> (b * 8)) & 0xFF);
            if (w != (words - 1) || b != 3) {
                outFile_ << ",";
            }
        }
    }
    outFile_ << std::dec;
    outFile_ << "\"\n";
}

// -------------------------------------------------------------------------------------------------
// DsfLogger::logSensorValue
// -------------------------------------------------------------------------------------------------
void DsfLogger::logSensorValue(sh2_SensorValue_t* pValue, double currTime) {
    uint32_t id = pValue->sensorId;

    switch (id) {

        case SH2_PERSONAL_ACTIVITY_CLASSIFIER: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                    "MOST_LIKELY_STATE[x]{state},CONFIDENCE[uvbfstwrax]{state},STATUS[x]",
                    "PersonalActivityClassifier", false);
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";

            outFile_ << static_cast<uint32_t>(pValue->un.personalActivityClassifier.mostLikelyState) << ",";
            for (int i = 0; i < 10; i++) {
                outFile_ << static_cast<uint32_t>(pValue->un.personalActivityClassifier.confidence[i]) << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_STEP_DETECTOR: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                    "STEP_DETECTOR_LATENCY[x]{us},STATUS[x]",
                    "StepDetector", false);
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            outFile_ << pValue->un.stepDetector.latency << ",";
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_STEP_COUNTER: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                    "STEPS[x]{steps}, STEP_COUNTER_LATENCY[x]{us},STATUS[x]",
                    "StepCounter", false);
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            outFile_ << pValue->un.stepCounter.steps << ",";
            outFile_ << pValue->un.stepCounter.latency << ",";
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_ROTATION_VECTOR: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                          "ANG_POS_GLOBAL[wxyz]{quaternion},ANG_POS_ACCURACY[x]{deg},STATUS[x]",
                          "RotationVector");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.rotationVector.real << ",";
                outFile_ << pValue->un.rotationVector.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.rotationVector.i << ",";
                outFile_ << -pValue->un.rotationVector.k << ",";
            } else {
                outFile_ << pValue->un.rotationVector.real << ",";
                outFile_ << pValue->un.rotationVector.i << ",";
                outFile_ << pValue->un.rotationVector.j << ",";
                outFile_ << pValue->un.rotationVector.k << ",";
            }
            outFile_ << (pValue->un.rotationVector.accuracy * 180.0 / PI) << ",";
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }
        case SH2_GYRO_INTEGRATED_RV: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                          "ANG_POS_GLOBAL[wxyz]{quaternion},ANG_VEL[xyz]{rad/s}",
                          "GyroIntegratedRV");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.gyroIntegratedRV.real << ",";
                outFile_ << pValue->un.gyroIntegratedRV.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.gyroIntegratedRV.i << ",";
                outFile_ << -pValue->un.gyroIntegratedRV.k << ",";
                outFile_ << pValue->un.gyroIntegratedRV.angVelY << ",";
                outFile_ << pValue->un.gyroIntegratedRV.angVelX << ",";
                outFile_ << -pValue->un.gyroIntegratedRV.angVelZ;
            } else {
                outFile_ << pValue->un.gyroIntegratedRV.real << ",";
                outFile_ << pValue->un.gyroIntegratedRV.i << ",";
                outFile_ << pValue->un.gyroIntegratedRV.j << ",";
                outFile_ << pValue->un.gyroIntegratedRV.k << ",";
                outFile_ << pValue->un.gyroIntegratedRV.angVelX << ",";
                outFile_ << pValue->un.gyroIntegratedRV.angVelY << ",";
                outFile_ << pValue->un.gyroIntegratedRV.angVelZ;
            }
            outFile_ << "\n";
            break;
        }
        case SH2_GAME_ROTATION_VECTOR: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "ANG_POS_GLOBAL[wxyz]{quaternion}", "GameRotationVector");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.gameRotationVector.real << ",";
                outFile_ << pValue->un.gameRotationVector.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.gameRotationVector.i << ",";
                outFile_ << -pValue->un.gameRotationVector.k;
            } else {
                outFile_ << pValue->un.gameRotationVector.real << ",";
                outFile_ << pValue->un.gameRotationVector.i << ",";
                outFile_ << pValue->un.gameRotationVector.j << ",";
                outFile_ << pValue->un.gameRotationVector.k;
            }
            outFile_ << "\n";
            break;
        }
        case SH2_GEOMAGNETIC_ROTATION_VECTOR: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                          "ANG_POS_GLOBAL[wxyz]{quaternion},ANG_POS_ACCURACY[x]{deg},STATUS[x]",
                          "GeomagneticRotationVector");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.geoMagRotationVector.real << ",";
                outFile_ << pValue->un.geoMagRotationVector.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.geoMagRotationVector.i << ",";
                outFile_ << -pValue->un.geoMagRotationVector.k << ",";
            } else {
                outFile_ << pValue->un.geoMagRotationVector.real << ",";
                outFile_ << pValue->un.geoMagRotationVector.i << ",";
                outFile_ << pValue->un.geoMagRotationVector.j << ",";
                outFile_ << pValue->un.geoMagRotationVector.k << ",";
            }
            outFile_ << (pValue->un.geoMagRotationVector.accuracy * 180.0 / PI) << ",";
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_RAW_ACCELEROMETER: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "LIN_ACC_GRAVITY[xyz]{ADC}", "RawAccelerometer");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            outFile_ << pValue->un.rawAccelerometer.x << ",";
            outFile_ << pValue->un.rawAccelerometer.y << ",";
            outFile_ << pValue->un.rawAccelerometer.z << "\n";
            break;
        }
        case SH2_RAW_GYROSCOPE: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "ANG_VEL[xyz]{ADC},TEMPERATURE[x]{ADC}", "RawGyroscope");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            outFile_ << pValue->un.rawGyroscope.x << ",";
            outFile_ << pValue->un.rawGyroscope.y << ",";
            outFile_ << pValue->un.rawGyroscope.z << ",";
            outFile_ << pValue->un.rawGyroscope.temperature << "\n";
            break;
        }
        case SH2_RAW_MAGNETOMETER: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "MAG[xyz]{ADC}", "RawMagnetometer");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            outFile_ << pValue->un.rawMagnetometer.x << ",";
            outFile_ << pValue->un.rawMagnetometer.y << ",";
            outFile_ << pValue->un.rawMagnetometer.z << "\n";
            break;
        }
        case SH2_ACCELEROMETER: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "LIN_ACC_GRAVITY[xyz]{m/s^2},STATUS[x]", "Accelerometer");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.accelerometer.y << ","; // ENU -> NED
                outFile_ << pValue->un.accelerometer.x << ",";
                outFile_ << -pValue->un.accelerometer.z << ",";
            } else {
                outFile_ << pValue->un.accelerometer.x << ",";
                outFile_ << pValue->un.accelerometer.y << ",";
                outFile_ << pValue->un.accelerometer.z << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }
        case SH2_GYROSCOPE_UNCALIBRATED: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "ANG_VEL[xyz]{rad/s},BIAS[xyz]{rad/s}", "UncalibratedGyroscope");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.gyroscopeUncal.y << ","; // ENU -> NED
                outFile_ << pValue->un.gyroscopeUncal.x << ",";
                outFile_ << -pValue->un.gyroscopeUncal.z << ",";
                outFile_ << pValue->un.gyroscopeUncal.biasY << ","; // ENU -> NED
                outFile_ << pValue->un.gyroscopeUncal.biasX << ",";
                outFile_ << -pValue->un.gyroscopeUncal.biasZ;
            } else {
                outFile_ << pValue->un.gyroscopeUncal.x << ",";
                outFile_ << pValue->un.gyroscopeUncal.y << ",";
                outFile_ << pValue->un.gyroscopeUncal.z << ",";
                outFile_ << pValue->un.gyroscopeUncal.biasX << ",";
                outFile_ << pValue->un.gyroscopeUncal.biasY << ",";
                outFile_ << pValue->un.gyroscopeUncal.biasZ;
            }
            outFile_ << "\n";
            break;
        }
        case SH2_GYROSCOPE_CALIBRATED: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "ANG_VEL[xyz]{rad/s}", "Gyroscope");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.gyroscope.y << ","; // ENU -> NED
                outFile_ << pValue->un.gyroscope.x << ",";
                outFile_ << -pValue->un.gyroscope.z;
            } else {
                outFile_ << pValue->un.gyroscope.x << ",";
                outFile_ << pValue->un.gyroscope.y << ",";
                outFile_ << pValue->un.gyroscope.z;
            }
            outFile_ << "\n";
            break;
        }
        case SH2_MAGNETIC_FIELD_CALIBRATED: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "MAG[xyz]{m/s^2},STATUS[x]", "MagneticField");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.magneticField.y << ","; // ENU -> NED
                outFile_ << pValue->un.magneticField.x << ",";
                outFile_ << -pValue->un.magneticField.z << ",";
            } else {
                outFile_ << pValue->un.magneticField.x << ",";
                outFile_ << pValue->un.magneticField.y << ",";
                outFile_ << pValue->un.magneticField.z << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }
        case SH2_MAGNETIC_FIELD_UNCALIBRATED: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                          "MAG_UNCAL[xyz]{m/s^2},MAG_BAIS[xyz]{m/s^2},STATUS[x]",
                          "UncalibratedMagField");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.magneticFieldUncal.y << ","; // ENU -> NED
                outFile_ << pValue->un.magneticFieldUncal.x << ",";
                outFile_ << -pValue->un.magneticFieldUncal.z << ",";
                outFile_ << pValue->un.magneticFieldUncal.biasY << ","; // ENU -> NED
                outFile_ << pValue->un.magneticFieldUncal.biasX << ",";
                outFile_ << -pValue->un.magneticFieldUncal.biasZ << ",";
            } else {
                outFile_ << pValue->un.magneticFieldUncal.x << ",";
                outFile_ << pValue->un.magneticFieldUncal.y << ",";
                outFile_ << pValue->un.magneticFieldUncal.z << ",";
                outFile_ << pValue->un.magneticFieldUncal.biasX << ",";
                outFile_ << pValue->un.magneticFieldUncal.biasY << ",";
                outFile_ << pValue->un.magneticFieldUncal.biasZ << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_LINEAR_ACCELERATION: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "LIN_ACC[xyz]{m/s^2},STATUS[x]", "LinearAcceleration");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.linearAcceleration.y << ","; // ENU -> NED
                outFile_ << pValue->un.linearAcceleration.x << ",";
                outFile_ << -pValue->un.linearAcceleration.z << ",";
            } else {
                outFile_ << pValue->un.linearAcceleration.x << ",";
                outFile_ << pValue->un.linearAcceleration.y << ",";
                outFile_ << pValue->un.linearAcceleration.z << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_GRAVITY: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id, "GRAVITY[xyz]{m/s^2},STATUS[x]", "Gravity");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.gravity.y << ","; // ENU -> NED
                outFile_ << pValue->un.gravity.x << ",";
                outFile_ << -pValue->un.gravity.z << ",";
            } else {
                outFile_ << pValue->un.gravity.x << ",";
                outFile_ << pValue->un.gravity.y << ",";
                outFile_ << pValue->un.gravity.z << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_ARVR_STABILIZED_RV: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                    "ANG_POS_GLOBAL[wxyz]{quaternion},ANG_POS_ACCURACY[x]{deg},STATUS[x]",
                    "ARVRStabilizedRotationVector");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.arvrStabilizedRV.real << ",";
                outFile_ << pValue->un.arvrStabilizedRV.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.arvrStabilizedRV.i << ",";
                outFile_ << -pValue->un.arvrStabilizedRV.k << ",";
            } else {
                outFile_ << pValue->un.arvrStabilizedRV.real << ",";
                outFile_ << pValue->un.arvrStabilizedRV.i << ",";
                outFile_ << pValue->un.arvrStabilizedRV.j << ",";
                outFile_ << pValue->un.arvrStabilizedRV.k << ",";
            }
            outFile_ << (pValue->un.arvrStabilizedRV.accuracy * 180.0 / PI) << ",";
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        case SH2_ARVR_STABILIZED_GRV: {
            static SampleIdExtender extender;
            if (extender.isEmpty()) {
                LogHeader(id,
                    "ANG_POS_GLOBAL[wxyz]{quaternion},STATUS[x]",
                    "ARVRStabilizedGameRotationVector");
            }
            outFile_ << "." << id << " ";
            outFile_ << std::fixed << std::setprecision(9) << currTime << ",";
            outFile_.unsetf(std::ios_base::floatfield);
            outFile_ << extender.extend(pValue->sequence) << ",";
            if (orientationNed_) {
                outFile_ << pValue->un.arvrStabilizedGRV.real << ",";
                outFile_ << pValue->un.arvrStabilizedGRV.j << ","; // Convert ENU -> NED
                outFile_ << pValue->un.arvrStabilizedGRV.i << ",";
                outFile_ << -pValue->un.arvrStabilizedGRV.k << ",";
            } else {
                outFile_ << pValue->un.arvrStabilizedGRV.real << ",";
                outFile_ << pValue->un.arvrStabilizedGRV.i << ",";
                outFile_ << pValue->un.arvrStabilizedGRV.j << ",";
                outFile_ << pValue->un.arvrStabilizedGRV.k << ",";
            }
            outFile_ << static_cast<uint32_t>(pValue->status) << "\n";
            break;
        }

        default:
            break;
    }
}


// =================================================================================================
// PRIVATE FUNCTIONS
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// DsfLogger::LogHeader
// -------------------------------------------------------------------------------------------------
void DsfLogger::LogHeader(uint8_t sensorId, char const* fieldNames, char const* name, bool orientation) {
    outFile_ << "+" << static_cast<int32_t>(sensorId) << " TIME{s},SAMPLE_ID[x]," << fieldNames
        << "\n";
    if (orientation) {
        outFile_ << "!" << static_cast<int32_t>(sensorId) << " coordinate_system=";
        if (orientationNed_) {
            outFile_ << "\"NED\"\n";
        } else {
            outFile_ << "\"ENU\"\n";
        }
    }
    outFile_ << "!" << static_cast<int32_t>(sensorId) << " name=\"" << name << "\"\n";
}
