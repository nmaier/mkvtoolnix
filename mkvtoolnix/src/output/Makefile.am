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
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) \
	-I../common

noinst_LIBRARIES = libmtxoutput.a

libmtxoutput_a_SOURCES = p_aac.cpp \
			p_ac3.cpp \
			p_dts.cpp \
			p_mp3.cpp \
			p_passthrough.cpp \
			p_pcm.cpp \
			p_textsubs.cpp \
			p_video.cpp \
			p_vobsub.cpp \
			p_vorbis.cpp
