/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mmg.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief main stuff - declarations
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MMG_H
#define __MMG_H

#include <map>
#include <vector>

#include "os.h"

#include "wx/string.h"
#include "wx/app.h"

#include "ebml/EbmlUnicodeString.h"

#include "wxcommon.h"

#ifdef SYS_WINDOWS
#define ALLFILES "All Files (*.*)|*.*"
#define PSEP '\\'
#define YOFF (-4)
#define YOFF2 0
#else
#define ALLFILES "All Files (*)|*"
#define PSEP '/'
#define YOFF (-2)
#define YOFF2 (-3)
#endif

using namespace std;
using namespace libebml;

typedef struct {
  char type;
  int64_t id;
  wxString *ctype;
  bool enabled, display_dimensions_selected;

  bool default_track, aac_is_sbr;
  bool track_name_was_present;
  wxString *language, *track_name, *cues, *delay, *stretch, *sub_charset;
  wxString *tags, *fourcc, *aspect_ratio, *compression, *timecodes;
  wxString *dwidth, *dheight;
} mmg_track_t;

typedef struct {
  wxString *file_name, *title;
  bool title_was_present;
  int container;
  vector<mmg_track_t> *tracks;
  bool no_chapters, no_attachments, no_tags;
} mmg_file_t;

typedef struct {
  wxString *file_name, *description, *mime_type;
  int style;
} mmg_attachment_t;

struct lt_wxString {
  bool operator()(const wxString s1, const wxString s2) const {
    return s1.Cmp(s2) < 0;
  }
};
extern wxString last_open_dir;
extern wxString mkvmerge_path;
extern vector<wxString> last_settings;
extern vector<wxString> last_chapters;
extern vector<mmg_file_t> files;
extern vector<mmg_attachment_t> attachments;
extern wxArrayString sorted_charsets;
extern wxArrayString sorted_iso_codes;
extern bool title_was_present;
extern map<wxString, wxString, lt_wxString> capabilities;

wxString &break_line(wxString &line, int break_after = 80);
wxString extract_language_code(wxString source);
bool is_popular_language(const char *lang);
bool is_popular_language_code(const char *code);
wxString shell_escape(wxString source);
vector<wxString> split(const wxString &src, const wxString &pattern,
                       int max_num = -1);
wxString join(const wxString &pattern, vector<wxString> &strings);
wxString &strip(wxString &s, bool newlines = false);
vector<wxString> & strip(vector<wxString> &v, bool newlines = false);
string to_utf8(const wxString &src);
wxString UTFstring_to_wxString(const UTFstring &u);
wxString unescape(const wxString &src);
wxString format_date_time(time_t date_time);

void set_on_top(bool on_top);
void restore_on_top();

class mmg_app: public wxApp {
public:
  virtual bool OnInit();
  virtual int OnExit();
};

extern mmg_app *app;

#endif // __MMG_H
