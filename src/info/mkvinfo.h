/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of global variables and functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MKVINFO_H
#define __MKVINFO_H

#include "common/os.h"

#include <string>
#include <vector>

#define NAME "MKVInfo"

void parse_args(std::vector<std::string> args, std::string &file_name);
int console_main(std::vector<std::string> args);
bool process_file(const std::string &file_name);
void setup(const std::string &locale = "");
void cleanup();

extern bool use_gui;

void ui_show_error(const std::string &error);
void ui_show_element(int level, const std::string &text, int64_t position);
void ui_show_progress(int percentage, const std::string &text);
int ui_run(int argc, char **argv);
bool ui_graphical_available();

void console_show_error(const std::string &text);
void console_show_element(int level, const std::string &text, int64_t position);

#endif // __MKVINFO_H
