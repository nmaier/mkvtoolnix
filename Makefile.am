## process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = dist-zip foreign

SUBDIRS = avilib

bin_PROGRAMS = mkvmerge mkvinfo

mkvmerge_SOURCES = mkvmerge.cpp mkvmerge.h \
			aac_common.cpp aac_common.h \
			ac3_common.cpp ac3_common.h \
			dts_common.cpp dts_common.h \
			cluster_helper.cpp cluster_helper.h \
			common.cpp common.h \
			iso639.cpp iso639.h \
			mm_io.cpp mm_io.h \
			mp3_common.cpp mp3_common.h \
			p_aac.cpp p_aac.h \
			p_ac3.cpp p_ac3.h \
			p_dts.cpp p_dts.h \
			p_mp3.cpp p_mp3.h \
			p_pcm.cpp p_pcm.h \
			p_textsubs.cpp p_textsubs.h \
			p_video.cpp p_video.h \
			p_vorbis.cpp p_vorbis.h \
			pr_generic.h pr_generic.cpp \
			r_aac.cpp r_aac.h \
			r_ac3.cpp r_ac3.h \
			r_avi.cpp r_avi.h \
			r_dts.cpp r_dts.h \
			r_matroska.cpp r_matroska.h \
			r_mp3.cpp r_mp3.h \
			r_mp4.cpp r_mp4.h \
			r_srt.cpp r_srt.h \
			r_ogm.cpp r_ogm.h \
			r_wav.cpp r_wav.h \
			subtitles.cpp subtitles.h

mkvinfo_SOURCES = mkvinfo.cpp mkvinfo.h \
			common.cpp common.h \
			mm_io.cpp mm_io.h \
			iso639.cpp iso639.h

mkvmerge_LDADD = @AVILIB_LIBS@ @PROFILING_LIBS@ \
		@MATROSKA_LIBS@ @EBML_LIBS@ @VORBIS_LIBS@ @OGG_LIBS@ \
		@ICONV_LIBS@

mkvinfo_LDADD = @PROFILING_LIBS@ @MATROSKA_LIBS@ @EBML_LIBS@ @ICONV_LIBS@

man_MANS = mkvmerge.1 mkvinfo.1

EXTRA_DIST = $(man_MANS)
