cmake_minimum_required(VERSION 3.4.3)

macro(EDLCompile outName inName)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		MAIN_DEPENDENCY ${inName}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND ${EDL_COMMAND} -g -t -O3 ${inName} -o ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		COMMENT "--- EDL Compiling : ${outName} ---"
		DEPENDS ${EDL_COMMAND})
endmacro(EDLCompile)
macro(EDLCompileSym outName inName symName)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		MAIN_DEPENDENCY ${inName}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND ${EDL_COMMAND} -g -t -O3 -s ${symName} ${inName} -o ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		COMMENT "--- EDL Compiling : ${outName} ---"
		DEPENDS ${EDL_COMMAND})
endmacro(EDLCompileSym)

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_STANDARD 99)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(slipstream)

find_package(glfw3 3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)
find_package(OpenAL REQUIRED)

add_definitions(-DENABLE_PDS=1)
add_definitions(-DMEMORY_MAPPED_DEBUGGER=1)
add_definitions(-DTERMINAL=0)
add_definitions(-DDISABLE_DSP=0)
add_definitions(-DDISABLE_AUDIO=0)
add_definitions(-DENABLE_DEBUG=$<CONFIG:DEBUG>)

if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
endif()
if (MSVC)
	add_definitions(-DOS_WINDOWS=1)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
endif()
if (APPLE)
	add_definitions(-DOS_APPLE=1)
endif()
include_directories(${OPENAL_INCLUDE_DIR})
include_directories(${OPENGL_INCLUDE_DIR})

include_directories(src/host)

set(HOST_H src/host/logfile.h src/host/audio.h src/host/video.h src/host/keys.h)
set(HOST_C src/host/logfile.c src/host/audio.c src/host/video.c src/host/keys.c)
set(MAIN_H src/asic.h src/dsp.h src/terminalEm.h src/system.h src/memory.h src/font.h src/fdisk.h src/disasm.h src/debugger.h)
set(MAIN_C src/main.c src/asic.c src/dsp.c src/terminalEm.c src/memory.c src/fdisk.c src/disasm.c src/debugger.c src/MemoryMappedDebugger.c)
set(PDS_H src/pds/pds.h)
set(PDS_C src/pds/pds.c)

EDLCompile(i8086.obj src/chips/i8086.edl)
EDLCompileSym(i80386.obj src/chips/i80386sx.edl MSU_)
EDLCompileSym(i80386_pds.obj src/chips/i80386sx.edl PDS_)
EDLCompileSym(slipDSP.obj src/chips/slipDSP.edl DSP_)
EDLCompileSym(flare1DSP.obj src/chips/flare1DSP.edl FL1DSP_)
EDLCompileSym(flare1Blitter.obj src/chips/flare1Blitter.edl FL1BLT_)
EDLCompileSym(z80.obj src/chips/z80.edl Z80_)

add_executable(slipstream
	${HOST_C}
	${PDS_C}
	${MAIN_C}
	${CMAKE_CURRENT_BINARY_DIR}/i8086.obj
	${CMAKE_CURRENT_BINARY_DIR}/i80386.obj
	${CMAKE_CURRENT_BINARY_DIR}/i80386_pds.obj
	${CMAKE_CURRENT_BINARY_DIR}/slipDSP.obj
	${CMAKE_CURRENT_BINARY_DIR}/flare1DSP.obj
	${CMAKE_CURRENT_BINARY_DIR}/flare1Blitter.obj
	${CMAKE_CURRENT_BINARY_DIR}/z80.obj
	${HOST_H}
	${MAIN_H}
	${PDS_H}
	)

target_link_libraries(slipstream ${OPENAL_LIBRARY})
target_link_libraries(slipstream ${OPENGL_gl_LIBRARY})
target_link_libraries(slipstream glfw)

source_group(Sources FILES ${MAIN_C})
source_group(Headers FILES ${MAIN_H})
source_group(Sources\\Host FILES ${HOST_C})
source_group(Headers\\Host FILES ${HOST_H})
source_group(Sources\\PDS FILES ${PDS_C})
source_group(Headers\\PDS FILES ${PDS_H})
source_group(Chips FILES src/chips/i8086.edl)
source_group(Chips FILES src/chips/i80386sx.edl)
source_group(Chips FILES src/chips/slipDSP.edl)
source_group(Chips FILES src/chips/flare1DSP.edl)
source_group(Chips FILES src/chips/flare1Blitter.edl)
source_group(Chips FILES src/chips/flare1Blitter_internal.edl)
source_group(Chips FILES src/chips/z80.edl)

#
# For debugging
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

