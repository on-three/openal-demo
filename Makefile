
TARGET=midi-player
WEB_TARGET=$(TARGET).html

INCLUDE_DIRS = -I../wildmidi/include
LINUX_LIBS = ../wildmidi/linux/libWildMidi.a
WEB_LIBS = ../wildmidi/web/libWildMidi.a
WEB_FLAGS = --emrun --embed-file assets -s ALLOW_MEMORY_GROWTH=1

all: native web

native: $(TARGET)

$(TARGET): main.cpp
	g++ main.cpp -g -lopenal $(INCLUDE_DIRS) $(LINUX_LIBS) -o $@

web: $(WEB_TARGET)
	
$(WEB_TARGET): main.cpp
	em++ main.cpp -g4 -lopenal  $(WEB_FLAGS) $(INCLUDE_DIRS) $(WEB_LIBS) -o $@

run: $(TARGET)
	./$^

run-web: $(WEB_TARGET)
	emrun $^

clean:
	rm -f $(TARGET)
	rm -f $(WEB_TARGET)
	rm -f *.wasm
	rm -f *.wast
	rm -f *.o
	rm -f *.js
	rm -f *.map