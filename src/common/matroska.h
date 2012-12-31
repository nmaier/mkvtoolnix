/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for the various Codec IDs

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef MTX_COMMON_MATROSKA_H
#define MTX_COMMON_MATROSKA_H

// see http://www.matroska.org/technical/specs/codecid/index.html

#define MKV_A_AAC_2MAIN  "A_AAC/MPEG2/MAIN"
#define MKV_A_AAC_2LC    "A_AAC/MPEG2/LC"
#define MKV_A_AAC_2SSR   "A_AAC/MPEG2/SSR"
#define MKV_A_AAC_2SBR   "A_AAC/MPEG2/LC/SBR"
#define MKV_A_AAC_4MAIN  "A_AAC/MPEG4/MAIN"
#define MKV_A_AAC_4LC    "A_AAC/MPEG4/LC"
#define MKV_A_AAC_4SSR   "A_AAC/MPEG4/SSR"
#define MKV_A_AAC_4LTP   "A_AAC/MPEG4/LTP"
#define MKV_A_AAC_4SBR   "A_AAC/MPEG4/LC/SBR"
#define MKV_A_AAC        "A_AAC"
#define MKV_A_AC3        "A_AC3"
#define MKV_A_ALAC       "A_ALAC"
#define MKV_A_EAC3       "A_EAC3"
#define MKV_A_DTS        "A_DTS"
#define MKV_A_MP2        "A_MPEG/L2"
#define MKV_A_MP3        "A_MPEG/L3"
#define MKV_A_PCM        "A_PCM/INT/LIT"
#define MKV_A_PCM_BE     "A_PCM/INT/BIG"
#define MKV_A_PCM_FLOAT  "A_PCM/FLOAT/IEEE"
#define MKV_A_VORBIS     "A_VORBIS"
#define MKV_A_ACM        "A_MS/ACM"
#define MKV_A_OPUS       "A_OPUS"
#define MKV_A_QUICKTIME  "A_QUICKTIME"
#define MKV_A_QDMC       "A_QUICKTIME/QDMC"
#define MKV_A_QDMC2      "A_QUICKTIME/QDM2"
#define MKV_A_REAL_14_4  "A_REAL/14_4"
#define MKV_A_REAL_28_8  "A_REAL/28_8"
#define MKV_A_REAL_COOK  "A_REAL/COOK"
#define MKV_A_REAL_SIPR  "A_REAL/SIPR"
#define MKV_A_REAL_ATRC  "A_REAL/ATRC"
#define MKV_A_FLAC       "A_FLAC"
#define MKV_A_MLP        "A_MLP"
#define MKV_A_TRUEHD     "A_TRUEHD"
#define MKV_A_TTA        "A_TTA1"
#define MKV_A_WAVPACK4   "A_WAVPACK4"

#define MKV_V_MPEG1       "V_MPEG1"
#define MKV_V_MPEG2       "V_MPEG2"
#define MKV_V_MPEG4_SP    "V_MPEG4/ISO/SP"
#define MKV_V_MPEG4_ASP   "V_MPEG4/ISO/ASP"
#define MKV_V_MPEG4_AP    "V_MPEG4/ISO/AP"
#define MKV_V_MPEG4_AVC   "V_MPEG4/ISO/AVC"
#define MKV_V_MSCOMP      "V_MS/VFW/FOURCC"
#define MKV_V_REALV1      "V_REAL/RV10"
#define MKV_V_REALV2      "V_REAL/RV20"
#define MKV_V_REALV3      "V_REAL/RV30"
#define MKV_V_REALV4      "V_REAL/RV40"
#define MKV_V_QUICKTIME   "V_QUICKTIME"
#define MKV_V_THEORA      "V_THEORA"
#define MKV_V_DIRAC       "V_DIRAC"
#define MKV_V_VP8         "V_VP8"

#define MKV_S_TEXTUTF8   "S_TEXT/UTF8"
#define MKV_S_TEXTSSA    "S_TEXT/SSA"
#define MKV_S_TEXTASS    "S_TEXT/ASS"
#define MKV_S_TEXTUSF    "S_TEXT/USF"
#define MKV_S_TEXTASCII  "S_TEXT/ASCII"
#define MKV_S_VOBSUB     "S_VOBSUB"
#define MKV_S_VOBSUBZLIB "S_VOBSUB/ZLIB"
#define MKV_S_KATE       "S_KATE"
#define MKV_S_HDMV_PGS   "S_HDMV/PGS"

#define MKV_B_VOBBTN     "B_VOBBTN"

// see http://www.matroska.org/technical/specs/tagging/index.html#targettypes
#define TAG_TARGETTYPE_COLLECTION     70

#define TAG_TARGETTYPE_EDITION        60
#define TAG_TARGETTYPE_ISSUE          60
#define TAG_TARGETTYPE_VOLUME         60
#define TAG_TARGETTYPE_OPUS           60
#define TAG_TARGETTYPE_SEASON         60
#define TAG_TARGETTYPE_SEQUEL         60
#define TAG_TARGETTYPE_VOLUME         60

#define TAG_TARGETTYPE_ALBUM          50
#define TAG_TARGETTYPE_OPERA          50
#define TAG_TARGETTYPE_CONCERT        50
#define TAG_TARGETTYPE_MOVIE          50
#define TAG_TARGETTYPE_EPISODE        50

#define TAG_TARGETTYPE_PART           40
#define TAG_TARGETTYPE_SESSION        40

#define TAG_TARGETTYPE_TRACK          30
#define TAG_TARGETTYPE_SONG           30
#define TAG_TARGETTYPE_CHAPTER        30

#define TAG_TARGETTYPE_SUBTRACK       20
#define TAG_TARGETTYPE_MOVEMENT       20
#define TAG_TARGETTYPE_SCENE          20

#define TAG_TARGETTYPE_SHOT           10

// see http://www.matroska.org/technical/specs/index.html#physical
#define CHAPTER_PHYSEQUIV_SET          70
#define CHAPTER_PHYSEQUIV_PACKAGE      70

#define CHAPTER_PHYSEQUIV_CD           60
#define CHAPTER_PHYSEQUIV_12INCH       60
#define CHAPTER_PHYSEQUIV_10INCH       60
#define CHAPTER_PHYSEQUIV_7INCH        60
#define CHAPTER_PHYSEQUIV_TAPE         60
#define CHAPTER_PHYSEQUIV_MINIDISC     60
#define CHAPTER_PHYSEQUIV_DAT          60
#define CHAPTER_PHYSEQUIV_DVD          60
#define CHAPTER_PHYSEQUIV_VHS          60
#define CHAPTER_PHYSEQUIV_LASERDISC    60

#define CHAPTER_PHYSEQUIV_SIDE         50

#define CHAPTER_PHYSEQUIV_LAYER        40

#define CHAPTER_PHYSEQUIV_SESSION      30

#define CHAPTER_PHYSEQUIV_TRACK        20

#define CHAPTER_PHYSEQUIV_INDEX        10

#endif // MTX_COMMON_MATROSKA_H
