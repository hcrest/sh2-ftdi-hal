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

 // =================================================================================================
 // INCLUDE
 // =================================================================================================
#include "TimerService.h"
#include <Windows.h>


// =================================================================================================
// DEFINES AND MACROS
// =================================================================================================

// =================================================================================================
// DATA TYPES
// =================================================================================================

// =================================================================================================
// LOCAL VARIABLES
// =================================================================================================
static double timerFrequency_;

// =================================================================================================
// LOCAL FUNCTIONS PROTOTYPES
// =================================================================================================

// =================================================================================================
// PUBLIC FUNCTIONS - TimerSrvWin
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// TimerSrvWin::init
// -------------------------------------------------------------------------------------------------
void TimerSrvWin::init() {
    unsigned __int64 freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    timerFrequency_ = (1.0 / freq);

    initialTimeUs_ = 0;
}

// -------------------------------------------------------------------------------------------------
// TimerSrvWin::getTimestamp_us
// -------------------------------------------------------------------------------------------------
uint64_t TimerSrvWin::getTimestamp_us() {
    uint64_t t_us = 0;

    uint64_t counterTime;
    QueryPerformanceCounter((LARGE_INTEGER*)&counterTime);

	t_us = (uint64_t)(counterTime * timerFrequency_ * 1000000);

    if (initialTimeUs_ == 0) {
        initialTimeUs_ = t_us;
    }
    t_us -= initialTimeUs_;

    return t_us;
}
