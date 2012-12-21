/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for AMF (Adobe Macromedia Flash) data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/amf.h"
#include "common/math.h"
#include "common/mm_io_x.h"

namespace mtx { namespace amf {

script_parser_c::script_parser_c(memory_cptr const &mem)
  : m_data_mem{mem}
  , m_data{*mem.get()}
  , m_in_ecma_array{}
  , m_in_meta_data{}
  , m_level{}
  , m_debug{debugging_requested("amf|amf_script_parser")}
{
}

std::string
script_parser_c::level_spacer()
  const {
  return std::string(m_level, ' ');
}

std::unordered_map<std::string, value_type_t> const &
script_parser_c::get_meta_data()
  const {
  return m_meta_data;
}

std::string
script_parser_c::read_string(data_type_e type) {
  unsigned int size = TYPE_STRING == type ? m_data.read_uint16_be() : m_data.read_uint32_be();
  std::string data;
  if (!size)
    m_data.skip(1);
  else if (m_data.read(data, size) != size)
    throw mtx::mm_io::end_of_file_x{};

  while ((0 < size) && !data[size - 1])
    --size;
  data.erase(size);

  return data;
}

void
script_parser_c::read_properties(std::unordered_map<std::string, value_type_t> &properties) {
  while (1) {
    auto key   = read_string(TYPE_STRING);
    auto value = read_value();

    mxdebug_if(m_debug, boost::format("%1%Property key: %2%; value read: %3%; value: %4%\n") % level_spacer() % key % value.second % boost::apply_visitor(value_to_string_c(), value.first));

    if (key.empty() || !value.second)
      break;

    properties[key] = value.first;
  }
}

std::pair<value_type_t, bool>
script_parser_c::read_value() {
  ++m_level;

  value_type_t value;
  auto data_type = static_cast<data_type_e>(m_data.read_uint8());

  mxdebug_if(m_debug, boost::format("%1%Data type @ %2%: %3%\n") % level_spacer() % (m_data.getFilePointer() - 1) % static_cast<unsigned int>(data_type));

  bool data_read = true;

  if ((TYPE_MOVIECLIP == data_type) || (TYPE_NULL == data_type) || (TYPE_UNDEFINED == data_type) || (TYPE_OBJECT_END == data_type))
    ;

  else if (TYPE_REFERENCE == data_type)
    m_data.skip(2);

  else if (TYPE_DATE == data_type)
    m_data.skip(10);

  else if (TYPE_NUMBER == data_type)
    value = int_to_double(static_cast<int64_t>(m_data.read_uint64_be()));

  else if (TYPE_BOOL == data_type)
    value = m_data.read_uint8() != 0;

  else if ((TYPE_STRING == data_type) || (TYPE_LONG_STRING == data_type)) {
    value = read_string(data_type);
    m_in_meta_data = boost::get<std::string>(value) == "onMetaData";

  } else if (TYPE_OBJECT == data_type) {
    std::unordered_map<std::string, value_type_t> dummy;
    read_properties(dummy);

  } else if (TYPE_ECMAARRAY == data_type) {
    m_data.skip(4);          // approximate array size
    if (m_in_meta_data)
      read_properties(m_meta_data);
    else {
      std::unordered_map<std::string, value_type_t> dummy;
      read_properties(dummy);
    }

    m_in_meta_data = false;

  } else if (TYPE_ARRAY == data_type) {
    auto num_entries = m_data.read_uint32_be();
    for (auto idx = 0u; idx < num_entries; ++idx)
      read_value();

  } else {
    mxdebug_if(m_debug, boost::format("%1%Unknown script data type: %2% position: %3%\n") % level_spacer() % static_cast<unsigned int>(data_type) % (m_data.getFilePointer() - 1));
    data_read = false;
  }

  --m_level;

  return std::make_pair(value, data_read);
}

bool
script_parser_c::parse() {
  try {
    while (m_data.getFilePointer() < static_cast<uint64_t>(m_data.get_size()))
      if (!read_value().second)
        return false;
  } catch (mtx::mm_io::exception &) {
  }

  return true;
}

}}
