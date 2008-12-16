UIC = uic
MOC = moc
LINK = g++ $(LDFLAGS)
CXX = g++
CXXCOMPILE = g++
CXXFLAGS = -I../.. -I../common -I../../avilib-0.6.10 -I../../librmff $(shell pkg-config --cflags QtCore QtGui) -DVERSION=\"2.4.0\"
LDFLAGS = -L../common

ifeq ($(shell gcc -v 2>&1 | awk '/mingw special/ { print "1" }'),1)
CXXFLAGS += -I$(HOME)/prog/video/mingw/include -I$(HOME)/prog/video/mingw/src/uc/libebml -I$(HOME)/prog/video/mingw/src/uc/libmatroska
LDFLAGS += -L$(HOME)/prog/video/mingw/lib -L$(HOME)/prog/video/mingw/src/uc/libebml/make/mingw32 -L$(HOME)/prog/video/mingw/src/uc/libmatroska/make/mingw32
EXEEXT = .exe
else
CXXFLAGS += -I../../../libebml -I../../../libmatroska
LDFLAGS += -L../../../libebml/make/linux -L../../../libmatroska/make/linux
endif

ifeq ($(V),1)
  Q =
else
  Q = @
endif

mmg_qt_UIFILES=$(patsubst %.ui,%.h,$(wildcard forms/*.ui))
mmg_qt_MOCFILES=main_window.moc.cpp capabilities_reader.moc.cpp file_prober.moc.cpp
mmg_qt_SOURCES=mmg_qt.cpp main_window.cpp capabilities_reader.cpp tab_input.cpp \
		file_prober.cpp input_file.cpp
mmg_qt_OBJECTS=$(patsubst %.cpp,%.o,$(mmg_qt_SOURCES)) $(patsubst %.cpp,%.o,$(mmg_qt_MOCFILES))
mmg_qt_LDADD=$(shell pkg-config --libs QtCore QtGui) -lmtxcommon -lmatroska -lebml

ALL_SOURCES=$(mmg_qt_SOURCES)

all: mmg-qt$(EXEEXT)

mmg-qt$(EXEEXT): $(mmg_qt_UIFILES) $(mmg_qt_MOCFILES) $(mmg_qt_OBJECTS)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(mmg_qt_OBJECTS) $(mmg_qt_LDADD)

clean:
	rm -f forms/*.h forms/*.moc.cpp forms/*.o *.o *.moc.cpp mmg-qt$(EXEEXT)

depend:
	@echo Calculating dependecies:
	@rm -f .depend
	@touch .depend
	@for i in $(ALL_SOURCES); do \
		o="`echo $$i | sed -e 's/\.c$$/.o/' -e 's/\.cpp$$/.o/'`" ; \
		echo ' CXX -MM '$$i ; \
		$(CXX) $(CXXFLAGS) $(INCLUDES) $(SYSTEM_INCLUDES) -MM \
		-MT $$o $$i >> .depend ; \
	done

%.o: %.cpp
	@echo '     CXX ' $<
	$(Q)$(CXXCOMPILE) $(CXXFLAGS) -c -o $@ $<

# Qt files
%.h: %.ui
	@echo '     UIC ' $<
	$(Q)$(UIC) $< > $@

%.moc.cpp: %.h
	@echo '     MOC ' $<
	$(Q)$(MOC) $(QT_CFLAGS) $< > $@

ifneq ($(wildcard .depend),)
include .depend
endif
