/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of global variables and functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MKVINFO_H
#define MTX_MKVINFO_H

#include "common/common_pch.h"

#include "info/options.h"

#define NAME "MKVInfo"

extern options_c g_options;

int console_main();
bool process_file(const std::string &file_name);
void setup(const std::string &locale = "");
void cleanup();

std::string create_element_text(const std::string &text, int64_t position, int64_t size);
void ui_show_error(const std::string &error);
void ui_show_element(int level, const std::string &text, int64_t position, int64_t size);
void ui_show_progress(int percentage, const std::string &text);
int ui_run(int argc, char **argv);
bool ui_graphical_available();

void console_show_error(const std::string &text);
void console_show_element(int level, const std::string &text, int64_t position, int64_t size);

#endif // MTX_MKVINFO_H
