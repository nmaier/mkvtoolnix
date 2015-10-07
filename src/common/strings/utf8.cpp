/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.

   These functions should normally reside in locale.cpp. But the
   'utf8' namespace declared by utf8.h clases with the 'utf8' symbol
   in the 'libebml' namespace and most of the common headers contain
   'using namespace libebml'.
*/

#include "common/common_pch.h"

#include <string>
#include <utf8.h>

#include "common/strings/utf8.h"

std::wstring
to_wide(const std::string &source) {
  std::wstring destination;

  ::utf8::utf8to32(source.begin(), source.end(), back_inserter(destination));

  return destination;
}

std::string
to_utf8(const std::wstring &source) {
  std::string destination;

  ::utf8::utf32to8(source.begin(), source.end(), back_inserter(destination));

  return destination;
}

size_t
get_width_in_em(const std::wstring &s) {
  size_t width = 0;
  for (std::wstring::const_iterator c = s.begin(); c != s.end(); c++)
    width += get_width_in_em(*c);

  return width;
}

// See http://unicode.org/reports/tr11/
size_t
get_width_in_em(wchar_t c) {
  return (    (0x0000a1 == c)
          ||  (0x0000a4 == c)
          || ((0x0000a7 <= c) && (0x0000a8 >= c))
          ||  (0x0000aa == c)
          || ((0x0000ad <= c) && (0x0000ae >= c))
          || ((0x0000b0 <= c) && (0x0000b4 >= c))
          || ((0x0000b6 <= c) && (0x0000ba >= c))
          || ((0x0000bc <= c) && (0x0000bf >= c))
          ||  (0x0000c6 == c)
          ||  (0x0000d0 == c)
          || ((0x0000d7 <= c) && (0x0000d8 >= c))
          || ((0x0000de <= c) && (0x0000e1 >= c))
          ||  (0x0000e6 == c)
          || ((0x0000e8 <= c) && (0x0000ea >= c))
          || ((0x0000ec <= c) && (0x0000ed >= c))
          ||  (0x0000f0 == c)
          || ((0x0000f2 <= c) && (0x0000f3 >= c))
          || ((0x0000f7 <= c) && (0x0000fa >= c))
          ||  (0x0000fe == c)
          ||  (0x000101 == c)
          ||  (0x000111 == c)
          ||  (0x000113 == c)
          ||  (0x00011b == c)
          || ((0x000126 <= c) && (0x000127 >= c))
          ||  (0x00012b == c)
          || ((0x000131 <= c) && (0x000133 >= c))
          ||  (0x000138 == c)
          || ((0x00013f <= c) && (0x000142 >= c))
          ||  (0x000144 == c)
          || ((0x000148 <= c) && (0x00014b >= c))
          ||  (0x00014d == c)
          || ((0x000152 <= c) && (0x000153 >= c))
          || ((0x000166 <= c) && (0x000167 >= c))
          ||  (0x00016b == c)
          ||  (0x0001ce == c)
          ||  (0x0001d0 == c)
          ||  (0x0001d2 == c)
          ||  (0x0001d4 == c)
          ||  (0x0001d6 == c)
          ||  (0x0001d8 == c)
          ||  (0x0001da == c)
          ||  (0x0001dc == c)
          ||  (0x000251 == c)
          ||  (0x000261 == c)
          ||  (0x0002c4 == c)
          ||  (0x0002c7 == c)
          || ((0x0002c9 <= c) && (0x0002cb >= c))
          ||  (0x0002cd == c)
          ||  (0x0002d0 == c)
          || ((0x0002d8 <= c) && (0x0002db >= c))
          ||  (0x0002dd == c)
          ||  (0x0002df == c)
          || ((0x000300 <= c) && (0x00036f >= c))
          || ((0x000391 <= c) && (0x0003a1 >= c))
          || ((0x0003a3 <= c) && (0x0003a9 >= c))
          || ((0x0003b1 <= c) && (0x0003c1 >= c))
          || ((0x0003c3 <= c) && (0x0003c9 >= c))
          ||  (0x000401 == c)
          || ((0x000410 <= c) && (0x00044f >= c))
          ||  (0x000451 == c)
          || ((0x001100 <= c) && (0x001159 >= c))
          ||  (0x00115f == c)
          ||  (0x002010 == c)
          || ((0x002013 <= c) && (0x002016 >= c))
          || ((0x002018 <= c) && (0x002019 >= c))
          || ((0x00201c <= c) && (0x00201d >= c))
          || ((0x002020 <= c) && (0x002022 >= c))
          || ((0x002024 <= c) && (0x002027 >= c))
          ||  (0x002030 == c)
          || ((0x002032 <= c) && (0x002033 >= c))
          ||  (0x002035 == c)
          ||  (0x00203b == c)
          ||  (0x00203e == c)
          ||  (0x002074 == c)
          ||  (0x00207f == c)
          || ((0x002081 <= c) && (0x002084 >= c))
          ||  (0x0020ac == c)
          ||  (0x002103 == c)
          ||  (0x002105 == c)
          ||  (0x002109 == c)
          ||  (0x002113 == c)
          ||  (0x002116 == c)
          || ((0x002121 <= c) && (0x002122 >= c))
          ||  (0x002126 == c)
          ||  (0x00212b == c)
          || ((0x002153 <= c) && (0x002154 >= c))
          || ((0x00215b <= c) && (0x00215e >= c))
          || ((0x002160 <= c) && (0x00216b >= c))
          || ((0x002170 <= c) && (0x002179 >= c))
          || ((0x002190 <= c) && (0x002199 >= c))
          || ((0x0021b8 <= c) && (0x0021b9 >= c))
          ||  (0x0021d2 == c)
          ||  (0x0021d4 == c)
          ||  (0x0021e7 == c)
          ||  (0x002200 == c)
          || ((0x002202 <= c) && (0x002203 >= c))
          || ((0x002207 <= c) && (0x002208 >= c))
          ||  (0x00220b == c)
          ||  (0x00220f == c)
          ||  (0x002211 == c)
          ||  (0x002215 == c)
          ||  (0x00221a == c)
          || ((0x00221d <= c) && (0x002220 >= c))
          ||  (0x002223 == c)
          ||  (0x002225 == c)
          || ((0x002227 <= c) && (0x00222c >= c))
          ||  (0x00222e == c)
          || ((0x002234 <= c) && (0x002237 >= c))
          || ((0x00223c <= c) && (0x00223d >= c))
          ||  (0x002248 == c)
          ||  (0x00224c == c)
          ||  (0x002252 == c)
          || ((0x002260 <= c) && (0x002261 >= c))
          || ((0x002264 <= c) && (0x002267 >= c))
          || ((0x00226a <= c) && (0x00226b >= c))
          || ((0x00226e <= c) && (0x00226f >= c))
          || ((0x002282 <= c) && (0x002283 >= c))
          || ((0x002286 <= c) && (0x002287 >= c))
          ||  (0x002295 == c)
          ||  (0x002299 == c)
          ||  (0x0022a5 == c)
          ||  (0x0022bf == c)
          ||  (0x002312 == c)
          || ((0x002329 <= c) && (0x00232a >= c))
          || ((0x002460 <= c) && (0x0024e9 >= c))
          || ((0x0024eb <= c) && (0x00254b >= c))
          || ((0x002550 <= c) && (0x002573 >= c))
          || ((0x002580 <= c) && (0x00258f >= c))
          || ((0x002592 <= c) && (0x002595 >= c))
          || ((0x0025a0 <= c) && (0x0025a1 >= c))
          || ((0x0025a3 <= c) && (0x0025a9 >= c))
          || ((0x0025b2 <= c) && (0x0025b3 >= c))
          || ((0x0025b6 <= c) && (0x0025b7 >= c))
          || ((0x0025bc <= c) && (0x0025bd >= c))
          || ((0x0025c0 <= c) && (0x0025c1 >= c))
          || ((0x0025c6 <= c) && (0x0025c8 >= c))
          ||  (0x0025cb == c)
          || ((0x0025ce <= c) && (0x0025d1 >= c))
          || ((0x0025e2 <= c) && (0x0025e5 >= c))
          ||  (0x0025ef == c)
          || ((0x002605 <= c) && (0x002606 >= c))
          ||  (0x002609 == c)
          || ((0x00260e <= c) && (0x00260f >= c))
          || ((0x002614 <= c) && (0x002615 >= c))
          ||  (0x00261c == c)
          ||  (0x00261e == c)
          ||  (0x002640 == c)
          ||  (0x002642 == c)
          || ((0x002660 <= c) && (0x002661 >= c))
          || ((0x002663 <= c) && (0x002665 >= c))
          || ((0x002667 <= c) && (0x00266a >= c))
          || ((0x00266c <= c) && (0x00266d >= c))
          ||  (0x00266f == c)
          ||  (0x00273d == c)
          || ((0x002776 <= c) && (0x00277f >= c))
          || ((0x002e80 <= c) && (0x002e99 >= c))
          || ((0x002e9b <= c) && (0x002ef3 >= c))
          || ((0x002f00 <= c) && (0x002fd5 >= c))
          || ((0x002ff0 <= c) && (0x002ffb >= c))
          || ((0x003000 <= c) && (0x00303e >= c))
          || ((0x003041 <= c) && (0x003096 >= c))
          || ((0x003099 <= c) && (0x0030ff >= c))
          || ((0x003105 <= c) && (0x00312d >= c))
          || ((0x003131 <= c) && (0x00318e >= c))
          || ((0x003190 <= c) && (0x0031b7 >= c))
          || ((0x0031c0 <= c) && (0x0031e3 >= c))
          || ((0x0031f0 <= c) && (0x00321e >= c))
          || ((0x003220 <= c) && (0x003243 >= c))
          || ((0x003250 <= c) && (0x0032fe >= c))
          || ((0x003300 <= c) && (0x003400 >= c))
          || ((0x004e00 <= c) && (0x009fc3 >= c))
          || ((0x00a000 <= c) && (0x00a48c >= c))
          || ((0x00a490 <= c) && (0x00a4c6 >= c))
          || ((0x00ac00 <= c) && (0x00d7a3 >= c))
          || ((0x00e000 <= c) && (0x00fa2d >= c))
          || ((0x00fa30 <= c) && (0x00fa6a >= c))
          || ((0x00fa70 <= c) && (0x00fad9 >= c))
          || ((0x00fe00 <= c) && (0x00fe19 >= c))
          || ((0x00fe30 <= c) && (0x00fe52 >= c))
          || ((0x00fe54 <= c) && (0x00fe66 >= c))
          || ((0x00fe68 <= c) && (0x00fe6b >= c))
          || ((0x00ff01 <= c) && (0x00ff60 >= c))
          || ((0x00ffe0 <= c) && (0x00ffe6 >= c))
          ||  (0x00fffd == c)
#if !defined(SYS_WINDOWS)
          || ((0x020000 <= c) && (0x02a6d7 >= c))
          || ((0x02f800 <= c) && (0x02fa1e >= c))
          || ((0x030000 <= c) && (0x03fffd >= c))
          || ((0x0e0100 <= c) && (0x0e01ef >= c))
          || ((0x0f0000 <= c) && (0x0ffffd >= c))
          || ((0x100000 <= c) && (0x10fffd >= c))
#endif
              )
    ? 2 : 1;
}
