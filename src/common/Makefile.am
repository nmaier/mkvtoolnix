if HAVE_AVILIB
avilib_incdirs = -I$(top_srcdir)/avilib
avilib_libdirs = -L$(top_srcdir)/avilib
endif
if HAVE_AVILIB0_6_10
avilib0_6_10_incdirs = -I$(top_srcdir)/avilib-0.6.10
avilib0_6_10_libdirs = -L$(top_srcdir)/avilib-0.6.10
endif
if HAVE_AVICLASSES
aviclasses_incdirs = -I$(top_srcdir)/aviclasses
aviclasses_libdirs = -L$(top_srcdir)/aviclasses
endif

DEFAULT_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) -I.

noinst_LIBRARIES = libmtxcommon.a

libmtxcommon_a_SOURCES = base64.cpp \
			aac_common.cpp \
			ac3_common.cpp \
			chapters.cpp \
			chapter_parser_xml.cpp \
			chapter_writer.cpp \
			compression.cpp \
			common.cpp \
			dts_common.cpp \
			iso639.cpp \
			mm_io.cpp \
			mp3_common.cpp \
			tagparser_start.cpp \
			tagparser_end.cpp \
			tagwriter.cpp
