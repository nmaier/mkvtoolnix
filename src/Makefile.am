AUTOMAKE_OPTIONS = dist-zip foreign

if HAVE_AVILIB0_6_10
avilib0_6_10_incdirs = -I$(top_srcdir)/avilib-0.6.10
avilib0_6_10_libdirs = -L$(top_srcdir)/avilib-0.6.10
avilib0_6_10_deps = ../avilib-0.6.10/libavi.a
endif
if HAVE_AVICLASSES
aviclasses_incdirs = -I$(top_srcdir)/aviclasses
aviclasses_libdirs = -L$(top_srcdir)/aviclasses
aviclasses_deps = ../aviclasses/libaviclasses.a
endif

DEFAULT_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) \
	-Icommon -Iinput -Ioutput

LDFLAGS = @LDFLAGS@ $(avilib0_6_10_libdirs) $(aviclasses_libdirs) \
	-Lcommon -Linput -Loutput

SUBDIRS = common input output @MMG_SUBDIRS@

bin_PROGRAMS = mkvmerge mkvinfo mkvextract base64tool

##
## mkvmerge
##
mkvmerge_DEPENDENCIES = common/libmtxcommon.a input/libmtxinput.a \
			output/libmtxoutput.a \
			$(avilib0_6_10_deps) $(aviclasses_deps)

mkvmerge_SOURCES = mkvmerge.cpp \
			cluster_helper.cpp \
			pr_generic.cpp

mkvmerge_LDADD = -lmtxinput -lmtxoutput -lmtxcommon \
		@AVILIB_LIBS@ @AVICLASSES_LIBS@ \
		@MATROSKA_LIBS@ @EBML_LIBS@ \
		@VORBIS_LIBS@ @FLAC_LIBS@ @OGG_LIBS@ \
		@ICONV_LIBS@ @MINGW_LIBS@ @EXPAT_LIBS@ \
		@ZLIB_LIBS@ @LZO_LIBS@ @BZ2_LIBS@ \
		@PROFILING_LIBS@


##
## mkvinfo
##
mkvinfo_DEPENDENCIES = common/libmtxcommon.a

mkvinfo_SOURCES = mkvinfo.cpp \
			mkvinfo_gui.cpp \
			mkvinfo_tag_types.cpp

mkvinfo_LDADD =  -lmtxcommon @PROFILING_LIBS@ \
		@MATROSKA_LIBS@ @EBML_LIBS@ @ICONV_LIBS@ \
		@WXWINDOWS_LIBS@ @MINGW_LIBS@

##
## mkvextract
##
mkvextract_DEPENDENCIES = common/libmtxcommon.a $(avilib0_6_10_deps)

mkvextract_SOURCES = mkvextract.cpp \
			mkvextract_attachments.cpp \
			mkvextract_chapters.cpp \
			mkvextract_tags.cpp \
			mkvextract_tracks.cpp

mkvextract_LDADD = -lmtxcommon @PROFILING_LIBS@ \
		@AVILIB_LIBS@ @MATROSKA_LIBS@ @EBML_LIBS@ \
		@VORBIS_LIBS@ @OGG_LIBS@ @ICONV_LIBS@ @LZO_LIBS@ @BZ2_LIBS@

##
## base64tool
##
base64tool_DEPENDENCIES = common/libmtxcommon.a

base64tool_SOURCES = base64tool.cpp

base64tool_LDADD = -lmtxcommon @EBML_LIBS@ @ICONV_LIBS@
