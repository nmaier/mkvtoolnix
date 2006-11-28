/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   definition of global variables and functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/


#ifndef __MKVINFO_H
#define __MKVINFO_H

#include "os.h"

#include <string>
#include <vector>

#define NAME "MKVInfo"

using namespace std;

void parse_args(vector<string> args, string &file_name);
int console_main(vector<string> args);
bool process_file(const string &file_name);
void setup();
void cleanup();

extern bool use_gui;

void ui_show_error(const char *text);
void ui_show_element(int level, const char *text, int64_t position);
void ui_show_progress(int percentage, const char *text);
int ui_run(int argc, char **argv);
bool ui_graphical_available();

void console_show_error(const char *text);
void console_show_element(int level, const char *text, int64_t position);

#endif // __MKVINFO_H
