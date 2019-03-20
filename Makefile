

INCLUDE_DIRS = -I../wildmidi/include
#LINUX_LIB_DIR = -L../wildmidi/lib
#WEB_LIB_DIR = -L../wildmidi/web
LINUX_LIBS = ../wildmidi/linux/libWildMidi.a
WEB_LIBS = ../wildmidi/web/libWildMidi.a

WEB_FLAGS = --embed-file assets -s ALLOW_MEMORY_GROWTH=1

native:
	g++ main.cpp -lopenal $(INCLUDE_DIRS) $(LINUX_LIBS) -o test

web:
	em++ main.cpp -g -lopenal  $(WEB_FLAGS) $(INCLUDE_DIRS) $(WEB_LIBS) -o test.html