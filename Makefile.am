## process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = dist-zip foreign

SUBDIRS = avilib

bin_PROGRAMS = mkvmerge

mkvmerge_SOURCES = mkvmerge.cpp mkvmerge.h \
			ac3_common.cpp ac3_common.h \
			common.cpp common.h \
			mp3_common.cpp mp3_common.h \
			p_mp3.cpp p_mp3.h \
			p_video.cpp p_video.h \
			pr_generic.h pr_generic.cpp \
			queue.cpp queue.h \
			r_avi.cpp r_avi.h

#mkvmerge_LDADD = @OGG_LIBS@ @VORBIS_LIBS@ @VORBISENC_LIBS@ @AVILIB_LIBS@ \
#		@DMALLOC_LIBS@ @PROFILING_LIBS@ @MATROSKA_LIBS@

mkvmerge_LDADD = @AVILIB_LIBS@ @DMALLOC_LIBS@ @PROFILING_LIBS@ @MATROSKA_LIBS@
