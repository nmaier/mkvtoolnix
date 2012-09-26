/** \brief segment info parser and helper functions

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>
*/

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/ebml.h"
#include "common/segment_tracks.h"

using namespace libmatroska;

static void
fix_mandatory_track_video_elements(KaxTrackVideo *track_video) {
  GetChild<KaxVideoPixelWidth>(track_video);
  GetChild<KaxVideoPixelHeight>(track_video);
  GetChild<KaxVideoFlagInterlaced>(track_video);
}

static void
fix_mandatory_track_audio_elements(KaxTrackAudio *track_audio) {
  GetChild<KaxAudioSamplingFreq>(track_audio);
  GetChild<KaxAudioChannels>(track_audio);
}

static void
fix_mandatory_content_compression_elements(KaxContentCompression *compression) {
  GetChild<KaxContentCompAlgo>(compression);
}

static void
fix_mandatory_content_encoding_elements(KaxContentEncoding *encoding) {
  GetChild<KaxContentEncodingOrder>(encoding);
  GetChild<KaxContentEncodingScope>(encoding);
  GetChild<KaxContentEncodingType>(encoding);

  size_t i;
  for (i = 0; encoding->ListSize() > i; ++i) {
    EbmlElement *e = (*encoding)[i];

    if (dynamic_cast<KaxContentCompression *>(e))
      fix_mandatory_content_compression_elements(static_cast<KaxContentCompression *>(e));
  }
}

static void
fix_mandatory_content_encodings_elements(KaxContentEncodings *encodings) {
  size_t i;
  for (i = 0; encodings->ListSize() > i; ++i) {
    EbmlElement *e = (*encodings)[i];

    if (dynamic_cast<KaxContentEncoding *>(e))
      fix_mandatory_content_encoding_elements(static_cast<KaxContentEncoding *>(e));
  }
}

static void
fix_mandatory_track_entry_elements(KaxTrackEntry *track_entry) {
  // Deprecated element that must not be rendered anymore
  DeleteChildren<KaxTrackTimecodeScale>(track_entry);

  GetChild<KaxTrackNumber>(track_entry);
  GetChild<KaxTrackUID>(track_entry);
  GetChild<KaxTrackType>(track_entry);
  GetChild<KaxTrackFlagEnabled>(track_entry);
  GetChild<KaxTrackFlagDefault>(track_entry);
  GetChild<KaxTrackFlagForced>(track_entry);
  GetChild<KaxTrackFlagLacing>(track_entry);
  GetChild<KaxTrackMinCache>(track_entry);
  GetChild<KaxMaxBlockAdditionID>(track_entry);
  GetChild<KaxCodecID>(track_entry);
  GetChild<KaxCodecDecodeAll>(track_entry);

  size_t i;
  for (i = 0; track_entry->ListSize() > i; ++i) {
    EbmlElement *e = (*track_entry)[i];

    if (dynamic_cast<KaxTrackVideo *>(e))
      fix_mandatory_track_video_elements(static_cast<KaxTrackVideo *>(e));

    else if (dynamic_cast<KaxTrackAudio *>(e))
      fix_mandatory_track_audio_elements(static_cast<KaxTrackAudio *>(e));

    else if (dynamic_cast<KaxContentEncodings *>(e))
      fix_mandatory_content_encodings_elements(static_cast<KaxContentEncodings *>(e));
  }
}

/** \brief Add missing mandatory track header elements

   The Matroska specs and \c libmatroska say that several elements are
   mandatory. This function makes sure that they all exist by adding them
   with their default values if they're missing. It works recursively.

   The parameters are checked for validity.

   \param e An element that really is an \c EbmlMaster. \a e's children
     should be checked.
*/
void
fix_mandatory_segment_tracks_elements(EbmlElement *e) {
  if (!e)
    return;

  if (dynamic_cast<KaxTrackEntry *>(e)) {
    fix_mandatory_track_entry_elements(static_cast<KaxTrackEntry *>(e));
    return;
  }

  KaxTracks *tracks = dynamic_cast<KaxTracks *>(e);
  if (!tracks)
    return;

  size_t i;
  for (i = 0; tracks->ListSize() > i; ++i) {
    e = (*tracks)[i];

    if (dynamic_cast<KaxTrackEntry *>(e))
      fix_mandatory_track_entry_elements(static_cast<KaxTrackEntry *>(e));
  }
}
