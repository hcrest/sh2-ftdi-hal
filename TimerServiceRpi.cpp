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

#include "TimerService.h"

#ifdef _WIN32
#include <Windows.h>

#else
#include <time.h>
#endif


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

#ifdef _WIN32
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
    // DWORD_PTR oldmask = SetThreadAffinityMask(GetCurrentTh), 1);
    QueryPerformanceCounter((LARGE_INTEGER*)&counterTime);
    // SetThreadAffinityMask(GetCurrentTh), oldmask);
    t_us = (uint64_t)(counterTime * timerFrequency_ * 1000000);

    if (initialTimeUs_ == 0) {
        initialTimeUs_ = t_us;
    }
    t_us -= initialTimeUs_;

    return t_us;
}
#endif


// =================================================================================================
// PUBLIC FUNCTIONS - TimerSrvRpi
// =================================================================================================
// -------------------------------------------------------------------------------------------------
// TimerSrvRpi::init
// -------------------------------------------------------------------------------------------------
void TimerSrvRpi::init() {
	initialTimeUs_ = 0;
}

// -------------------------------------------------------------------------------------------------
// TimerSrvRpi::getTimestamp_us
// -------------------------------------------------------------------------------------------------
uint64_t TimerSrvRpi::getTimestamp_us() {
	uint64_t t_us = 0;

	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	t_us = ((uint32_t)tp.tv_sec * 1000000) + ((uint32_t)tp.tv_nsec / 1000.0);

	if (initialTimeUs_ == 0) {
		initialTimeUs_ = t_us;
	}
	t_us -= initialTimeUs_;

	return t_us;
}
