AUTOMAKE_OPTIONS = dist-zip foreign

bin_PROGRAMS = mmg

INCLUDES = -I.. -I../..

mmg_SOURCES = mmg.cpp mmg.h \
			../chapters.cpp ../chapters.h \
			../chapter_parser_xml.cpp \
			../chapter_writer.cpp \
			../common.cpp ../common.h \
			extern_data.cpp extern_data.h \
			../iso639.cpp ../iso639.h \
			kax_analyzer.cpp kax_analyzer.h \
			../mm_io.cpp ../mm_io.h \
			mux_dialog.cpp \
			tab_attachments.cpp \
			tab_chapters.cpp \
			tab_input.cpp \
			tab_global.cpp \
			tab_settings.cpp

mmg_LDADD = @PROFILING_LIBS@ @MATROSKA_LIBS@ @EBML_LIBS@ @EXPAT_LIBS@ \
		@WXWINDOWS_LIBS@ @MINGW_LIBS@ @MINGW_GUIAPP@ @ICONV_LIBS@
