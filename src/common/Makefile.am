if HAVE_AVILIB0_6_10
avilib0_6_10_incdirs = -I$(top_srcdir)/avilib-0.6.10
avilib0_6_10_libdirs = -L$(top_srcdir)/avilib-0.6.10
endif
if HAVE_AVICLASSES
aviclasses_incdirs = -I$(top_srcdir)/aviclasses
aviclasses_libdirs = -L$(top_srcdir)/aviclasses
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) -I.

#lib_LTLIBRARIES = libmtxcommon.la libmtxebmlcommon.la libmtxkaxcommon.la \
#		libmtxcomp.la

#libmtxcommon_la_SOURCES = base64.cpp \
#			aac_common.cpp \
#			ac3_common.cpp \
#			common.cpp \
#			dts_common.cpp \
#			iso639.cpp \
#			mp3_common.cpp
#
#libmtxebmlcommon_la_SOURCES = commonebml.cpp \
#			mm_io.cpp
#
#libmtxkaxcommon_la_SOURCES = chapters.cpp \
#			chapter_parser_xml.cpp \
#			chapter_writer.cpp \
#			tagparser_start.cpp \
#			tagparser_end.cpp \
#			tagwriter.cpp
#
#libmtxcomp_la_SOURCES = compression.cpp
#
noinst_LIBRARIES = libmtxcommon.a libmtxebmlcommon.a libmtxkaxcommon.a \
		libmtxcomp.a

libmtxcommon_a_SOURCES = base64.cpp \
			aac_common.cpp \
			ac3_common.cpp \
			common.cpp \
			dts_common.cpp \
			iso639.cpp \
			mp3_common.cpp

libmtxebmlcommon_a_SOURCES = commonebml.cpp \
			mm_io.cpp

libmtxkaxcommon_a_SOURCES = chapters.cpp \
			chapter_parser_xml.cpp \
			chapter_writer.cpp \
			tagparser_start.cpp \
			tagparser_end.cpp \
			tagwriter.cpp

libmtxcomp_a_SOURCES = compression.cpp
