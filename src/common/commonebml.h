/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  commonkax.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief helper functions that need libebml/libmatroska
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __COMMONEBML_H
#define __COMMONEBML_H

#include "os.h"

#include <ebml/EbmlMaster.h>
#include <ebml/EbmlUnicodeString.h>

using namespace libebml;

#if defined(DEBUG)
void MTX_DLL_API __debug_dump_elements(EbmlElement *e, int level);
#define debug_dump_elements(e, l) __debug_dump_elements(e, l)
#else
#define debug_dump_elements(e, l) ;
#endif

#define can_be_cast(c, e) (dynamic_cast<c *>(e) != NULL)

UTFstring MTX_DLL_API cstr_to_UTFstring(const char *c);
UTFstring MTX_DLL_API cstrutf8_to_UTFstring(const char *c);
char *MTX_DLL_API UTFstring_to_cstr(const UTFstring &u);
char *MTX_DLL_API UTFstring_to_cstrutf8(const UTFstring &u);

#define is_id(e, ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)

#define FINDFIRST(p, c) (static_cast<c *> \
  (((EbmlMaster *)p)->FindFirstElt(c::ClassInfos, false)))
#define FINDNEXT(p, c, e) (static_cast<c *> \
  (((EbmlMaster *)p)->FindNextElt(*e, false)))

template <typename type>type &GetEmptyChild(EbmlMaster &master) {
  EbmlElement *e;
  EbmlMaster *m;

  e = master.FindFirstElt(type::ClassInfos, true);
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }

	return *(static_cast<type *>(e));
}

template <typename type>type &GetNextEmptyChild(EbmlMaster &master,
                                                const type &past_elt) {
  EbmlElement *e;
  EbmlMaster *m;

  e = master.FindNextElt(past_elt, true);
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }

	return *(static_cast<type *>(e));
}

template <typename type>type &AddEmptyChild(EbmlMaster &master) {
  EbmlElement *e;
  EbmlMaster *m;

  e = new type;
  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    while (m->ListSize() > 0) {
      delete (*m)[0];
      m->Remove(0);
    }
  }
  master.PushElement(*e);

	return *(static_cast<type *>(e));
}

#endif // __COMMONEBML_H
