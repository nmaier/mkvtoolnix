## process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = dist-zip foreign

SUBDIRS = avilib

bin_PROGRAMS = mkvmerge

mkvmerge_SOURCES = mkvmerge.cpp \
			common.cpp common.h \
			queue.cpp queue.h \
			r_generic.h p_generic.h \
			r_avi.cpp r_avi.h

#mkvmerge_LDADD = @OGG_LIBS@ @VORBIS_LIBS@ @VORBISENC_LIBS@ @AVILIB_LIBS@ \
#		@DMALLOC_LIBS@ @PROFILING_LIBS@ @MATROSKA_LIBS@

mkvmerge_LDADD = @AVILIB_LIBS@ @DMALLOC_LIBS@ @PROFILING_LIBS@ @MATROSKA_LIBS@
