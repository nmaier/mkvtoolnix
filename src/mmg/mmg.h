/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   main stuff - declarations
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_H
#define __MMG_H

#include <map>
#include <vector>

#include "os.h"

#include "wx/string.h"
#include "wx/app.h"

#include "ebml/EbmlUnicodeString.h"

#include "iso639.h"
#include "smart_pointers.h"
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

struct mmg_track_t {
  char type;
  int64_t id;
  int source;
  wxString ctype;
  bool enabled, display_dimensions_selected;

  bool default_track, aac_is_sbr;
  bool track_name_was_present;
  wxString language, track_name, cues, delay, stretch, sub_charset;
  wxString tags, fourcc, aspect_ratio, compression, timecodes;
  wxString dwidth, dheight;

  bool appending;

  mmg_track_t():
    type(0), id(0), source(0),
    enabled(false), display_dimensions_selected(false),
    default_track(false), aac_is_sbr(false), track_name_was_present(false),
    appending(false) {};
};
typedef counted_ptr<mmg_track_t> mmg_track_ptr;

struct mmg_file_t {
  wxString file_name, title;
  bool title_was_present;
  int container;
  vector<mmg_track_ptr> tracks;
  bool no_chapters, no_attachments, no_tags;
  bool appending;

  mmg_file_t():
    title_was_present(false), container(0),
    no_chapters(false), no_attachments(false), no_tags(false),
    appending(false) {};
};
typedef counted_ptr<mmg_file_t> mmg_file_ptr;

struct mmg_attachment_t {
  wxString file_name, description, mime_type;
  int style;

  mmg_attachment_t():
    style(0) {};
};
typedef counted_ptr<mmg_attachment_t> mmg_attachment_ptr;

extern wxString last_open_dir;
extern wxString mkvmerge_path;
extern vector<wxString> last_settings;
extern vector<wxString> last_chapters;
extern vector<mmg_file_t> files;
extern vector<mmg_track_t *> tracks;
extern vector<mmg_attachment_t> attachments;
extern wxArrayString sorted_charsets;
extern wxArrayString sorted_iso_codes;
extern bool title_was_present;
extern map<wxString, wxString> capabilities;

wxString &break_line(wxString &line, int break_after = 80);
wxString extract_language_code(wxString source);
wxString shell_escape(wxString source);
vector<wxString> split(const wxString &src, const wxString &pattern,
                       int max_num = -1);
wxString join(const wxString &pattern, vector<wxString> &strings);
wxString &strip(wxString &s, bool newlines = false);
vector<wxString> & strip(vector<wxString> &v, bool newlines = false);
string to_utf8(const wxString &src);
wxString from_utf8(const wxString &src);
wxString UTFstring_to_wxString(const UTFstring &u);
wxString unescape(const wxString &src);
wxString format_date_time(time_t date_time);

wxString create_track_order(bool all);

void wxdie(const wxString &errmsg);

#if defined(SYS_WINDOWS)
#define TIP(s) format_tooltip(wxT(s))
wxString format_tooltip(const wxString &s);
#else
#define TIP(s) wxT(s)
#endif

class mmg_app: public wxApp {
public:
  virtual bool OnInit();
  virtual int OnExit();
};

extern mmg_app *app;

#endif // __MMG_H
