/**
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <string>
#include <vector>

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/common_pch.h"
#include "common/ebml.h"
#include "common/property_element.h"
#include "common/translation.h"

std::map<uint32_t, std::vector<property_element_c> > property_element_c::s_properties;
std::map<uint32_t, std::vector<property_element_c> > property_element_c::s_composed_properties;

property_element_c::property_element_c(const std::string &name,
                                       const EbmlCallbacks &callbacks,
                                       const translatable_string_c &title,
                                       const translatable_string_c &description,
                                       const EbmlCallbacks &sub_master_callbacks)
  : m_name(name)
  , m_title(title)
  , m_description(description)
  , m_callbacks(&callbacks)
  , m_sub_master_callbacks(&sub_master_callbacks)
  , m_type(EBMLT_SKIP)
{
  derive_type();
}

property_element_c::property_element_c()
  : m_callbacks(nullptr)
  , m_sub_master_callbacks(nullptr)
  , m_type(EBMLT_SKIP)
{
}

bool
property_element_c::is_valid()
  const
{
  return !m_name.empty() && (m_callbacks) && (EBMLT_SKIP != m_type);
}

void
property_element_c::derive_type() {
  EbmlElement *e = &m_callbacks->Create();

  m_type = dynamic_cast<EbmlBinary *>(e)        ? EBMLT_BINARY
         : dynamic_cast<EbmlFloat *>(e)         ? EBMLT_FLOAT
         : dynamic_cast<EbmlSInteger *>(e)      ? EBMLT_INT
         : dynamic_cast<EbmlString *>(e)        ? EBMLT_STRING
         : dynamic_cast<EbmlUInteger *>(e)      ? EBMLT_UINT
         : dynamic_cast<EbmlUnicodeString *>(e) ? EBMLT_USTRING
         :                                        EBMLT_SKIP;

  if (EBMLT_SKIP == m_type)
    mxerror(boost::format("property_element_c::derive_type(): programming error: unknown type for EBML ID %|1$08x|\n") % m_callbacks->GlobalId.Value);

  if ((EBMLT_UINT == m_type) && (m_name.find("flag") != std::string::npos))
    m_type = EBMLT_BOOL;

  delete e;
}

#define ELE(name, callbacks, title, description) s_properties[current_index].push_back(property_element_c(name, callbacks, title, description, *sub_master_callbacks))

void
property_element_c::init_tables() {
  const EbmlCallbacks *sub_master_callbacks = nullptr;

  s_properties.clear();

  s_properties[KaxInfo::ClassInfos.GlobalId.Value]   = std::vector<property_element_c>();
  s_properties[KaxTracks::ClassInfos.GlobalId.Value] = std::vector<property_element_c>();
  uint32_t current_index                             = KaxInfo::ClassInfos.GlobalId.Value;

  ELE("title",                KaxTitle::ClassInfos,           Y("Title"),                        Y("The title for the whole movie."));
  ELE("segment-filename",     KaxSegmentFilename::ClassInfos, Y("Segment filename"),             Y("The file name for this segment."));
  ELE("prev-filename",        KaxPrevFilename::ClassInfos,    Y("Previous filename"),            Y("An escaped filename corresponding to\nthe previous segment."));
  ELE("next-filename",        KaxNextFilename::ClassInfos,    Y("Next filename"),                Y("An escaped filename corresponding to\nthe next segment."));
  ELE("segment-uid",          KaxSegmentUID::ClassInfos,      Y("Segment unique ID"),            Y("A randomly generated unique ID to identify the current\n"
                                                                                                   "segment between many others (128 bits)."));
  ELE("prev-uid",             KaxPrevUID::ClassInfos,         Y("Previous segment's unique ID"), Y("A unique ID to identify the previous chained\nsegment (128 bits)."));
  ELE("next-uid",             KaxNextUID::ClassInfos,         Y("Next segment's unique ID"),     Y("A unique ID to identify the next chained\nsegment (128 bits)."));

  current_index = KaxTracks::ClassInfos.GlobalId.Value;

  ELE("track-number",         KaxTrackNumber::ClassInfos,          Y("Track number"),          Y("The track number as used in the Block Header."));
  ELE("track-uid",            KaxTrackUID::ClassInfos,             Y("Track UID"),             Y("A unique ID to identify the Track. This should be\nkept the same when making a "
                                                                                                 "direct stream copy\nof the Track to another file."));
  ELE("flag-default",         KaxTrackFlagDefault::ClassInfos,     Y("'Default track' flag"),  Y("Set if that track (audio, video or subs) SHOULD\nbe used if no language found matches the\n"
                                                                                                 "user preference."));
  ELE("flag-enabled",         KaxTrackFlagEnabled::ClassInfos,     Y("'Track enabled' flag"),  Y("Set if the track is used."));
  ELE("flag-forced",          KaxTrackFlagForced::ClassInfos,      Y("'Forced display' flag"), Y("Set if that track MUST be used during playback.\n"
                                                                                                 "There can be many forced track for a kind (audio,\nvideo or subs). "
                                                                                                 "The player should select the one\nwhose language matches the user preference or the\n"
                                                                                                 "default + forced track."));
  ELE("min-cache",            KaxTrackMinCache::ClassInfos,        Y("Minimum cache"),         Y("The minimum number of frames a player\nshould be able to cache during playback.\n"
                                                                                                 "If set to 0, the reference pseudo-cache system\nis not used."));
  ELE("max-cache",            KaxTrackMaxCache::ClassInfos,        Y("Maximum cache"),         Y("The maximum number of frames a player\nshould be able to cache during playback.\n"
                                                                                                 "If set to 0, the reference pseudo-cache system\nis not used."));
  ELE("default-duration",     KaxTrackDefaultDuration::ClassInfos, Y("Default duration"),      Y("Number of nanoseconds (not scaled) per frame."));
  ELE("track-timecode-scale", KaxTrackTimecodeScale::ClassInfos,   Y("Timecode scaling"),      Y("The scale to apply on this track to work at normal\nspeed in relation with other tracks "
                                                                                                 "(mostly used\nto adjust video speed when the audio length differs)."));
  ELE("name",                 KaxTrackName::ClassInfos,            Y("Name"),                  Y("A human-readable track name."));
  ELE("language",             KaxTrackLanguage::ClassInfos,        Y("Language"),              Y("Specifies the language of the track in the\nMatroska languages form."));
  ELE("codec-id",             KaxCodecID::ClassInfos,              Y("Codec ID"),              Y("An ID corresponding to the codec."));
  ELE("codec-name",           KaxCodecName::ClassInfos,            Y("Codec name"),            Y("A human-readable string specifying the codec."));

  sub_master_callbacks = &KaxTrackVideo::ClassInfos;

  ELE("interlaced",        KaxVideoFlagInterlaced::ClassInfos,  Y("Video interlaced flag"),   Y("Set if the video is interlaced."));
  ELE("pixel-width",       KaxVideoPixelWidth::ClassInfos,      Y("Video pixel width"),       Y("Width of the encoded video frames in pixels."));
  ELE("pixel-height",      KaxVideoPixelHeight::ClassInfos,     Y("Video pixel height"),      Y("Height of the encoded video frames in pixels."));
  ELE("display-width",     KaxVideoDisplayWidth::ClassInfos,    Y("Video display width"),     Y("Width of the video frames to display."));
  ELE("display-height",    KaxVideoDisplayHeight::ClassInfos,   Y("Video display height"),    Y("Height of the video frames to display."));
  ELE("display-unit",      KaxVideoDisplayUnit::ClassInfos,     Y("Video display unit"),      Y("Type of the unit for DisplayWidth/Height\n(0: pixels, 1: centimeters, 2: inches)."));
  ELE("pixel-crop-left",   KaxVideoPixelCropLeft::ClassInfos,   Y("Video crop left"),         Y("The number of video pixels to remove\non the left of the image."));
  ELE("pixel-crop-top",    KaxVideoPixelCropTop::ClassInfos,    Y("Video crop top"),          Y("The number of video pixels to remove\non the top of the image."));
  ELE("pixel-crop-right",  KaxVideoPixelCropRight::ClassInfos,  Y("Video crop right"),        Y("The number of video pixels to remove\non the right of the image."));
  ELE("pixel-crop-bottom", KaxVideoPixelCropBottom::ClassInfos, Y("Video crop bottom"),       Y("The number of video pixels to remove\non the bottom of the image."));
  ELE("aspect-ratio-type", KaxVideoAspectRatio::ClassInfos,     Y("Video aspect ratio type"), Y("Specify the possible modifications to the aspect ratio\n"
                                                                                                "(0: free resizing, 1: keep aspect ratio, 2: fixed)."));
  ELE("stereo-mode",       KaxVideoStereoMode::ClassInfos,      Y("Video stereo mode"),       Y("Stereo-3D video mode (0: mono, 1: right eye,\n2: left eye, 3: both eyes)."));

  sub_master_callbacks = &KaxTrackAudio::ClassInfos;

  ELE("sampling-frequency",        KaxAudioSamplingFreq::ClassInfos,       Y("Audio sampling frequency"),        Y("Sampling frequency in Hz."));
  ELE("output-sampling-frequency", KaxAudioOutputSamplingFreq::ClassInfos, Y("Audio output sampling frequency"), Y("Real output sampling frequency in Hz."));
  ELE("channels",                  KaxAudioChannels::ClassInfos,           Y("Audio channels"),                  Y("Numbers of channels in the track."));
  ELE("bit-depth",                 KaxAudioBitDepth::ClassInfos,           Y("Audio bit depth"),                 Y("Bits per sample, mostly used for PCM."));
}

std::vector<property_element_c> &
property_element_c::get_table_for(const EbmlCallbacks &master_callbacks,
                                  const EbmlCallbacks *sub_master_callbacks,
                                  bool full_table) {
  if (s_properties.empty())
    init_tables();

  std::map<uint32_t, std::vector<property_element_c> >::iterator src_map_it = s_properties.find(master_callbacks.GlobalId.Value);
  if (s_properties.end() == src_map_it)
    mxerror(boost::format("property_element_c::get_table_for(): programming error: no table found for EBML ID %|1$08x|\n") % master_callbacks.GlobalId.Value);

  if (full_table)
    return src_map_it->second;

  uint32_t element_id = !sub_master_callbacks ? master_callbacks.GlobalId.Value : sub_master_callbacks->GlobalId.Value;
  std::map<uint32_t, std::vector<property_element_c> >::iterator composed_map_it = s_composed_properties.find(element_id);
  if (s_composed_properties.end() != composed_map_it)
    return composed_map_it->second;

  s_composed_properties[element_id]      = std::vector<property_element_c>();
  std::vector<property_element_c> &table = s_composed_properties[element_id];

  for (auto &property : src_map_it->second)
    if (!property.m_sub_master_callbacks || (sub_master_callbacks && (sub_master_callbacks->GlobalId == property.m_sub_master_callbacks->GlobalId)))
      table.push_back(property);

  return table;
}
