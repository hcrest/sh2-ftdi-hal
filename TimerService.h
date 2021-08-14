/*
 * Copyright 2017-21 CEVA, Inc.
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

#ifndef TIMER_SERVICE_H
#define TIMER_SERVICE_H

#include <stdint.h>


// =================================================================================================
// CLASS DEFINITON - TimerSrv
// =================================================================================================
class TimerSrv {
public:
	TimerSrv() : initialTimeUs_(0){};
	~TimerSrv() {};

    virtual void init() = 0;
	virtual uint64_t getTimestamp_us() = 0;

protected:
    uint64_t initialTimeUs_;
};


// =================================================================================================
// CLASS DEFINITON - TimerSrvWin
// =================================================================================================
class TimerSrvWin : public TimerSrv {
public:
	TimerSrvWin() {};
	~TimerSrvWin() {};

	virtual void init();
	virtual uint64_t getTimestamp_us();

};


// =================================================================================================
// CLASS DEFINITON - TimerSrvRpi
// =================================================================================================
class TimerSrvRpi : public TimerSrv {
public:
	TimerSrvRpi() {};
	~TimerSrvRpi() {};

	virtual void init();
	virtual uint64_t getTimestamp_us();

};

#endif // TIMER_SERVICE_H
