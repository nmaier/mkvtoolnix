AUTOMAKE_OPTIONS = dist-zip foreign

bin_PROGRAMS = mmg

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
		-I$(top_srcdir)/src/common

CXXFLAGS = @CXXFLAGS@ -Wshadow

LDFLAGS = @LDFLAGS@ -L$(top_srcdir)/src/common

#mmg_DEPENDENCIES = ../common/libmtxcommon.la ../common/libmtxebmlcommon.la \
#		../common/libmtxkaxcommon.la
mmg_DEPENDENCIES = ../common/libmtxcommon.a ../common/libmtxebmlcommon.a \
		../common/libmtxkaxcommon.a

mmg_SOURCES = mmg.cpp \
			extern_data.cpp \
			kax_analyzer.cpp \
			mux_dialog.cpp \
			tab_advanced.cpp \
			tab_attachments.cpp \
			tab_chapters.cpp \
			tab_input.cpp \
			tab_global.cpp \
			tab_settings.cpp

#mmg_LDADD = ../common/libmtxcommon.la ../common/libmtxebmlcommon.la \
#		../common/libmtxkaxcommon.la @MATROSKA_LIBS@ @EBML_LIBS@ \
#		@WXWINDOWS_LIBS@ @EXPAT_LIBS@ \
#		@MINGW_GUIAPP@ @MINGW_LIBS@ @ICONV_LIBS@ @PROFILING_LIBS@
mmg_LDADD = -lmtxkaxcommon -lmtxebmlcommon -lmtxcommon \
		 @MATROSKA_LIBS@ @EBML_LIBS@ @WXWINDOWS_LIBS@ @EXPAT_LIBS@ \
		@MINGW_GUIAPP@ @MINGW_LIBS@ @ICONV_LIBS@ @PROFILING_LIBS@
