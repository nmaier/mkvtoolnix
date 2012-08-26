/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for helper functions for unit tests

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TESTS_UNIT_UTIL_H
#define MTX_TESTS_UNIT_UTIL_H

#include "common/common_pch.h"

#include <ebml/EbmlElement.h>

#include "gtest/gtest.h"
#include "common/strings/formatting.h"

#define ASSERT_EBML_EQ(a, b) ASSERT_PRED_FORMAT2(EbmlEquals, (a), (b))
#define EXPECT_EBML_EQ(a, b) EXPECT_PRED_FORMAT2(EbmlEquals, (a), (b))

namespace mtxut {

using namespace libebml;

void dump(EbmlElement *element, bool with_values = false, unsigned int level = 0);

::testing::AssertionResult EbmlEquals(char const *a_expr, char const *b_expr, EbmlElement &a, EbmlElement &b);

class ebml_equals_c {
private:
  std::vector<std::string> m_path;
  std::string m_error;

public:
  ebml_equals_c();

  bool compare_impl(EbmlElement &a, EbmlElement &b);
  std::string get_error() const;
  bool set_error(std::string const &error, EbmlElement *cmp = nullptr);
  bool set_error(boost::format const &error, EbmlElement *cmp = nullptr);

public:
  static bool check(EbmlElement &a, EbmlElement &b, std::string &error);
};

}

inline bool
operator ==(memory_c const &a,
            std::string const &b) {
  return (a.get_size() == b.length()) && !memcmp(a.get_buffer(), b.c_str(), b.length());
}

#endif // MTX_TESTS_UNIT_UTIL_H
