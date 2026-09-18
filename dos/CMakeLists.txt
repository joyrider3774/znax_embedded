# MS-DOS build of Znax: the game's own sources with the PLATFORM_DOS platform
# (source/znax_embedded/PlatformDOS.*) and DJGPP (toolchain.cmake, C:/dos_dev by default, or the
# DOSDEV environment variable).
#
# With MSYS2's cmake and ninja, from this folder:
#   cmake -S . -B build -G Ninja
#   cmake --build build
# gives build/ZNAX.EXE, a 32 bit DOS program with the CWSDPMI host built into it, so it runs on a
# plain DOS or in DOSBox with nothing else alongside it.

cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(znax_dos C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# DOS takes eight characters and three, and the name is what the player types to start it
set(GAME_NAME ZNAX)

if(NOT EXISTS "${CMAKE_CXX_COMPILER}")
    message(FATAL_ERROR "DJGPP not found in ${DOSDEV}, pass -DDOSDEV=<folder>")
endif()

# the game's folder, next to this one unless a build copies this folder somewhere else
set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../source/znax_embedded" CACHE PATH "the game's source folder")

# where drawing goes, see SCREENBUFFER in PlatformDOS.h: 0 (straight into the frame), 1, 8 or 16.
# 8 is what the screen itself holds, so a frame reaches the card without a colour being worked out
set(SCREENBUFFER 8 CACHE STRING "SCREENBUFFER setting: 0, 1, 8 or 16")
set_property(CACHE SCREENBUFFER PROPERTY STRINGS 0 1 8 16)
if(NOT SCREENBUFFER MATCHES "^(0|1|8|16)$")
    message(FATAL_ERROR "SCREENBUFFER has to be 0, 1, 8 or 16, not ${SCREENBUFFER}")
endif()

# 1 = blown up to 240x240, as high as the screen, 0 = 1:1 in the middle, see SCALESCREEN in
# PlatformDOS.h
set(SCALESCREEN 1 CACHE STRING "SCALESCREEN setting: 1 or 0")

# the image set, 1 (16x16 tiles) or 2 (8x8 tiles), see IMAGESET in defines.h
set(IMAGESET 2 CACHE STRING "IMAGESET setting: 1 or 2")

# -1 = every skin, n = skin n, with a 1 bpp buffer always the black & white one, see FORCESKIN in
# defines.h
set(FORCESKIN -1 CACHE STRING "FORCESKIN setting")

# 1 = frames are held to the game's frame rate, 0 = they run as fast as they go, see FPSLOCK in
# defines.h
set(FPSLOCK 1 CACHE STRING "FPSLOCK setting: 1 or 0")

# 1 = the debug header is always shown, 0 = the debug combination shows and hides it, see FORCEDEBUG
# in defines.h
set(FORCEDEBUG 0 CACHE STRING "FORCEDEBUG setting: 0 or 1")

# the game's files, not the ESPboy platform or its library (lib/ is not globbed)
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/*.cpp")
list(FILTER GAME_SOURCES EXCLUDE REGEX "PlatformESPboy\\.cpp$")

add_executable(${GAME_NAME} ${GAME_SOURCES})
# DJGPP keeps PRIu32 and the rest behind this in C++, and the game prints its figures with them
target_compile_definitions(${GAME_NAME} PRIVATE PLATFORM_DOS __STDC_FORMAT_MACROS=1
    SCREENBUFFER=${SCREENBUFFER} SCALESCREEN=${SCALESCREEN} FPSLOCK=${FPSLOCK}
    FORCESKIN=${FORCESKIN} FORCEDEBUG=${FORCEDEBUG} IMAGESET=${IMAGESET})
target_include_directories(${GAME_NAME} PRIVATE "${GAME_DIR}")
# a 386 is the least a DJGPP program runs on anyway, and the game has no use for exceptions
target_compile_options(${GAME_NAME} PRIVATE -O2 -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_options(${GAME_NAME} PRIVATE -Wl,--gc-sections)
set_target_properties(${GAME_NAME} PROPERTIES OUTPUT_NAME "${GAME_NAME}" SUFFIX ".EXE")

# CWSDPMI in front of the program, so it needs nothing beside it on the disk
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND EXISTS "${CWSDSTUB}")
    add_custom_command(TARGET ${GAME_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/stubify.py" "${CWSDSTUB}" "$<TARGET_FILE:${GAME_NAME}>"
        COMMENT "Building the DPMI host into ${GAME_NAME}.EXE"
        VERBATIM)
else()
    message(WARNING "CWSDPMI not built in: the game will need a DPMI host on the machine it runs on")
endif()
