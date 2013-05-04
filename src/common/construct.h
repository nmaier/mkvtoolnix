/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CONSTRUCT_H
#define MTX_COMMON_CONSTRUCT_H

#include "common/common_pch.h"

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlDate.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>

#include "common/strings/utf8.h"

namespace mtx {
namespace construct {

using namespace libebml;

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlDate, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(value));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlUInteger, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(value));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlSInteger, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(value));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlFloat, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(value));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlString, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(value));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlUnicodeString, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  master->PushElement(object->SetValue(to_wide(value).c_str()));
}

template<typename Tobject,
         typename Tvalue>
inline typename std::enable_if< std::is_base_of<EbmlBinary, Tobject>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value) {
  if (!object)
    return;
  object->CopyBuffer(static_cast<binary *>(value->get_buffer()), value->get_size());
  master->PushElement(*object);
}

inline void
cons_impl(EbmlMaster *master,
          EbmlMaster *sub_master) {
  if (!sub_master)
    return;
  master->PushElement(*sub_master);
}

template<typename... Targs>
inline void
cons_impl(EbmlMaster *master,
          EbmlMaster *sub_master,
          Targs... args) {
  if (sub_master)
    master->PushElement(*sub_master);
  cons_impl(master, args...);
}

template<typename Tobject,
         typename Tvalue,
         typename... Targs>
inline typename std::enable_if< !std::is_convertible<Tobject *, EbmlMaster *>::value >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue const &value,
          Targs... args) {
  if (object)
    cons_impl(master, object, value);
  cons_impl(master, args...);
}

template<typename T>
EbmlMaster *
master() {
  T *master = new T;
  for (auto element : *master)
    delete element;
  master->RemoveAll();
  return master;
}

template<typename Tmaster>
inline EbmlMaster *
cons() {
  return master<Tmaster>();
}

template<typename Tmaster,
         typename... Targs>
inline EbmlMaster *
cons(Targs... args) {
  auto master = cons<Tmaster>();
  cons_impl(master, args...);

  return master;
}

}}

#endif // MTX_COMMON_CONSTRUCT_H
