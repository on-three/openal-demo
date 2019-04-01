
TARGET=midi-player
WEB_TARGET=index.html

# dependencies
WILDMIDI_DIR := ../wildmidi
WILDMIDI_INCLUDE_DIR := $(WILDMIDI_DIR)/include
MIDIFILE_DIR := ../midifile
MIDIFILE_INCLUDE_DIR := $(MIDIFILE_DIR)/include
SDL_INCLUDE_DIR := /usr/include/SDL2

FLAGS := -std=c++11 -lopenal
INCLUDE_DIRS = -I$(WILDMIDI_INCLUDE_DIR) -I$(MIDIFILE_INCLUDE_DIR)
LINUX_LIBS = $(WILDMIDI_DIR)/linux/libWildMidi.a $(MIDIFILE_DIR)/lib/libmidifile.a -lSDL2
WEB_LIBS = $(WILDMIDI_DIR)/web/libWildMidi.bc $(MIDIFILE_DIR)/lib/libmidifile.a
WEB_FLAGS = --emrun --preload-file assets -s ALLOW_MEMORY_GROWTH=1

all: native web

native: $(TARGET)

$(TARGET): main.cpp
	g++ main.cpp -g $(FLAGS) $(INCLUDE_DIRS) -I$(SDL_INCLUDE_DIR) $(LINUX_LIBS) -o $@

web: $(WEB_TARGET)
	
$(WEB_TARGET): main.cpp
	em++ main.cpp -g4 $(FLAGS) $(WEB_FLAGS) $(INCLUDE_DIRS) $(WEB_LIBS) -o $@

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