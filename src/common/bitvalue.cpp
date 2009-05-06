/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   The "bitvalue_c" class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/bitvalue.h"
#include "common/error.h"
#include "common/memory.h"
#include "common/random.h"
#include "common/string_editing.h"

bitvalue_c::bitvalue_c(int bitsize)
  : m_value(NULL)
  , m_bitsize(bitsize)
{
  assert(bitsize > 0);
  assert((bitsize % 8) == 0);

  m_value = (unsigned char *)safemalloc(bitsize / 8);
  memset(m_value, 0, m_bitsize / 8);
}

bitvalue_c::bitvalue_c(const bitvalue_c &src)
  : m_value(NULL)
  , m_bitsize(0)
{
  *this = src;
}

#define ishexdigit(c) (isdigit(c) || (((c) >= 'a') && ((c) <= 'f')))
#define hextodec(c)   (isdigit(c) ? ((c) - '0') : ((c) - 'a' + 10))

bitvalue_c::bitvalue_c(std::string s,
                       int allowed_bitlength) {
  if ((allowed_bitlength != -1) && ((allowed_bitlength % 8) != 0))
    throw error_c("wrong usage: invalid allowed_bitlength");

  int len                 = s.size();
  bool previous_was_space = true;
  s                       = downcase(s);
  std::string s2;

  int i;
  for (i = 0; i < len; i++) {
    // Space or tab?
    if (isblanktab(s[i])) {
      previous_was_space = true;
      continue;
    }
    previous_was_space = false;

    // Skip hyphens and curly braces. Makes copy & paste a bit easier.
    if ((s[i] == '-') || (s[i] == '{') || (s[i] == '}'))
      continue;

    // Space or tab followed by "0x"? Then skip it.
    if (s.substr(i, 2) == "0x") {
      i++;
      continue;
    }

    // Invalid character?
    if (!ishexdigit(s[i]))
      throw error_c(boost::format(Y("Not a hex digit at position %1%")) % i);

    // Input too long?
    if ((allowed_bitlength > 0) && ((s2.length() * 4) >= allowed_bitlength))
      throw error_c(boost::format(Y("Input too long: %1% > %2%")) % (s2.length() * 4) % allowed_bitlength);

    // Store the value.
    s2 += s[i];
  }

  // Is half a byte or more missing?
  len = s2.length();
  if (((len % 2) != 0)
      ||
      ((allowed_bitlength != -1) && ((len * 4) < allowed_bitlength)))
    throw error_c(Y("Missing one hex digit"));

  m_value   = (unsigned char *)safemalloc(len / 2);
  m_bitsize = len * 4;

  for (i = 0; i < len; i += 2)
    m_value[i / 2] = hextodec(s2[i]) << 4 | hextodec(s2[i + 1]);
}

bitvalue_c::bitvalue_c(const EbmlBinary &elt)
  : m_value((unsigned char *)safememdup(elt.GetBuffer(), elt.GetSize()))
  , m_bitsize(elt.GetSize() << 3)
{
}

bitvalue_c &
bitvalue_c::operator =(const bitvalue_c &src) {
  safefree(m_value);
  m_bitsize = src.m_bitsize;
  m_value   = (unsigned char *)safememdup(src.m_value, m_bitsize / 8);

  return *this;
}

bitvalue_c::~bitvalue_c() {
  safefree(m_value);
}

bool
bitvalue_c::operator ==(const bitvalue_c &cmp)
  const {
  if (cmp.m_bitsize != m_bitsize)
    return false;
  return memcmp(m_value, cmp.m_value, m_bitsize / 8) == 0;
}

unsigned char
bitvalue_c::operator [](int index)
  const {
  assert((index >= 0) && (index < (m_bitsize / 8)));
  return m_value[index];
}

void
bitvalue_c::generate_random() {
  random_c::generate_bytes(m_value, m_bitsize / 8);
}

