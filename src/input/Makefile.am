if HAVE_AVILIB0_6_10
avilib0_6_10_incdirs = -I$(top_srcdir)/avilib-0.6.10
avilib0_6_10_libdirs = -L$(top_srcdir)/avilib-0.6.10
endif
if HAVE_AVICLASSES
aviclasses_incdirs = -I$(top_srcdir)/aviclasses
aviclasses_libdirs = -L$(top_srcdir)/aviclasses
endif

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
	$(avilib0_6_10_incdirs) $(aviclasses_incdirs) \
	-I$(top_srcdir)/src/common -I$(top_srcdir)/src/output

noinst_LIBRARIES = libmtxinput.a

libmtxinput_a_SOURCES = r_aac.cpp \
			r_ac3.cpp \
			r_avi.cpp \
			r_dts.cpp \
			r_flac.cpp \
			r_matroska.cpp \
			r_mp3.cpp \
			r_qtmp4.cpp \
			r_real.cpp \
			r_srt.cpp \
			r_ssa.cpp \
			r_ogm.cpp \
			r_vobsub.cpp \
			r_wav.cpp \
			subtitles.cpp
