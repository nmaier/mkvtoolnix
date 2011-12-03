/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: track type page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include <matroska/KaxTrackEntryData.h>

#include "common/common_pch.h"
#include "common/wx.h"
#include "mmg/header_editor/track_type_page.h"

he_track_type_page_c::he_track_type_page_c(header_editor_frame_c *parent,
                                           int track_type,
                                           unsigned int track_number,
                                           ebml_element_cptr l1_element)
  : he_top_level_page_c(parent, "" , l1_element)
  , m_track_type(track_type)
  , m_track_number(track_number)
{
}

he_track_type_page_c::~he_track_type_page_c() {
}

void
he_track_type_page_c::translate_ui() {
  wxString title;

  switch (m_track_type) {
    case track_audio:
      title.Printf(Z("Audio track %u"), m_track_number);
      break;
    case track_video:
      title.Printf(Z("Video track %u"), m_track_number);
      break;
    case track_subtitle:
      title.Printf(Z("Subtitle track %u"), m_track_number);
      break;
  }

  m_title = translatable_string_c(wxMB(title));
  if (NULL != m_st_title)
    m_st_title->SetLabel(title);

  he_top_level_page_c::translate_ui();
}
