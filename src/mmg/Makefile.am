AUTOMAKE_OPTIONS = dist-zip foreign

bin_PROGRAMS = mmg

INCLUDES = -I.. -I../..

mmg_SOURCES = mmg.cpp mmg.h \
			../common.cpp ../common.h \
			extern_data.cpp extern_data.h \
			../iso639.cpp ../iso639.h \
			mux_dialog.cpp \
			tab_attachments.cpp \
			tab_input.cpp \
			tab_global.cpp \
			tab_settings.cpp

mmg_LDADD = @PROFILING_LIBS@ @EBML_LIBS@ @WXWINDOWS_LIBS@ @MINGW_LIBS@ \
		@ICONV_LIBS@

