/*
   ebml_validator - A tool for validating the EBML structure

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include <stdlib.h>

#include "common/bit_cursor.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/common_pch.h"
#include "common/mm_io.h"
#include "common/strings/parsing.h"
#include "common/translation.h"

#include "element_info.h"

static int64_t g_start = 0;
static int64_t g_end   = std::numeric_limits<long long>::max();

static int64_t g_file_size;
static mm_file_io_c *g_in;

static std::map<int64_t, bool> g_is_master;

class vint_c {
public:
  int64_t value;
  int coded_size;

  vint_c(int64_t p_value,
         int p_coded_size)
    : value(p_value)
    , coded_size(p_coded_size)
  {
  }

  bool is_unknown() {
    return
         ((1 == coded_size) && (0x000000000000007fll == value))
      || ((2 == coded_size) && (0x0000000000003fffll == value))
      || ((3 == coded_size) && (0x00000000001fffffll == value))
      || ((4 == coded_size) && (0x000000000fffffffll == value))
      || ((5 == coded_size) && (0x00000007ffffffffll == value))
      || ((6 == coded_size) && (0x000003ffffffffffll == value))
      || ((7 == coded_size) && (0x0001ffffffffffffll == value))
      || ((8 == coded_size) && (0x00ffffffffffffffll == value));
  }
};

static void
show_help() {
  mxinfo(Y("ebml_validor [options] input_file_name\n"
           "\n"
           "Options output and information control:\n"
           "\n"
           "  -s, --start <value>    Start parsing at file position value\n"
           "  -e, --end <value>      Stop parsing at file position value\n"
           "  -m, --master <value>   The EBML ID value (in hex) is a master\n"
           "  -M, --auto-masters     Use all of Matroska's master elements\n"
           "\n"
           "General options:\n"
           "\n"
           "  -h, --help             This help text\n"
           "  -V, --version          Print version information\n"));
  mxexit(0);
}

static void
show_version() {
  mxinfo("ebml_validator v" VERSION "\n");
  mxexit(0);
}

static std::string
parse_args(std::vector<std::string> &args) {
  std::string file_name;

  std::vector<std::string>::iterator arg = args.begin();
  while (arg != args.end()) {
    if ((*arg == "-h") || (*arg == "--help"))
      show_help();

    else if ((*arg == "-V") || (*arg == "--version"))
      show_version();

    else if ((*arg == "-s") || (*arg == "--start")) {
      ++arg;
      if ((args.end() == arg) || !parse_number(*arg, g_start) || (0 >= g_start))
        mxerror(Y("Missing/wrong arugment to --start\n"));

    } else if ((*arg == "-e") || (*arg == "--end")) {
      ++arg;
      if ((args.end() == arg) || !parse_number(*arg, g_end) || (0 >= g_end))
        mxerror(Y("Missing/wrong arugment to --end\n"));

    } else if ((*arg == "-m") || (*arg == "--master")) {
      ++arg;
      if (args.end() == arg)
        mxerror(Y("Missing arugment to --master\n"));

      if (arg->substr(0, 2) == "0x") {
        arg->erase(0, 2);
        g_is_master[strtoll(arg->c_str(), nullptr, 16)] = true;

      } else {
        uint32_t id = element_name_to_id(*arg);
        if (0 == id)
          mxerror(boost::format(Y("Unknown element name in '--master %1%'\n")) % *arg);
        g_is_master[id] = true;
      }

    } else if ((*arg == "-M") || (*arg == "--auto-masters")) {
      std::map<uint32_t, bool>::const_iterator i = g_master_information.begin();
      while (g_master_information.end() != i) {
        g_is_master[i->first] = true;
        ++i;
      }

    } else if (!file_name.empty())
      mxerror(Y("More than one input file given\n"));

    else
      file_name = *arg;

    ++arg;
  }

  if (file_name.empty())
    mxerror(Y("No file name given\n"));

  return file_name;
}

class id_error_c {
public:
  enum error_type_e {
    end_of_file,
    end_of_scope,
    first_byte_is_zero,
    longer_than_four_bytes,
  };

  error_type_e code;

  id_error_c(error_type_e p_code)
    : code(p_code)
  {
  }
};

class size_error_c {
public:
  enum error_type_e {
    end_of_file,
    end_of_scope,
  };

  error_type_e code;

  size_error_c(error_type_e p_code)
    : code(p_code)
  {
  }
};

static std::string
level_string(int level) {
  std::string s;
  int i;

  for (i = 0; i < level; ++i)
    s += " ";

  return s;
}

static vint_c
read_id(int64_t end_pos) {
  try {
    int64_t pos = g_in->getFilePointer();
    int mask    = 0x80;
    int id_len  = 1;

    if (pos >= end_pos)
      throw id_error_c(id_error_c::end_of_scope);

    unsigned char first_byte = g_in->read_uint8();

    while (0 != mask) {
      if (0 != (first_byte & mask))
        break;

      mask >>= 1;
      id_len++;
    }

    if (0 == mask)
      throw id_error_c(id_error_c::first_byte_is_zero);

    if (4 < id_len)
      throw id_error_c(id_error_c::longer_than_four_bytes);

    if ((pos + id_len) > end_pos)
      throw id_error_c(id_error_c::end_of_scope);

    uint32_t id = first_byte;
    int i;
    for (i = 1; i < id_len; ++i) {
      id <<= 8;
      id  |= g_in->read_uint8();
    }

    return vint_c(id, id_len);

  } catch (mtx::exception &error) {
    throw id_error_c(id_error_c::end_of_file);
  }
}

static vint_c
read_size(int64_t end_pos) {
  try {
    int64_t pos  = g_in->getFilePointer();
    int mask     = 0x80;
    int size_len = 1;

    if (pos >= end_pos)
      throw size_error_c(size_error_c::end_of_scope);

    unsigned char first_byte = g_in->read_uint8();

    while (0 != mask) {
      if (0 != (first_byte & mask))
        break;

      mask >>= 1;
      size_len++;
    }

    if ((pos + size_len) > end_pos)
      throw size_error_c(size_error_c::end_of_scope);

    int64_t size = first_byte & ~mask;
    int i;
    for (i = 1; i < size_len; ++i) {
      size <<= 8;
      size  |= g_in->read_uint8();
    }

    return vint_c(size, size_len);

  } catch (mtx::exception &error) {
    throw size_error_c(size_error_c::end_of_file);
  }
}

static void
parse_content(int level,
              int64_t end_pos) {
  while (static_cast<int64_t>(g_in->getFilePointer()) < end_pos) {
    int64_t element_start_pos = g_in->getFilePointer();

    try {
      vint_c  id          = read_id(end_pos);
      vint_c size         = read_size(end_pos);

      std::string element_name = g_element_names[id.value];
      if (element_name.empty())
        element_name      = Y("unknown");

      mxinfo(boost::format(Y("%1%pos %2% id 0x%|3$x| size %4% header size %5% (%6%)\n"))
             % level_string(level) % element_start_pos % id.value % size.value % (id.coded_size + size.coded_size) % element_name);

      if (size.is_unknown())
        mxinfo(boost::format(Y("%1%  Warning: size is coded as 'unknown' (all bits are set)\n")) % level_string(level));

      int64_t content_end_pos = g_in->getFilePointer() + size.value;

      if (content_end_pos > end_pos) {
        mxinfo(boost::format(Y("%1%  Error: Element ends after scope\n")) % level_string(level));
        if (!g_in->setFilePointer2(end_pos))
          mxerror(boost::format(Y("Error: Seek to %1%\n")) % end_pos);
        return;
      }

      if (g_is_master[id.value])
        parse_content(level + 1, content_end_pos);

      if (!g_in->setFilePointer2(content_end_pos))
        mxerror(boost::format(Y("Error: Seek to %1%\n")) % content_end_pos);

    } catch (id_error_c &error) {
      std::string message
        = id_error_c::end_of_file            == error.code ? Y("End of file")
        : id_error_c::end_of_scope           == error.code ? Y("End of scope")
        : id_error_c::first_byte_is_zero     == error.code ? Y("First byte is zero")
        : id_error_c::longer_than_four_bytes == error.code ? Y("ID is longer than four bytes")
        :                                                    Y("reason is unknown");

      mxinfo(boost::format(Y("%1%Error at %2%: error reading the element ID (%3%)\n")) % level_string(level) % element_start_pos % message);

      if (!g_in->setFilePointer2(end_pos))
        mxerror(boost::format(Y("Error: Seek to %1%\n")) % end_pos);
      return;

    } catch (size_error_c &error) {
      std::string message
        = size_error_c::end_of_file  == error.code ? Y("End of file")
        : size_error_c::end_of_scope == error.code ? Y("End of scope")
        :                                            Y("reason is unknown");

      mxinfo(boost::format(Y("%1%Error at %2%: error reading the element size (%3%)\n")) % level_string(level) % element_start_pos % message);

      if (!g_in->setFilePointer2(end_pos))
        mxerror(boost::format(Y("Error: Seek to %1%\n")) % end_pos);
      return;

    } catch (...) {
      mxerror(Y("Unknown error occured\n"));
    }
  }
}

static void
parse_file(const std::string &file_name) {
  mm_file_io_c in(file_name);

  g_in        = &in;
  g_file_size = in.get_size();

  g_start     = std::min(g_file_size, g_start);
  g_end       = std::min(g_file_size, g_end);

  if (!in.setFilePointer2(g_start))
    mxerror(boost::format(Y("Error: Seek to %1%\n")) % g_start);

  parse_content(0, g_end);
}

int
main(int argc,
     char **argv) {
  mtx_common_init("ebml_validator", argv[0]);

  init_element_names();
  init_master_information();

  std::vector<std::string> args = command_line_utf8(argc, argv);
  std::string file_name         = parse_args(args);

  try {
    parse_file(file_name);
  } catch (...) {
    mxerror(Y("File not found\n"));
  }

  return 0;
}
