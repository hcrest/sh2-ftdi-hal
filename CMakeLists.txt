#
# FTDI hal
#
cmake_minimum_required(VERSION 2.8)
project(sh2_ftdi_hal)

#list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_FLAGS "-std=gnu99")
set(CMAKE_CXX_STANDARD 11)
	
# Be able to find our generated header file.
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/sh2")

link_directories("${CMAKE_CURRENT_SOURCE_DIR}")


# Build 
if(WIN32)
add_library(sh2_ftdi_hal
    FtdiHalWin.cpp
	FtdiHal.cpp
	TimerServiceWin.cpp
	ftd2xx.h
	Rfc1662Framer.cpp
	sh2/sh2.c
	sh2/sh2_SensorValue.c
	sh2/sh2_util.c
	sh2/shtp.c
	)
else()
add_library(sh2_ftdi_hal
    FtdiHalRpi.cpp
	FtdiHal.cpp
	TimerServiceRpi.cpp
	ftd2xx.h
	Rfc1662Framer.cpp
	sh2/sh2.c
	sh2/sh2_SensorValue.c
	sh2/sh2_util.c
	sh2/shtp.c
	)
endif()

if(WIN32)
	if(CMAKE_CL_64)
		set(LIBRARIES ${LIBRARIES} x64/ftd2xx)
	else()
		set(LIBRARIES ${LIBRARIES} Win32/ftd2xx)
	endif()

else()
	set(LIBRARIES ${LIBRARIES} 
	pthread)
endif()
	
# Link 
target_link_libraries(sh2_ftdi_hal ${LIBRARIES})


