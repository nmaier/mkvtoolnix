#include "common/common_pch.h"

#include "common/codec.h"
#include "common/mp4.h"

#include "gtest/gtest.h"

namespace {

TEST(Codec, LookUpStringAudio) {
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_2MAIN).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_2LC).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_2SSR).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_2SBR).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4MAIN).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4LC).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4LC).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4SSR).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4LTP).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC_4SBR).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_AAC).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("mp4a").is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("aac ").is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("aacl").is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("aach").is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("raac").is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up("racp").is(CT_A_AAC));

  EXPECT_TRUE(codec_c::look_up(MKV_A_AC3).is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up(MKV_A_EAC3).is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up("a52 ").is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up("a52b").is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up("ac-3").is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up("sac3").is(CT_A_AC3));

  EXPECT_TRUE(codec_c::look_up(MKV_A_ALAC).is(CT_A_ALAC));
  EXPECT_TRUE(codec_c::look_up("ALAC").is(CT_A_ALAC));

  EXPECT_TRUE(codec_c::look_up(MKV_A_DTS).is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dts ").is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dtsb").is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dtsc").is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dtse").is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dtsh").is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up("dtsl").is(CT_A_DTS));

  EXPECT_TRUE(codec_c::look_up(MKV_A_MP2).is(CT_A_MP2));
  EXPECT_TRUE(codec_c::look_up("mp2a").is(CT_A_MP2));
  EXPECT_TRUE(codec_c::look_up(".mp1").is(CT_A_MP2));
  EXPECT_TRUE(codec_c::look_up(".mp2").is(CT_A_MP2));

  EXPECT_TRUE(codec_c::look_up(MKV_A_MP3).is(CT_A_MP3));
  EXPECT_TRUE(codec_c::look_up("mp3a").is(CT_A_MP3));
  EXPECT_TRUE(codec_c::look_up(".mp3").is(CT_A_MP3));

  EXPECT_TRUE(codec_c::look_up(MKV_A_PCM).is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up(MKV_A_PCM_BE).is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up(MKV_A_PCM_FLOAT).is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up("twos").is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up("sowt").is(CT_A_PCM));

  EXPECT_TRUE(codec_c::look_up(MKV_A_VORBIS).is(CT_A_VORBIS));
  EXPECT_TRUE(codec_c::look_up("vorb").is(CT_A_VORBIS));
  EXPECT_TRUE(codec_c::look_up("vor1").is(CT_A_VORBIS));

  EXPECT_TRUE(codec_c::look_up(MKV_A_OPUS).is(CT_A_OPUS));
  EXPECT_TRUE(codec_c::look_up(std::string{MKV_A_OPUS} + "/EXPERIMENTAL").is(CT_A_OPUS));
  EXPECT_TRUE(codec_c::look_up("opus").is(CT_A_OPUS));

  EXPECT_TRUE(codec_c::look_up(MKV_A_QDMC).is(CT_A_QDMC));
  EXPECT_TRUE(codec_c::look_up(MKV_A_QDMC2).is(CT_A_QDMC));
  EXPECT_TRUE(codec_c::look_up("qdm2").is(CT_A_QDMC));

  EXPECT_TRUE(codec_c::look_up(MKV_A_FLAC).is(CT_A_FLAC));
  EXPECT_TRUE(codec_c::look_up("flac").is(CT_A_FLAC));

  EXPECT_TRUE(codec_c::look_up(MKV_A_MLP).is(CT_A_MLP));
  EXPECT_TRUE(codec_c::look_up("mlp ").is(CT_A_MLP));

  EXPECT_TRUE(codec_c::look_up(MKV_A_TRUEHD).is(CT_A_TRUEHD));
  EXPECT_TRUE(codec_c::look_up("trhd").is(CT_A_TRUEHD));

  EXPECT_TRUE(codec_c::look_up(MKV_A_TTA).is(CT_A_TTA));
  EXPECT_TRUE(codec_c::look_up("tta1").is(CT_A_TTA));

  EXPECT_TRUE(codec_c::look_up(MKV_A_WAVPACK4).is(CT_A_WAVPACK4));
  EXPECT_TRUE(codec_c::look_up("wvpk").is(CT_A_WAVPACK4));
}

TEST(Codec, LookUpStringVideo) {
  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG1).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG2).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("h262").is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("mp1v").is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("mp2v").is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("mpeg").is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("mpg2").is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up("mpgv").is(CT_V_MPEG12));

  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG4_SP).is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG4_ASP).is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG4_AP).is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("3iv2").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("divx").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("dx50").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("fmp4").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("mp4v").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("xvid").is(CT_V_MPEG4_P2));
  EXPECT_TRUE(codec_c::look_up("xvix").is(CT_V_MPEG4_P2));

  EXPECT_TRUE(codec_c::look_up(MKV_V_MPEG4_AVC).is(CT_V_MPEG4_P10));
  EXPECT_TRUE(codec_c::look_up("h264").is(CT_V_MPEG4_P10));
  EXPECT_TRUE(codec_c::look_up("x264").is(CT_V_MPEG4_P10));
  EXPECT_TRUE(codec_c::look_up("avc1").is(CT_V_MPEG4_P10));

  EXPECT_TRUE(codec_c::look_up(MKV_V_REALV1).is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up(MKV_V_REALV2).is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up(MKV_V_REALV3).is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up(MKV_V_REALV4).is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up("rv10").is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up("rv20").is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up("rv30").is(CT_V_REAL));
  EXPECT_TRUE(codec_c::look_up("rv40").is(CT_V_REAL));

  EXPECT_TRUE(codec_c::look_up(MKV_V_THEORA).is(CT_V_THEORA));
  EXPECT_TRUE(codec_c::look_up("theo").is(CT_V_THEORA));
  EXPECT_TRUE(codec_c::look_up("thra").is(CT_V_THEORA));

  EXPECT_TRUE(codec_c::look_up(MKV_V_DIRAC).is(CT_V_DIRAC));
  EXPECT_TRUE(codec_c::look_up("drac").is(CT_V_DIRAC));

  EXPECT_TRUE(codec_c::look_up(MKV_V_VP8).is(CT_V_VP8));
  EXPECT_TRUE(codec_c::look_up("vp80").is(CT_V_VP8));

  EXPECT_TRUE(codec_c::look_up(MKV_V_VP9).is(CT_V_VP9));
  EXPECT_TRUE(codec_c::look_up("vp90").is(CT_V_VP9));

  EXPECT_TRUE(codec_c::look_up("svq1").is(CT_V_SVQ));
  EXPECT_TRUE(codec_c::look_up("svq1").is(CT_V_SVQ));
  EXPECT_TRUE(codec_c::look_up("svqi").is(CT_V_SVQ));
  EXPECT_TRUE(codec_c::look_up("svq3").is(CT_V_SVQ));

  EXPECT_TRUE(codec_c::look_up("vc-1").is(CT_V_VC1));
  EXPECT_TRUE(codec_c::look_up("wvc1").is(CT_V_VC1));
}

