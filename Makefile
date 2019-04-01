
TARGET=midi-player
WEB_TARGET=index.html

# dependencies
WILDMIDI_DIR := ../wildmidi
WILDMIDI_INCLUDE_DIR := $(WILDMIDI_DIR)/include
MIDIFILE_DIR := ../midifile
MIDIFILE_INCLUDE_DIR := $(MIDIFILE_DIR)/include
MIDIFILE_LIB_DIR_NATIVE := lib
MIDIFILE_LIB_DIR_WEB := lib-web
MIDIFILE_LIB_NATIVE := $(MIDIFILE_DIR)/$(MIDIFILE_LIB_DIR_NATIVE)/libmidifile.a
MIDIFILE_LIB_WEB := $(MIDIFILE_DIR)/$(MIDIFILE_LIB_DIR_WEB)/libmidifile.bc
SDL_INCLUDE_DIR := /usr/include/SDL2

FLAGS := -std=c++11 -lopenal
INCLUDE_DIRS = -I$(WILDMIDI_INCLUDE_DIR) -I$(MIDIFILE_INCLUDE_DIR)
LINUX_LIBS = $(WILDMIDI_DIR)/linux/libWildMidi.a $(MIDIFILE_LIB_NATIVE) -lSDL2
WEB_LIBS = $(WILDMIDI_DIR)/web/libWildMidi.bc $(MIDIFILE_LIB_WEB) -s USE_SDL=2
WEB_FLAGS = --emrun --preload-file assets -s ALLOW_MEMORY_GROWTH=1

all: native web

native: $(TARGET)

$(TARGET): main.cpp
	g++ main.cpp -g $(FLAGS) $(INCLUDE_DIRS) -I$(SDL_INCLUDE_DIR) $(LINUX_LIBS) -o $@

web: $(WEB_TARGET) $(MIDIFILE_LIB_WEB)
	
$(WEB_TARGET): main.cpp
	em++ main.cpp -g4 $(FLAGS) $(WEB_FLAGS) $(INCLUDE_DIRS) $(WEB_LIBS) -o $@

run: $(TARGET)
	./$^

run-web: $(WEB_TARGET)
	emrun $^


$(MIDIFILE_LIB_NATIVE):
	pushd $(MIDIFILE_DIR); make

$(MIDIFILE_LIB_WEB):
	pushd $(MIDIFILE_DIR); emmake make OBJDIR=obj-web COMPILER=em++ LIBDIR=lib-web LIBFILE=libmidifile.bc AR=emar RANLIB=emranlib library

clean:
	rm -f $(TARGET)
	rm -f $(WEB_TARGET)
	rm -f *.wasm
	rm -f *.wast
	rm -f *.o
	rm -f *.js
	rm -f *.map