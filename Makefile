all: slipstream

GLHEADERS=-I../glfw-3/include
GLLIBS= -L../glfw-3/lib -lglfw3 -lglu32 -lopengl32 -lgdi32
EDL=../edl/bin/edl.exe

DISABLE_AUDIO=0
ENABLE_DEBUG=0
DISABLE_DSP=0

ifeq ($(DISABLE_AUDIO),1)
ALHEADERS=
ALLIBS=
else
ALHEADERS=-I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include
ALLIBS= /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
endif

ifeq ($(ENABLE_DEBUG),1)
SYM_OPTS=-g
DISASSM=
else
SYM_OPTS=
DISASSM=-n
endif

COMPILE=-c -O3 -Wall -Werror $(SYM_OPTS) -DDISABLE_DSP=$(DISABLE_DSP) -DDISABLE_AUDIO=$(DISABLE_AUDIO) -DENABLE_DEBUG=$(ENABLE_DEBUG) -Isrc/ -Isrc/host/ $(GLHEADERS) $(ALHEADERS)

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) slipstream.exe

out/i8086.lls: src/chips/i8086.edl
	mkdir -p out
	$(EDL) $(DISASSM) -t -O2 src/chips/i8086.edl >out/i8086.lls

out/i8086.lls.s: out/i8086.lls
	llc -O3 out/i8086.lls

out/slipDSP.lls: src/chips/slipDSP.edl
	mkdir -p out
	$(EDL) $(DISASSM) -O2 -s DSP_ src/chips/slipDSP.edl >out/slipDSP.lls

out/slipDSP.lls.s: out/slipDSP.lls
	llc -O3 out/slipDSP.lls

out/audio.o: src/host/audio.h src/host/audio.c
	mkdir -p out
	gcc $(COMPILE) src/host/audio.c -o out/audio.o

out/video.o: src/host/video.h src/host/video.c
	mkdir -p out
	gcc $(COMPILE) src/host/video.c -o out/video.o

out/keys.o: src/host/keys.h src/host/keys.c
	mkdir -p out
	gcc $(COMPILE) src/host/keys.c -o out/keys.o

out/asic.o: src/asic.h src/asic.c src/dsp.h
	mkdir -p out
	gcc $(COMPILE) src/asic.c -o out/asic.o

out/dsp.o: src/dsp.h src/dsp.c src/system.h
	mkdir -p out
	gcc $(COMPILE) src/dsp.c -o out/dsp.o

out/main.o: src/main.c src/host/keys.h src/host/video.h src/host/audio.h src/asic.h src/dsp.h src/system.h
	mkdir -p out
	gcc $(COMPILE) src/main.c -o out/main.o

slipstream: out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/asic.o out/dsp.o
	gcc $(SYM_OPTS) out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/asic.o out/dsp.o $(ALLIBS) $(GLLIBS) -o slipstream.exe

