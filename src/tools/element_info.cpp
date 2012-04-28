#include "element_info.h"

std::map<uint32_t, std::string> g_element_names;

void
init_element_names() {
  g_element_names[0xBF]       = "EBML::Crc32";
  g_element_names[0x4282]     = "EBML::DocType";
  g_element_names[0x4285]     = "EBML::DocTypeReadVersion";
  g_element_names[0x4287]     = "EBML::DocTypeVersion";
  g_element_names[0x1A45DFA3] = "EBML::Head";
  g_element_names[0x42F2]     = "EBML::MaxIdLength";
  g_element_names[0x42F3]     = "EBML::MaxSizeLength";
  g_element_names[0x42F7]     = "EBML::ReadVersion";
  g_element_names[0x4286]     = "EBML::Version";
  g_element_names[0xEC]       = "EBML::Void";

  g_element_names[0x61A7]     = "Attached";
  g_element_names[0x1941A469] = "Attachments";
  g_element_names[0x6264]     = "AudioBitDepth";
  g_element_names[0x9F]       = "AudioChannels";
  g_element_names[0x78B5]     = "AudioOutputSamplingFreq";
  g_element_names[0x7D7B]     = "AudioPosition";
  g_element_names[0xB5]       = "AudioSamplingFreq";
  g_element_names[0xA1]       = "Block";
  g_element_names[0xEE]       = "BlockAddID";
  g_element_names[0xA5]       = "BlockAdditional";
  g_element_names[0x75A1]     = "BlockAdditions";
  g_element_names[0x9B]       = "BlockDuration";
  g_element_names[0xA0]       = "BlockGroup";
  g_element_names[0xA6]       = "BlockMore";
  g_element_names[0xA2]       = "BlockVirtual";
  g_element_names[0xB6]       = "ChapterAtom";
  g_element_names[0x437E]     = "ChapterCountry";
  g_element_names[0x80]       = "ChapterDisplay";
  g_element_names[0x4598]     = "ChapterFlagEnabled";
  g_element_names[0x98]       = "ChapterFlagHidden";
  g_element_names[0x437C]     = "ChapterLanguage";
  g_element_names[0x63C3]     = "ChapterPhysicalEquiv";
  g_element_names[0x6944]     = "ChapterProcess";
  g_element_names[0x6955]     = "ChapterProcessCodecID";
  g_element_names[0x6911]     = "ChapterProcessCommand";
  g_element_names[0x6933]     = "ChapterProcessData";
  g_element_names[0x450D]     = "ChapterProcessPrivate";
  g_element_names[0x6922]     = "ChapterProcessTime";
  g_element_names[0x1043A770] = "Chapters";
  g_element_names[0x6EBC]     = "ChapterSegmentEditionUID";
  g_element_names[0x6E67]     = "ChapterSegmentUID";
  g_element_names[0x85]       = "ChapterString";
  g_element_names[0x92]       = "ChapterTimeEnd";
  g_element_names[0x91]       = "ChapterTimeStart";
  g_element_names[0x8F]       = "ChapterTrack";
  g_element_names[0x89]       = "ChapterTrackNumber";
  g_element_names[0x6924]     = "ChapterTranslate";
  g_element_names[0x69BF]     = "ChapterTranslateCodec";
  g_element_names[0x69FC]     = "ChapterTranslateEditionUID";
  g_element_names[0x69A5]     = "ChapterTranslateID";
  g_element_names[0x73C4]     = "ChapterUID";
  g_element_names[0x1F43B675] = "Cluster";
  g_element_names[0xA7]       = "ClusterPosition";
  g_element_names[0xAB]       = "ClusterPrevSize";
  g_element_names[0x58D7]     = "ClusterSilentTrackNumber";
  g_element_names[0x5854]     = "ClusterSilentTracks";
  g_element_names[0xE7]       = "ClusterTimecode";
  g_element_names[0xAA]       = "CodecDecodeAll";
  g_element_names[0x26B240]   = "CodecDownloadURL";
  g_element_names[0x86]       = "CodecID";
  g_element_names[0x3B4040]   = "CodecInfoURL";
  g_element_names[0x258688]   = "CodecName";
  g_element_names[0x63A2]     = "CodecPrivate";
  g_element_names[0x3A9697]   = "CodecSettings";
  g_element_names[0xA4]       = "CodecState";
  g_element_names[0x4254]     = "ContentCompAlgo";
  g_element_names[0x5034]     = "ContentCompression";
  g_element_names[0x4255]     = "ContentCompSettings";
  g_element_names[0x47e1]     = "ContentEncAlgo";
  g_element_names[0x47e2]     = "ContentEncKeyID";
  g_element_names[0x6240]     = "ContentEncoding";
  g_element_names[0x5031]     = "ContentEncodingOrder";
  g_element_names[0x6d80]     = "ContentEncodings";
  g_element_names[0x5032]     = "ContentEncodingScope";
  g_element_names[0x5033]     = "ContentEncodingType";
  g_element_names[0x5035]     = "ContentEncryption";
  g_element_names[0x47e5]     = "ContentSigAlgo";
  g_element_names[0x47e6]     = "ContentSigHashAlgo";
  g_element_names[0x47e4]     = "ContentSigKeyID";
  g_element_names[0x47e3]     = "ContentSignature";
  g_element_names[0x5378]     = "CueBlockNumber";
  g_element_names[0xF1]       = "CueClusterPosition";
  g_element_names[0xEA]       = "CueCodecState";
  g_element_names[0xBB]       = "CuePoint";
  g_element_names[0x97]       = "CueRefCluster";
  g_element_names[0xEB]       = "CueRefCodecState";
  g_element_names[0xDB]       = "CueReference";
  g_element_names[0x535F]     = "CueRefNumber";
  g_element_names[0x96]       = "CueRefTime";
  g_element_names[0x1C53BB6B] = "Cues";
  g_element_names[0xB3]       = "CueTime";
  g_element_names[0xF7]       = "CueTrack";
  g_element_names[0xB7]       = "CueTrackPositions";
  g_element_names[0x4461]     = "DateUTC";
  g_element_names[0x4489]     = "Duration";
  g_element_names[0x45B9]     = "EditionEntry";
  g_element_names[0x45DB]     = "EditionFlagDefault";
  g_element_names[0x45BD]     = "EditionFlagHidden";
  g_element_names[0x45DD]     = "EditionFlagOrdered";
  g_element_names[0x45BC]     = "EditionUID";
  g_element_names[0x465C]     = "FileData";
  g_element_names[0x467E]     = "FileDescription";
  g_element_names[0x466E]     = "FileName";
  g_element_names[0x4675]     = "FileReferral";
  g_element_names[0x46AE]     = "FileUID";
  g_element_names[0x1549A966] = "Info";
  g_element_names[0x55EE]     = "MaxBlockAdditionID";
  g_element_names[0x4660]     = "MimeType";
  g_element_names[0x4D80]     = "MuxingApp";
  g_element_names[0x3E83BB]   = "NextFilename";
  g_element_names[0x3EB923]   = "NextUID";
  g_element_names[0x3C83AB]   = "PrevFilename";
  g_element_names[0x3CB923]   = "PrevUID";
  g_element_names[0xFB]       = "ReferenceBlock";
  g_element_names[0xFA]       = "ReferencePriority";
  g_element_names[0xFD]       = "ReferenceVirtual";
  g_element_names[0x4DBB]     = "Seek";
  g_element_names[0x114D9B74] = "SeekHead";
  g_element_names[0x53AB]     = "SeekID";
  g_element_names[0x53AC]     = "SeekPosition";
  g_element_names[0x18538067] = "Segment";
  g_element_names[0x4444]     = "SegmentFamily";
  g_element_names[0x7384]     = "SegmentFilename";
  g_element_names[0x73A4]     = "SegmentUID";
  g_element_names[0xA3]       = "SimpleBlock";
  g_element_names[0xCB]       = "SliceBlockAddID";
  g_element_names[0xCE]       = "SliceDelay";
  g_element_names[0xCF]       = "SliceDuration";
  g_element_names[0xCD]       = "SliceFrameNumber";
  g_element_names[0xCC]       = "SliceLaceNumber";
  g_element_names[0x8E]       = "Slices";
  g_element_names[0x7373]     = "Tag";
  g_element_names[0x45A4]     = "TagArchivalLocation";
  g_element_names[0x4EC3]     = "TagAttachment";
  g_element_names[0x5BA0]     = "TagAttachmentID";
  g_element_names[0x63C6]     = "TagAttachmentUID";
  g_element_names[0x41B4]     = "TagAudioEncryption";
  g_element_names[0x4199]     = "TagAudioGain";
  g_element_names[0x65C2]     = "TagAudioGenre";
  g_element_names[0x4189]     = "TagAudioPeak";
  g_element_names[0x41C5]     = "TagAudioSpecific";
  g_element_names[0x4488]     = "TagBibliography";
  g_element_names[0x4485]     = "TagBinary";
  g_element_names[0x41A1]     = "TagBPM";
  g_element_names[0x49C7]     = "TagCaptureDPI";
  g_element_names[0x49E1]     = "TagCaptureLightness";
  g_element_names[0x4934]     = "TagCapturePaletteSetting";
  g_element_names[0x4922]     = "TagCaptureSharpness";
  g_element_names[0x63C4]     = "TagChapterUID";
  g_element_names[0x4EC7]     = "TagCommercial";
  g_element_names[0x4987]     = "TagCropped";
  g_element_names[0x4EC8]     = "TagDate";
  g_element_names[0x4484]     = "TagDefault";
  g_element_names[0x41B6]     = "TagDiscTrack";
  g_element_names[0x63C9]     = "TagEditionUID";
  g_element_names[0x4431]     = "TagEncoder";
  g_element_names[0x6526]     = "TagEncodeSettings";
  g_element_names[0x4EC9]     = "TagEntity";
  g_element_names[0x41B1]     = "TagEqualisation";
  g_element_names[0x454E]     = "TagFile";
  g_element_names[0x67C9]     = "TagGeneral";
  g_element_names[0x6583]     = "TagGenres";
  g_element_names[0x4EC6]     = "TagIdentifier";
  g_element_names[0x4990]     = "TagImageSpecific";
  g_element_names[0x413A]     = "TagInitialKey";
  g_element_names[0x458C]     = "TagKeywords";
  g_element_names[0x22B59F]   = "TagLanguage";
  g_element_names[0x447A]     = "TagLangue";
  g_element_names[0x4EC5]     = "TagLegal";
  g_element_names[0x5243]     = "TagLength";
  g_element_names[0x45AE]     = "TagMood";
  g_element_names[0x4DC3]     = "TagMultiAttachment";
  g_element_names[0x5B7B]     = "TagMultiComment";
  g_element_names[0x5F7C]     = "TagMultiCommentComments";
  g_element_names[0x22B59D]   = "TagMultiCommentLanguage";
  g_element_names[0x5F7D]     = "TagMultiCommentName";
  g_element_names[0x4DC7]     = "TagMultiCommercial";
  g_element_names[0x5BBB]     = "TagMultiCommercialAddress";
  g_element_names[0x5BC0]     = "TagMultiCommercialEmail";
  g_element_names[0x5BD7]     = "TagMultiCommercialType";
  g_element_names[0x5BDA]     = "TagMultiCommercialURL";
  g_element_names[0x4DC8]     = "TagMultiDate";
  g_element_names[0x4460]     = "TagMultiDateDateBegin";
  g_element_names[0x4462]     = "TagMultiDateDateEnd";
  g_element_names[0x5BD8]     = "TagMultiDateType";
  g_element_names[0x4DC9]     = "TagMultiEntity";
  g_element_names[0x5BDC]     = "TagMultiEntityAddress";
  g_element_names[0x5BC1]     = "TagMultiEntityEmail";
  g_element_names[0x5BED]     = "TagMultiEntityName";
  g_element_names[0x5BD9]     = "TagMultiEntityType";
  g_element_names[0x5BDB]     = "TagMultiEntityURL";
  g_element_names[0x4DC6]     = "TagMultiIdentifier";
  g_element_names[0x6B67]     = "TagMultiIdentifierBinary";
  g_element_names[0x6B68]     = "TagMultiIdentifierString";
  g_element_names[0x5BAD]     = "TagMultiIdentifierType";
  g_element_names[0x4DC5]     = "TagMultiLegal";
  g_element_names[0x5B9B]     = "TagMultiLegalAddress";
  g_element_names[0x5BB2]     = "TagMultiLegalContent";
  g_element_names[0x5BBD]     = "TagMultiLegalType";
  g_element_names[0x5B34]     = "TagMultiLegalURL";
  g_element_names[0x5BC3]     = "TagMultiPrice";
  g_element_names[0x5B6E]     = "TagMultiPriceAmount";
  g_element_names[0x5B6C]     = "TagMultiPriceCurrency";
  g_element_names[0x5B6F]     = "TagMultiPricePriceDate";
  g_element_names[0x4DC4]     = "TagMultiTitle";
  g_element_names[0x5B33]     = "TagMultiTitleAddress";
  g_element_names[0x5BAE]     = "TagMultiTitleEdition";
  g_element_names[0x5BC9]     = "TagMultiTitleEmail";
  g_element_names[0x22B59E]   = "TagMultiTitleLanguage";
  g_element_names[0x5BB9]     = "TagMultiTitleName";
  g_element_names[0x5B5B]     = "TagMultiTitleSubTitle";
  g_element_names[0x5B7D]     = "TagMultiTitleType";
  g_element_names[0x5BA9]     = "TagMultiTitleURL";
  g_element_names[0x45A3]     = "TagName";
  g_element_names[0x4133]     = "TagOfficialAudioFileURL";
  g_element_names[0x413E]     = "TagOfficialAudioSourceURL";
  g_element_names[0x4933]     = "TagOriginalDimensions";
  g_element_names[0x45A7]     = "TagOriginalMediaType";
  g_element_names[0x4566]     = "TagPlayCounter";
  g_element_names[0x72CC]     = "TagPlaylistDelay";
  g_element_names[0x4532]     = "TagPopularimeter";
  g_element_names[0x45E3]     = "TagProduct";
  g_element_names[0x52BC]     = "TagRating";
  g_element_names[0x457E]     = "TagRecordLocation";
  g_element_names[0x1254C367] = "Tags";
  g_element_names[0x416E]     = "TagSetPart";
  g_element_names[0x67C8]     = "TagSimple";
  g_element_names[0x458A]     = "TagSource";
  g_element_names[0x45B5]     = "TagSourceForm";
  g_element_names[0x4487]     = "TagString";
  g_element_names[0x65AC]     = "TagSubGenre";
  g_element_names[0x49C1]     = "TagSubject";
  g_element_names[0x63C0]     = "TagTargets";
  g_element_names[0x63CA]     = "TagTargetType";
  g_element_names[0x68CA]     = "TagTargetTypeValue";
  g_element_names[0x4EC4]     = "TagTitle";
  g_element_names[0x63C5]     = "TagTrackUID";
  g_element_names[0x874B]     = "TagUnsynchronisedText";
  g_element_names[0x434A]     = "TagUserDefinedURL";
  g_element_names[0x65A1]     = "TagVideoGenre";
  g_element_names[0x2AD7B1]   = "TimecodeScale";
  g_element_names[0xE8]       = "TimeSlice";
  g_element_names[0x7BA9]     = "Title";
  g_element_names[0x7446]     = "TrackAttachmentLink";
  g_element_names[0xE1]       = "TrackAudio";
  g_element_names[0x23E383]   = "TrackDefaultDuration";
  g_element_names[0xAE]       = "TrackEntry";
  g_element_names[0x88]       = "TrackFlagDefault";
  g_element_names[0xB9]       = "TrackFlagEnabled";
  g_element_names[0x55AA]     = "TrackFlagForced";
  g_element_names[0x9C]       = "TrackFlagLacing";
  g_element_names[0x22B59C]   = "TrackLanguage";
  g_element_names[0x6DF8]     = "TrackMaxCache";
  g_element_names[0x6DE7]     = "TrackMinCache";
  g_element_names[0x536E]     = "TrackName";
  g_element_names[0xD7]       = "TrackNumber";
  g_element_names[0x6FAB]     = "TrackOverlay";
  g_element_names[0x1654AE6B] = "Tracks";
  g_element_names[0x23314F]   = "TrackTimecodeScale";
  g_element_names[0x6624]     = "TrackTranslate";
  g_element_names[0x66BF]     = "TrackTranslateCodec";
  g_element_names[0x66FC]     = "TrackTranslateEditionUID";
  g_element_names[0x66A5]     = "TrackTranslateTrackID";
  g_element_names[0x83]       = "TrackType";
  g_element_names[0x73C5]     = "TrackUID";
  g_element_names[0xE0]       = "TrackVideo";
  g_element_names[0x54B3]     = "VideoAspectRatio";
  g_element_names[0x2EB524]   = "VideoColourSpace";
  g_element_names[0x54BA]     = "VideoDisplayHeight";
  g_element_names[0x54B2]     = "VideoDisplayUnit";
  g_element_names[0x54B0]     = "VideoDisplayWidth";
  g_element_names[0x9A]       = "VideoFlagInterlaced";
  g_element_names[0x2383E3]   = "VideoFrameRate";
  g_element_names[0x2FB523]   = "VideoGamma";
  g_element_names[0x54AA]     = "VideoPixelCropBottom";
  g_element_names[0x54CC]     = "VideoPixelCropLeft";
  g_element_names[0x54DD]     = "VideoPixelCropRight";
  g_element_names[0x54BB]     = "VideoPixelCropTop";
  g_element_names[0xBA]       = "VideoPixelHeight";
  g_element_names[0xB0]       = "VideoPixelWidth";
  g_element_names[0x53B8]     = "VideoStereoMode";
  g_element_names[0x5741]     = "WritingApp";
}

