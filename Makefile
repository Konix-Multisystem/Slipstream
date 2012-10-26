all: slipstream

GLHEADERS=-I../glfw-3/include
ALHEADERS=-I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include
GLLIBS= -L../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../edl/bin/edl.exe

COMPILE=-c -O3 -g -Isrc/ -Isrc/host/ $(GLHEADERS) $(ALHEADERS)

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) slipstream.exe

out/i8086.lls: src/chips/i8086.edl
	mkdir -p out
	$(EDL) -O2 src/chips/i8086.edl >out/i8086.lls

out/i8086.lls.s: out/i8086.lls
	llc out/i8086.lls

out/audio.o: src/host/audio.h src/host/audio.c
	mkdir -p out
	gcc $(COMPILE) src/host/audio.c -o out/audio.o

out/video.o: src/host/video.h src/host/video.c
	mkdir -p out
	gcc $(COMPILE) src/host/video.c -o out/video.o

out/keys.o: src/host/keys.h src/host/keys.c
	mkdir -p out
	gcc $(COMPILE) src/host/keys.c -o out/keys.o

out/main.o: src/main.c src/host/keys.h src/host/video.h src/host/audio.h
	mkdir -p out
	gcc $(COMPILE) src/main.c -o out/main.o

slipstream: out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s 
	gcc -O3 -g out/main.o out/keys.o out/video.o out/audio.o out/i8086.lls.s $(ALLIBS) $(GLLIBS) -o slipstream.exe

