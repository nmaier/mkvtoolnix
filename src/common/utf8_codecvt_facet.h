// Copyright (c) 2001 Ronald Garcia, Indiana University (garcia@osl.iu.edu)
// Andrew Lumsdaine, Indiana University (lums@osl.iu.edu).

// Distributed under the Boost Software License, Version 1.0.
// (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef MTX_COMMON_UTF8_FILECVT_FACET_H
#define MTX_COMMON_UTF8_FILECVT_FACET_H

#include <boost/filesystem/config.hpp>

#define BOOST_UTF8_BEGIN_NAMESPACE namespace mtx {

#define BOOST_UTF8_END_NAMESPACE }
#define BOOST_UTF8_DECL BOOST_FILESYSTEM_DECL

#include <boost/detail/utf8_codecvt_facet.hpp>

#undef BOOST_UTF8_BEGIN_NAMESPACE
#undef BOOST_UTF8_END_NAMESPACE
#undef BOOST_UTF8_DECL

#endif
