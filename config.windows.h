// derived from config.h.in for building with Visual Studio - refer to
// that file for details and options

#define PACKAGE "mkvtoolnix"
#define VERSION "3.2.0"

#pragma warning(disable:4018)	//signed/unsigned mismatch
#pragma warning(disable:4244)	//conversion; possible loss of data
#pragma warning(disable:4290)	//C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
#pragma warning(disable:4334)	//'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable:4800)	//forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable:4996)	//'function': was declared deprecated

#define HAVE_BOOST 1
#define HAVE_BOOST_FILESYSTEM 1
#define HAVE_BOOST_REGEX 1
#define HAVE_BOOST_SYSTEM 1
#define HAVE_EXPAT_H 1
#define HAVE_MEMORY_H 1
#define HAVE_OGG_OGG_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_VORBIS_CODEC_H 1
#define HAVE_ZLIB_H 1

#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME ""
#define PACKAGE_STRING ""
#define PACKAGE_TARNAME ""
#define PACKAGE_URL ""
#define PACKAGE_VERSION ""
#define STDC_HEADERS ""
