/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   writes tags in XML format

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include <matroska/KaxTags.h>

#include "common/xml/ebml_tags_converter.h"

using namespace libmatroska;

void
write_tags_xml(KaxTags &tags,
               mm_io_c *out) {
  mtx::xml::xml_document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> ");

  mtx::xml::ebml_tags_converter_c converter;
  converter.to_xml(&tags, doc);

  std::stringstream out_stream;
  doc->save(out_stream, "  ", pugi::format_default | pugi::format_write_bom);
  out->puts(out_stream.str());
}
