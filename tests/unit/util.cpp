#include "common/common_pch.h"

#include <iostream>

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlDate.h>
#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlVoid.h>

#include "common/strings/editing.h"
#include "common/at_scope_exit.h"
#include "tests/unit/util.h"

namespace mtxut {

void
dump(EbmlElement *element,
     bool with_values,
     unsigned int level) {
  std::string indent_str, value_str;
  size_t i;

  for (i = 1; i <= level; ++i)
    indent_str += " ";

  if (with_values) {
    if (dynamic_cast<EbmlUInteger *>(element))
      value_str = to_string(uint64(*static_cast<EbmlUInteger *>(element)));

    else if (dynamic_cast<EbmlSInteger *>(element))
      value_str = to_string(int64(*static_cast<EbmlSInteger *>(element)));

    else if (dynamic_cast<EbmlFloat *>(element))
      value_str = to_string(double(*static_cast<EbmlFloat *>(element)));

    else if (dynamic_cast<EbmlUnicodeString *>(element))
      value_str = UTFstring(*static_cast<EbmlUnicodeString *>(element)).GetUTF8();

    else if (dynamic_cast<EbmlString *>(element))
      value_str = std::string(*static_cast<EbmlString *>(element));

    else if (dynamic_cast<EbmlDate *>(element))
      value_str = to_string(static_cast<EbmlDate *>(element)->GetEpochDate());

    else
      value_str = (boost::format("(type: %1%)") %
                   (  dynamic_cast<EbmlBinary *>(element) ? "binary"
                    : dynamic_cast<EbmlMaster *>(element) ? "master"
                    : dynamic_cast<EbmlVoid *>(element)   ? "void"
                    :                                       "unknown")).str();

    value_str = " " + value_str;
  }

  std::cout << indent_str << EBML_NAME(element) << value_str << std::endl;

  EbmlMaster *master = dynamic_cast<EbmlMaster *>(element);
  if (!master)
    return;

  for (auto el : *master)
    dump(el, with_values, level + 1);
}

//
// ----------------------------------------------------------------------
//

::testing::AssertionResult
EbmlEquals(char const *a_expr,
           char const *b_expr,
           EbmlElement &a,
           EbmlElement &b) {
  std::string error;
  if (ebml_equals_c::check(a, b, error))
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << a_expr << " and " << b_expr << " are not equal: " << error;
}

//
// ----------------------------------------------------------------------
//

ebml_equals_c::ebml_equals_c() {
}


std::string
ebml_equals_c::get_error() const {
  return m_error;
}

bool
ebml_equals_c::set_error(std::string const &error,
                         EbmlElement *cmp) {
  auto full_path = m_path;
  if (cmp)
    full_path.push_back(EBML_NAME(cmp));

  m_error = (boost::format("%1% path: %2%") % error % join(" -> ", full_path)).str();
  return false;
}

bool
ebml_equals_c::set_error(boost::format const &error,
                         EbmlElement *cmp) {
  return set_error(error.str(), cmp);
}

bool
ebml_equals_c::check(EbmlElement &a,
                     EbmlElement &b,
                     std::string &error) {
  ebml_equals_c checker;
  bool result = checker.compare_impl(a, b);
  error       = checker.get_error();
  return result;
}

bool
ebml_equals_c::compare_impl(EbmlElement &a,
                            EbmlElement &b) {
  if (&a == &b)
    return true;

  if (std::string{EBML_NAME(&a)} != std::string{EBML_NAME(&b)})
    return set_error(boost::format("types are differing: %1% vs %2%") % EBML_NAME(&a) % EBML_NAME(&b));

  EbmlUInteger *ui_a;
  EbmlSInteger *si_a;
  EbmlFloat *f_a;
  EbmlString *str_a;
  EbmlUnicodeString *ustr_a;
  EbmlMaster *m_a;
  EbmlDate *d_a;

  if ((ui_a = dynamic_cast<EbmlUInteger *>(&a))) {
    auto val_b = uint64(*dynamic_cast<EbmlUInteger *>(&b));
    return uint64(*ui_a) == val_b ? true : set_error(boost::format("UInteger values differ: %1% vs %2%") % uint64(*ui_a) % val_b, &a);

  } else if ((si_a = dynamic_cast<EbmlSInteger *>(&a))) {
    auto val_b = int64(*dynamic_cast<EbmlSInteger *>(&b));
    return int64(*si_a) == val_b ? true : set_error(boost::format("SInteger values differ: %1% vs %2%") % int64(*si_a) % val_b, &a);

  } else if ((d_a = dynamic_cast<EbmlDate *>(&a))) {
    auto val_a = d_a->GetEpochDate();
    auto val_b = dynamic_cast<EbmlDate *>(&b)->GetEpochDate();
    return val_a == val_b ? true : set_error(boost::format("Date values differ: %1% vs %2%") % val_a % val_b, &a);

  } else if ((f_a = dynamic_cast<EbmlFloat *>(&a))) {
    auto val_b = double(*dynamic_cast<EbmlFloat *>(&b));
    return double(*f_a) == val_b ? true : set_error(boost::format("Float values differ: %1% vs %2%") % double(*f_a) % val_b, &a);

  } else if ((str_a = dynamic_cast<EbmlString *>(&a))) {
    auto val_b = std::string(*dynamic_cast<EbmlString *>(&b));
    return std::string(*str_a) == val_b ? true : set_error(boost::format("String values differ: %1% vs %2%") % std::string(*str_a) % val_b, &a);

  } else if ((ustr_a = dynamic_cast<EbmlUnicodeString *>(&a))) {
    auto val_a = UTFstring(*dynamic_cast<EbmlUnicodeString *>(&a)).GetUTF8();
    auto val_b = UTFstring(*dynamic_cast<EbmlUnicodeString *>(&b)).GetUTF8();
    return val_a == val_b ? true : set_error(boost::format("UnicodeString values differ: %1% vs %2%") % val_a % val_b, &a);

  } else if ((m_a = dynamic_cast<EbmlMaster *>(&a))) {
    m_path.push_back(EBML_NAME(&a));
    at_scope_exit_c popper{[&]() { m_path.pop_back(); }};

    auto m_b = dynamic_cast<EbmlMaster *>(&b);
    for (int i = 0; i < std::min<int>(m_a->ListSize(), m_b->ListSize()); ++i)
      if (!compare_impl(*(*m_a)[i], *(*m_b)[i]))
        return false;

    if (m_a->ListSize() == m_b->ListSize())
      return true;

    std::string op;
    EbmlMaster *with_more, *with_less;
    if (m_a->ListSize() > m_b->ListSize()) {
      op        = "more";
      with_more = m_a;
      with_less = m_b;
    } else {
      op        = "less";
      with_less = m_a;
      with_more = m_b;
    }

    std::stringstream error{ (boost::format("LHS contains %1% elements than RHS (%2% vs %3%):") % op % m_a->ListSize() % m_b->ListSize()).str() };
    for (auto i = with_less->ListSize(); i < with_more->ListSize(); ++i)
      error << " " << EBML_NAME((*with_more)[i]);

    return set_error(error.str());

  } else
    return set_error(boost::format("unsupported types: %1% and %2%") % EBML_NAME(&a) % EBML_NAME(&b));
}

}
