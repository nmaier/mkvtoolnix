MATROSKADIR=$(HOME)/prog/video/libmatroska/cvs/libmatroska
LDFLAGS=-L$(MATROSKADIR)/make/linux -Lavilib
LIBS=-lmatroska -lavi
INCLUDES=-I$(MATROSKADIR)/src -Iavilib
DEBUG=-g -DDEBUG
CXXFLAGS=$(DEBUG) $(INCLUDES)
CXX=g++
LD=$(CXX)

READERS_SOURCES=r_avi.cpp
READERS_OBJECTS=$(patsubst %.cpp,%.o,$(READERS_SOURCES))

OTHER_SOURCES=common.cpp
OTHER_OBJECTS=$(patsubst %.cpp,%.o,$(OTHER_SOURCES))

MKVMERGE_OBJECTS=$(READERS_OBJECTS) $(OTHER_OBJECTS)

all: subdirs mkvmerge

subdirs:
	make -C avilib

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

mkvmerge: mkvmerge.o $(MKVMERGE_OBJECTS)
	$(LD) -o $@ $(LDFLAGS) $< $(MKVMERGE_OBJECTS) $(LIBS)

clean:
	make -C avilib clean
	rm -f *.o mkvmerge
