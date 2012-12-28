/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   FourCC handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/bswap.h"
#include "common/endian.h"
#include "common/fourcc.h"

static uint32_t
val(uint32_t value,
    fourcc_c::byte_order_t byte_order) {
  return fourcc_c::big_endian == byte_order ? value : bswap_32(value);
}

fourcc_c::fourcc_c()
  : m_value{}
{
}

fourcc_c::fourcc_c(uint32_t value,
                   fourcc_c::byte_order_t byte_order)
  : m_value{val(value, byte_order)}
{
}

fourcc_c::fourcc_c(std::string const &value)
  : m_value{read(value.c_str(), big_endian)}
{
}

fourcc_c::fourcc_c(char const *value)
  : m_value{read(value, big_endian)}
{
}

fourcc_c::fourcc_c(memory_cptr const &mem,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(mem->get_buffer(), byte_order)}
{
}

fourcc_c::fourcc_c(unsigned char const *mem,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(mem, byte_order)}
{
}

fourcc_c::fourcc_c(uint32_t const *mem,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(mem, byte_order)}
{
}

fourcc_c::fourcc_c(mm_io_cptr const &io,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(*io, byte_order)}
{
}

fourcc_c::fourcc_c(mm_io_c &io,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(io, byte_order)}
{
}

fourcc_c::fourcc_c(mm_io_c *io,
                   fourcc_c::byte_order_t byte_order)
  : m_value{read(*io, byte_order)}
{
}

size_t
fourcc_c::write(memory_cptr const &mem,
                fourcc_c::byte_order_t byte_order) {
  return write(mem->get_buffer(), byte_order);
}

size_t
fourcc_c::write(unsigned char *mem,
                fourcc_c::byte_order_t byte_order) {
  put_uint32_be(mem, val(m_value, byte_order));
  return 4;
}

size_t
fourcc_c::write(mm_io_cptr const &io,
                fourcc_c::byte_order_t byte_order) {
  return write(*io, byte_order);
}

size_t
fourcc_c::write(mm_io_c &io,
                fourcc_c::byte_order_t byte_order) {
  return io.write_uint32_be(val(m_value, byte_order));
}

size_t
fourcc_c::write(mm_io_c *io,
                fourcc_c::byte_order_t byte_order) {
  return write(*io, byte_order);
}

uint32_t
fourcc_c::value(fourcc_c::byte_order_t byte_order)
  const {
  return val(m_value, byte_order);
}

std::string
fourcc_c::str()
  const {
  char buffer[4];
  put_uint32_be(buffer, m_value);
  for (auto idx = 0; 4 > idx; ++idx)
    buffer[idx] = 32 <= buffer[idx] ? buffer[idx] : '?';

  return std::string{buffer, 4};
}

fourcc_c::operator bool()
  const {
  return !!m_value;
}

fourcc_c &
fourcc_c::reset() {
  m_value = 0;
  return *this;
}

bool
fourcc_c::operator ==(fourcc_c const &cmp)
  const {
  return m_value == cmp.m_value;
}

bool
fourcc_c::operator !=(fourcc_c const &cmp)
  const {
  return m_value != cmp.m_value;
}

bool
fourcc_c::equiv(char const *cmp)
  const {
  return balg::to_lower_copy(str()) == balg::to_lower_copy(std::string{cmp});
}

uint32_t
fourcc_c::read(void const *mem,
               fourcc_c::byte_order_t byte_order) {
  return val(get_uint32_be(mem), byte_order);
}

uint32_t
fourcc_c::read(mm_io_c &io,
               fourcc_c::byte_order_t byte_order) {
  return val(io.read_uint32_be(), byte_order);
}
