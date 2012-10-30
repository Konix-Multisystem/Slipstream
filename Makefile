all: slipstream

GLHEADERS=-I../glfw-3/include
GLLIBS= -L../glfw-3/lib -lglfw3 -lglu32 -lopengl32 -lgdi32
EDL=../edl/bin/edl.exe

DISABLE_AUDIO=1
ENABLE_DEBUG=0

ifeq ($(DISABLE_AUDIO),1)
ALHEADERS=
ALLIBS=
else
ALHEADERS=-I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include
ALLIBS= /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
endif

COMPILE=-c -O0 -g -DDISABLE_AUDIO=$(DISABLE_AUDIO) -DENABLE_DEBUG=$(ENABLE_DEBUG) -Isrc/ -Isrc/host/ $(GLHEADERS) $(ALHEADERS)

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) slipstream.exe

out/i8086.lls: src/chips/i8086.edl
	mkdir -p out
	$(EDL) -O2 src/chips/i8086.edl >out/i8086.lls

out/i8086.lls.s: out/i8086.lls
	llc out/i8086.lls

out/slipDSP.lls: src/chips/slipDSP.edl
	mkdir -p out
	$(EDL) -O2 -s DSP_ src/chips/slipDSP.edl >out/slipDSP.lls

out/slipDSP.lls.s: out/slipDSP.lls
	llc out/slipDSP.lls

out/audio.o: src/host/audio.h src/host/audio.c
	mkdir -p out
	gcc $(COMPILE) src/host/audio.c -o out/audio.o

out/video.o: src/host/video.h src/host/video.c
	mkdir -p out
	gcc $(COMPILE) src/host/video.c -o out/video.o

out/keys.o: src/host/keys.h src/host/keys.c
	mkdir -p out
	gcc $(COMPILE) src/host/keys.c -o out/keys.o

out/asic.o: src/asic.h src/asic.c
	mkdir -p out
	gcc $(COMPILE) src/asic.c -o out/asic.o

out/main.o: src/main.c src/host/keys.h src/host/video.h src/host/audio.h src/asic.h
	mkdir -p out
	gcc $(COMPILE) src/main.c -o out/main.o

slipstream: out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/asic.o
	gcc -O0 -g out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s out/slipDSP.lls.s out/asic.o $(ALLIBS) $(GLLIBS) -o slipstream.exe

