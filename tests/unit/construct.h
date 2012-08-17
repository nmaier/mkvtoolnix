/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for helper functions for unit tests

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TESTS_UNIT_CONSTRUCT_H
#define MTX_TESTS_UNIT_CONSTRUCT_H

#include "common/common_pch.h"

#include <ebml/EbmlDate.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>

namespace mtxut {
namespace construct {

using namespace libebml;

template<typename Tobject,
         typename Tvalue>
typename boost::enable_if< boost::is_base_of<EbmlUInteger, Tobject> >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value) {
  *static_cast<EbmlUInteger *>(object) = value;
  master->PushElement(*object);
}

template<typename Tobject,
         typename Tvalue>
typename boost::enable_if< boost::is_base_of<EbmlSInteger, Tobject> >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value) {
  *static_cast<EbmlSInteger *>(object) = value;
  master->PushElement(*object);
}

template<typename Tobject,
         typename Tvalue>
typename boost::enable_if< boost::is_base_of<EbmlFloat, Tobject> >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value) {
  *static_cast<EbmlFloat *>(object) = value;
  master->PushElement(*object);
}

template<typename Tobject,
         typename Tvalue>
typename boost::enable_if< boost::is_base_of<EbmlString, Tobject> >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value) {
  *static_cast<EbmlString *>(object) = value;
  master->PushElement(*object);
}

template<typename Tobject,
         typename Tvalue>
typename boost::enable_if< boost::is_base_of<EbmlUnicodeString, Tobject> >::type
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value) {
  *static_cast<EbmlUnicodeString *>(object) = value;
  master->PushElement(*object);
}

template<typename Tsub_master>
typename boost::enable_if< boost::is_base_of<EbmlMaster, Tsub_master> >::type
cons_impl(EbmlMaster *master,
          Tsub_master *sub_master) {
  master->PushElement(*sub_master);
}

template<typename Tsub_master,
         typename... Targs>
typename boost::enable_if< boost::is_base_of<EbmlMaster, Tsub_master> >::type
cons_impl(EbmlMaster *master,
          Tsub_master *sub_master,
          Targs... args) {
  master->PushElement(*sub_master);
  cons_impl(master, args...);
}

template<typename Tobject,
         typename Tvalue,
         typename... Targs>
void
cons_impl(EbmlMaster *master,
          Tobject *object,
          Tvalue &&value,
          Targs... args) {
  cons_impl(master, object, value);
  cons_impl(master, args...);
}

template<typename Tmaster,
         typename... Targs>
Tmaster *
cons(Targs... args) {
  auto master = new Tmaster;
  master->RemoveAll();

  cons_impl(master, args...);

  return master;
}

}}

#endif // MTX_TESTS_UNIT_CONSTRUCT_H
