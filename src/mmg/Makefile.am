AUTOMAKE_OPTIONS = dist-zip foreign

bin_PROGRAMS = mmg

DEFAULT_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
		-I$(top_srcdir)/src/common

LDFLAGS = @LDFLAGS@ -L$(top_srcdir)/src/common

mmg_DEPENDENCIES = ../common/libmtxcommon.a

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

mmg_LDADD = -lmtxcommon @MATROSKA_LIBS@ @EBML_LIBS@ \
		@WXWINDOWS_LIBS@ @EXPAT_LIBS@ \
		@MINGW_GUIAPP@ @MINGW_LIBS@ @ICONV_LIBS@ @PROFILING_LIBS@
