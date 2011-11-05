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

#include <ctype.h>
#include <stdarg.h>

#include <cassert>

#include <ebml/EbmlVersion.h>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxVersion.h>

#include "common/ebml.h"
#include "common/error.h"
#include "common/hacks.h"
#include "common/segmentinfo.h"
#include "common/version.h"

using namespace libmatroska;

/** \brief Parse a XML file containing segment info elements.

   The file \a file_name is opened and handed over to ::parse_xml_segmentinfo

   Its parameters don't have to be checked for validity.

   \param file_name The name of the text file to read from.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c NULL will be returned.

   \return A segment element containing the elements parsed from the file or
     \c NULL if an error occured.
*/
KaxInfo *
parse_segmentinfo(const std::string &file_name,
                  bool exception_on_error) {
  try {
    mm_text_io_c *in = new mm_text_io_c(new mm_file_io_c(file_name));
    return parse_xml_segmentinfo(in, exception_on_error);
  } catch (error_c &e) {
    if (exception_on_error)
      throw e;
    mxerror(e.get_error());
  }

  return NULL;
}

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
  if (NULL == e)
    return;

  KaxInfo *info = dynamic_cast<KaxInfo *>(e);
  if (NULL == info)
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
