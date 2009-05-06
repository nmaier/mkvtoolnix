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

#include "common/os.h"

#include <ctype.h>
#include <stdarg.h>

#include <cassert>
#include <string>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>

#include "common/common.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/segmentinfo.h"

using namespace std;
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
KaxInfo *MTX_DLL_API
parse_segmentinfo(const string &file_name,
                  bool exception_on_error) {
  mm_text_io_c *in;

  try {
    in = new mm_text_io_c(new mm_file_io_c(file_name));
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
   with their default values if they're missing. It works recursively.

   The parameters are checked for validity.

   \param e An element that really is an \c EbmlMaster. \a e's children
     should be checked.
*/
void
fix_mandatory_segmentinfo_elements(EbmlElement *e) {
  if (e == NULL)
    return;

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_segmentinfo_elements((*m)[i]);
  }
}
