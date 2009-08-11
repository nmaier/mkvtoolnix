/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_SETUP_H
#define __PROPEDIT_SETUP_H

#include "common/os.h"

#include <string>
#include <vector>

#include "propedit/options.h"

void setup();
options_c parse_args(std::vector<std::string> args);

#endif // __PROPEDIT_SETUP_H
