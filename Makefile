all: slipstream

COMPILER=gcc

ifeq ($(OS),Windows_NT)
	OS_WINDOWS=1
else
	OS_WINDOWS=0
endif

ifeq ($(OS_WINDOWS),1)
	ENABLE_REMOTE_DEBUG=0
	AL_INCLUDE=-I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include
	AL_LIBS=/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
	EDL=../edl/bin/edl.exe
	GLHEADERS=-I../glfw-3.0.4/include 
	GLLIBS= -L../glfw-3.0.4/lib-mingw -lglfw3 -lglu32 -lopengl32 -lgdi32
	EXTRA_LIBS=-lws2_32
else
	ENABLE_REMOTE_DEBUG=0
	AL_INCLUDE=
	AL_LIBS=-lopenal 
	EDL=../EDL/bin/edl
	GLHEADERS=
	GLLIBS=-lglfw3 -lX11 -lXxf86vm -lXrandr -lXi -lrt -lm -lXcursor -lGL
	EXTRA_LIBS=
endif

ENABLE_DEBUG=1
ENABLE_GPROF=0
DISABLE_AUDIO=0
DISABLE_DSP=0

ifeq ($(DISABLE_AUDIO),1)
ALHEADERS=
ALLIBS=
else
ALHEADERS=
ALLIBS= 
endif

ifeq ($(ENABLE_GPROF),1)
PROF_OPTS=-pg
else
PROF_OPTS=
endif

ifeq ($(ENABLE_DEBUG),1)
SYM_OPTS=-g $(PROF_OPTS)
DISASSM=
else
SYM_OPTS=$(PROF_OPTS)
DISASSM=-n
endif

COMPILE=-c -O3 -Wall -Werror $(SYM_OPTS) -DOS_WINDOWS=$(OS_WINDOWS) -DENABLE_REMOTE_DEBUG=$(ENABLE_REMOTE_DEBUG) -DDISABLE_DSP=$(DISABLE_DSP) -DDISABLE_AUDIO=$(DISABLE_AUDIO) -DENABLE_DEBUG=$(ENABLE_DEBUG) -Isrc/ -Isrc/host/ $(GLHEADERS) $(AL_INCLUDE)

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) slipstream.exe

out/i8086.lls: src/chips/i8086.edl
	mkdir -p out
	$(EDL) $(DISASSM) -t -O3 src/chips/i8086.edl >out/i8086.lls

out/i8086.lls.s: out/i8086.lls
	llc -O3 out/i8086.lls

out/slipDSP.lls: src/chips/slipDSP.edl
	mkdir -p out
	$(EDL) $(DISASSM) -O3 -s DSP_ src/chips/slipDSP.edl >out/slipDSP.lls

out/slipDSP.lls.s: out/slipDSP.lls
	llc -O3 out/slipDSP.lls

out/flare1DSP.lls: src/chips/flare1DSP.edl
	mkdir -p out
	$(EDL) $(DISASSM) -O3 -s FL1DSP_ src/chips/flare1DSP.edl >out/flare1DSP.lls

out/flare1DSP.lls.s: out/flare1DSP.lls
	llc -O3 out/flare1DSP.lls

out/flare1Blitter.lls: src/chips/flare1Blitter.edl src/chips/flare1Blitter_internal.edl
	mkdir -p out
	$(EDL) $(DISASSM) -O2 -s FL1BLT_ src/chips/flare1Blitter.edl >out/flare1Blitter.lls

out/flare1Blitter.lls.s: out/flare1Blitter.lls
	llc -O3 out/flare1Blitter.lls

out/z80.lls: src/chips/z80.edl
	mkdir -p out
	$(EDL) $(DISASSM) -O3 -s Z80_ src/chips/z80.edl >out/z80.lls

out/z80.lls.s: out/z80.lls
	llc -O3 out/z80.lls

out/logfile.o: src/host/logfile.h src/host/logfile.c
	mkdir -p out
	$(COMPILER) $(COMPILE) src/host/logfile.c -o out/logfile.o

out/audio.o: src/host/audio.h src/host/audio.c src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/host/audio.c -o out/audio.o

out/video.o: src/host/video.h src/host/video.c src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/host/video.c -o out/video.o

out/keys.o: src/host/keys.h src/host/keys.c src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/host/keys.c -o out/keys.o

out/asic.o: src/asic.h src/asic.c src/dsp.h src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/asic.c -o out/asic.o

out/dsp.o: src/dsp.h src/dsp.c src/system.h src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/dsp.c -o out/dsp.o

out/terminalEm.o: src/terminalEm.c src/system.h src/host/logfile.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/terminalEm.c -o out/terminalEm.o

out/memory.o: src/memory.c src/system.h src/host/logfile.h src/memory.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/memory.c -o out/memory.o

out/debugger.o: src/debugger.c src/system.h src/host/logfile.h src/debugger.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/debugger.c -o out/debugger.o

out/main.o: src/main.c src/host/keys.h src/host/video.h src/host/audio.h src/asic.h src/dsp.h src/system.h src/host/logfile.h src/memory.h src/debugger.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/main.c -o out/main.o

out/fdisk.o: src/fdisk.c src/system.h src/host/logfile.h src/fdisk.h
	mkdir -p out
	$(COMPILER) $(COMPILE) src/fdisk.c -o out/fdisk.o

slipstream: out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/asic.o out/dsp.o out/logfile.o out/z80.lls.s out/memory.o out/debugger.o out/flare1DSP.lls.s out/terminalEm.o out/fdisk.o out/flare1Blitter.lls.s
	$(COMPILER) $(SYM_OPTS) out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/flare1DSP.lls.s out/terminalEm.o out/z80.lls.s out/flare1Blitter.lls.s out/asic.o out/dsp.o out/logfile.o out/memory.o out/debugger.o out/fdisk.o $(ALLIBS) $(GLLIBS) -lpthread -lws2_32 -o slipstream.exe