uint32_t
element_name_to_id(const std::string &name) {
  std::map<uint32_t, std::string>::const_iterator i;
  for (i = g_element_names.begin(); g_element_names.end() != i; ++i)
    if (i->second == name)
      return i->first;

  return 0;
}

std::map<uint32_t, bool> g_master_information;

void
init_master_information() {
  const char *s_master_names[] = {
    "Attached",
    "Attachments",
    "TimeSlice",
    "Slices",
    "BlockGroup",
    "BlockAdditions",
    "BlockMore",
    "Chapters",
    "EditionEntry",
    "ChapterAtom",
    "ChapterTrack",
    "ChapterDisplay",
    "ChapterProcess",
    "ChapterProcessCommand",
    "ClusterSilentTracks",
    "Cluster",
    "ContentEncodings",
    "ContentEncoding",
    "ContentCompression",
    "ContentEncryption",
    "CuePoint",
    "CueTrackPositions",
    "CueReference",
    "Cues",
    "ChapterTranslate",
    "Info",
    "SeekHead",
    "Seek",
    "Segment",
    "Tag",
    "TagTargets",
    "TagGeneral",
    "TagGenres",
    "TagAudioSpecific",
    "TagImageSpecific",
    "TagSimple",
    "TagMultiComment",
    "TagMultiCommercial",
    "TagCommercial",
    "TagMultiPrice",
    "TagMultiDate",
    "TagDate",
    "TagMultiEntity",
    "TagEntity",
    "TagMultiIdentifier",
    "TagIdentifier",
    "TagMultiLegal",
    "TagLegal",
    "TagMultiTitle",
    "TagTitle",
    "TagMultiAttachment",
    "TagAttachment",
    "Tags",
    "TrackAudio",
    "TrackTranslate",
    "Tracks",
    "TrackEntry",
    "TrackVideo",
    nullptr
  };

  int i;
  for (i = 0; s_master_names[i]; ++i) {
    uint32_t id = element_name_to_id(s_master_names[i]);
    if (0 != id)
      g_master_information[id] = true;
  }
}
