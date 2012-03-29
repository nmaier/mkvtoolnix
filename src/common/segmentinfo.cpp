/** \brief segment info parser and helper functions

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org> and
   \author Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <ebml/EbmlVersion.h>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxVersion.h>

#include "common/ebml.h"
#include "common/hacks.h"
#include "common/segmentinfo.h"
#include "common/version.h"
#include "common/xml/element_parser.h"

using namespace libmatroska;

/** \brief Add missing mandatory elements

   The Matroska specs and \c libmatroska say that several elements are
   mandatory. This function makes sure that they all exist by adding them
   with their default values if they're missing.

   The parameters are checked for validity.

   \param e An element that really is an \c EbmlMaster. \a e's children
     should be checked.
*/
void
fix_mandatory_segmentinfo_elements(EbmlElement *e) {
  if (nullptr == e)
    return;

  KaxInfo *info = dynamic_cast<KaxInfo *>(e);
  if (nullptr == info)
    return;

  GetChild<KaxTimecodeScale>(info);

  if (!hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
    provide_default_for_child<KaxMuxingApp>( info, cstrutf8_to_UTFstring((boost::format("libebml v%1% + libmatroska v%2%") %  EbmlCodeVersion % KaxCodeVersion).str()));
    provide_default_for_child<KaxWritingApp>(info, cstrutf8_to_UTFstring(get_version_info("mkvmerge", static_cast<version_info_flags_e>(vif_full | vif_untranslated))));
  } else {
    provide_default_for_child<KaxMuxingApp>( info, cstrutf8_to_UTFstring("no_variable_data"));
    provide_default_for_child<KaxWritingApp>(info, cstrutf8_to_UTFstring("no_variable_data"));
  }
}