TEST(Codec, LookUpStringSubtitles) {
  EXPECT_TRUE(codec_c::look_up(MKV_S_TEXTUTF8).is(CT_S_SRT));
  EXPECT_TRUE(codec_c::look_up(MKV_S_TEXTASCII).is(CT_S_SRT));

  EXPECT_TRUE(codec_c::look_up(MKV_S_TEXTSSA).is(CT_S_SSA_ASS));
  EXPECT_TRUE(codec_c::look_up(MKV_S_TEXTASS).is(CT_S_SSA_ASS));
  EXPECT_TRUE(codec_c::look_up("ssa ").is(CT_S_SSA_ASS));
  EXPECT_TRUE(codec_c::look_up("ass ").is(CT_S_SSA_ASS));

  EXPECT_TRUE(codec_c::look_up(MKV_S_TEXTUSF).is(CT_S_USF));
  EXPECT_TRUE(codec_c::look_up("usf ").is(CT_S_USF));

  EXPECT_TRUE(codec_c::look_up(MKV_S_VOBSUB).is(CT_S_VOBSUB));
  EXPECT_TRUE(codec_c::look_up(MKV_S_VOBSUBZLIB).is(CT_S_VOBSUB));

  EXPECT_TRUE(codec_c::look_up(MKV_S_KATE).is(CT_S_KATE));
  EXPECT_TRUE(codec_c::look_up("kate").is(CT_S_KATE));

  EXPECT_TRUE(codec_c::look_up(MKV_S_HDMV_PGS).is(CT_S_PGS));
}

TEST(Codec, LookUpAudioFormat) {
  EXPECT_TRUE(codec_c::look_up_audio_format(0x00ffu).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x706du).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x2000u).is(CT_A_AC3));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x2001u).is(CT_A_DTS));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x0050u).is(CT_A_MP2));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x0055u).is(CT_A_MP3));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x0001u).is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x0003u).is(CT_A_PCM));
  EXPECT_TRUE(codec_c::look_up_audio_format(0x566fu).is(CT_A_VORBIS));
  EXPECT_TRUE(codec_c::look_up_audio_format(0xfffeu).is(CT_A_VORBIS));
}

TEST(Codec, LookUpObjectTypeId) {
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2AudioMain).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2AudioLowComplexity).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2AudioScaleableSamplingRate).is(CT_A_AAC));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2AudioPart3).is(CT_A_MP3));

  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG1Audio).is(CT_A_MP2));

  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_DTS).is(CT_A_DTS));

  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG4Visual).is(CT_V_MPEG4_P2));

  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2VisualSimple).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2VisualMain).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2VisualSNR).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2VisualSpatial).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2VisualHigh).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG2Visual422).is(CT_V_MPEG12));
  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_MPEG1Visual).is(CT_V_MPEG12));

  EXPECT_TRUE(codec_c::look_up_object_type_id(MP4OTI_VOBSUB).is(CT_S_VOBSUB));
}

TEST(Codec, LookUpOverloading) {
  EXPECT_TRUE(codec_c::look_up("kate").is(CT_S_KATE));
  EXPECT_TRUE(codec_c::look_up(std::string{"kate"}).is(CT_S_KATE));
  EXPECT_TRUE(codec_c::look_up(fourcc_c{"kate"}).is(CT_S_KATE));
  EXPECT_TRUE(codec_c::look_up(CT_S_KATE).is(CT_S_KATE));
}

TEST(Codec, LookUpValidity) {
  EXPECT_FALSE(codec_c::look_up("DOES-NOT-EXIST").valid());
  EXPECT_FALSE(codec_c::look_up_audio_format(0x0000u).valid());
  EXPECT_FALSE(codec_c::look_up_audio_format(0x4711).valid());
  EXPECT_FALSE(codec_c::look_up_object_type_id(0x0000u).valid());
  EXPECT_FALSE(codec_c::look_up_object_type_id(0x4711).valid());
}

TEST(Codec, HandlingOfUnknownCodec) {
  EXPECT_TRUE(codec_c::look_up(CT_UNKNOWN).is(CT_UNKNOWN));
  EXPECT_FALSE(codec_c::look_up(CT_UNKNOWN).valid());
}

TEST(Codec, GetNameFallbacks) {
  EXPECT_EQ(codec_c::look_up("DOES-NOT-EXIST").get_name("HelloWorld"), "HelloWorld");
  EXPECT_EQ(codec_c::look_up("DOES-NOT-EXIST").get_name(),             "");
}

}
