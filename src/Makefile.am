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

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) \
	-Icommon -Iinput -Ioutput

LDFLAGS = @LDFLAGS@ $(avilib0_6_10_libdirs) $(aviclasses_libdirs) \
	-Linput -Loutput

SUBDIRS = common input output @MMG_SUBDIRS@

bin_PROGRAMS = mkvmerge mkvinfo mkvextract base64tool

##
## mkvmerge
##
mkvmerge_DEPENDENCIES = common/libmtxcommon.la common/libmtxebmlcommon.la \
			common/libmtxkaxcommon.la common/libmtxcomp.la \
			input/libmtxinput.a output/libmtxoutput.a \
			$(avilib0_6_10_deps) $(aviclasses_deps)

mkvmerge_SOURCES = mkvmerge.cpp \
			cluster_helper.cpp \
			pr_generic.cpp

mkvmerge_LDADD = -lmtxinput -lmtxoutput common/libmtxcommon.la \
		common/libmtxebmlcommon.la common/libmtxkaxcommon.la \
		common/libmtxcomp.la \
		@AVILIB_LIBS@ @AVICLASSES_LIBS@ \
		@MATROSKA_LIBS@ @EBML_LIBS@ \
		@VORBIS_LIBS@ @FLAC_LIBS@ @OGG_LIBS@ \
		@ICONV_LIBS@ @MINGW_LIBS@ @EXPAT_LIBS@ \
		@ZLIB_LIBS@ @LZO_LIBS@ @BZ2_LIBS@ \
		@PROFILING_LIBS@


##
## mkvinfo
##
mkvinfo_DEPENDENCIES = common/libmtxcommon.la common/libmtxebmlcommon.la \
			common/libmtxkaxcommon.la

mkvinfo_SOURCES = mkvinfo.cpp \
			mkvinfo_gui.cpp \
			mkvinfo_tag_types.cpp

mkvinfo_LDADD = common/libmtxcommon.la common/libmtxebmlcommon.la \
		@PROFILING_LIBS@ \
		@MATROSKA_LIBS@ @EBML_LIBS@ @ICONV_LIBS@ \
		@WXWINDOWS_LIBS@ @MINGW_LIBS@

##
## mkvextract
##
mkvextract_DEPENDENCIES = common/libmtxcommon.la common/libmtxebmlcommon.la \
			common/libmtxkaxcommon.la $(avilib0_6_10_deps)

mkvextract_SOURCES = mkvextract.cpp \
			mkvextract_attachments.cpp \
			mkvextract_chapters.cpp \
			mkvextract_tags.cpp \
			mkvextract_tracks.cpp

mkvextract_LDADD = common/libmtxcommon.la common/libmtxebmlcommon.la \
		common/libmtxkaxcommon.la @EXPAT_LIBS@ \
		@PROFILING_LIBS@ @AVILIB_LIBS@ @MATROSKA_LIBS@ @EBML_LIBS@ \
		@VORBIS_LIBS@ @OGG_LIBS@ @ICONV_LIBS@ @LZO_LIBS@ @BZ2_LIBS@

##
## base64tool
##
base64tool_DEPENDENCIES = common/libmtxcommon.la common/libmtxebmlcommon.la

base64tool_SOURCES = base64tool.cpp

base64tool_LDADD = common/libmtxcommon.la common/libmtxebmlcommon.la \
		@EBML_LIBS@ @ICONV_LIBS@
