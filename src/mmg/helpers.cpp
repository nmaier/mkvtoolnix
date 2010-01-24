/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/file.h>

#include "mmg/mmg.h"
#include "common/common.h"
#include "common/strings/formatting.h"
#include "common/wx.h"

wxString
UTFstring_to_wxString(const UTFstring &u) {
  return wxString(u.c_str());
}

wxString &
break_line(wxString &line,
           int break_after) {
  uint32_t i, chars;
  wxString broken;

  for (i = 0, chars = 0; i < line.length(); i++) {
    if (chars >= break_after) {
      if ((line[i] == wxT(' ')) || (line[i] == wxT('\t'))) {
        broken += wxT("\n");
        chars = 0;
      } else if (line[i] == wxT('(')) {
        broken += wxT("\n(");
        chars = 0;
      } else {
        broken += line[i];
        chars++;
      }
    } else if ((chars != 0) || (broken[i] != wxT(' '))) {
      broken += line[i];
      chars++;
    }
  }

  line = broken;
  return line;
}

wxString
extract_language_code(wxString source) {
  wxString copy;
  int pos;

  if (source.Find(wxT("---")) == 0)
    return wxT("---");

  copy = source;
  if ((pos = copy.Find(wxT(" ("))) >= 0)
    copy.Remove(pos);

  return copy;
}

wxString
shell_escape(wxString source,
             bool cmd_exe_mode) {

  int i;
  wxString escaped;

#if !defined(SYS_WINDOWS)
  cmd_exe_mode = false;
#endif

  for (i = 0; i < source.Length(); i++) {
#if defined(SYS_WINDOWS)
    if (cmd_exe_mode && (source[i] == wxT('\\')) && ((i + 1) < source.Length()) && (source[i + 1] == wxT('"'))) {
      escaped += wxT("\\\\\\\"");
      ++i;
      continue;
    }
#endif

    if (!cmd_exe_mode && (source[i] == wxT('\\')))
      escaped += wxT("\\\\");

    else if (source[i] == wxT('"'))
      escaped += wxT("\\\"");

    else if ((source[i] == wxT('\n')) || (source[i] == wxT('\r')))
      escaped += wxT(" ");

    else
      escaped += source[i];
  }

  return escaped;
}

wxString
no_cr(wxString source) {
  uint32_t i;
  wxString escaped;

  for (i = 0; i < source.Length(); i++) {
    if (source[i] == wxT('\n'))
      escaped += wxT(" ");
    else
      escaped += source[i];
  }

  return escaped;
}

std::vector<wxString>
split(const wxString &src,
      const wxString &pattern,
      int max_num) {
  int num, pos;
  wxString copy;
  std::vector<wxString> v;

  copy = src;
  pos = copy.Find(pattern);
  num = 1;
  while ((pos >= 0) && ((max_num == -1) || (num < max_num))) {
    v.push_back(copy.Left(pos));
    copy.Remove(0, pos + pattern.length());
    pos = copy.Find(pattern);
    num++;
  }
  v.push_back(copy);

  return v;
}

wxString
join(const wxString &pattern,
     std::vector<wxString> &strings) {
  wxString dst;
  uint32_t i;

  if (strings.size() == 0)
    return wxEmptyString;
  dst = strings[0];
  for (i = 1; i < strings.size(); i++) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

wxString
join(const wxString &pattern,
     wxArrayString &strings) {
  wxString dst;
  uint32_t i;

  if (strings.IsEmpty())
    return wxEmptyString;
  dst = strings[0];
  for (i = 1; strings.Count() > i; ++i) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

wxString &
strip(wxString &s,
      bool newlines) {
  int i, len;
  const wxChar *c;

  c = s.c_str();
  i = 0;
  if (newlines)
    while ((c[i] != 0) && (isblanktab(c[i]) || iscr(c[i])))
      i++;
  else
    while ((c[i] != 0) && isblanktab(c[i]))
      i++;

  if (i > 0)
    s.Remove(0, i);

  c = s.c_str();
  len = s.length();
  i = 0;

  if (newlines)
    while ((i < len) && (isblanktab(c[len - i - 1]) || iscr(c[len - i - 1])))
      i++;
  else
    while ((i < len) && isblanktab(c[len - i - 1]))
      i++;

  if (i > 0)
    s.Remove(len - i, i);

  return s;
}

std::vector<wxString> &
strip(std::vector<wxString> &v,
      bool newlines) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);

  return v;
}

wxString
unescape(const wxString &src) {
  wxString dst;
  int current_char, next_char;

  if (src.length() <= 1)
    return src;
  next_char = 1;
  current_char = 0;
  while (current_char < src.length()) {
    if (src[current_char] == wxT('\\')) {
      if (next_char == src.length()) // This is an error...
        dst += wxT('\\');
      else {
        if (src[next_char] == wxT('2'))
          dst += wxT('"');
        else if (src[next_char] == wxT('s'))
          dst += wxT(' ');
        else if (src[next_char] == wxT('c'))
          dst += wxT(':');
        else
          dst += src[next_char];
        current_char++;
      }
    } else
      dst += src[current_char];
    current_char++;
    next_char = current_char + 1;
  }

  return dst;
}

wxString
format_date_time(time_t date_time) {
  return wxDateTime(date_time).Format(wxT("%Y-%m-%d %H:%M:%S"));
}

#if defined(SYS_WINDOWS)
wxString
format_tooltip(const wxString &s) {
  return format_paragraph(s.c_str(), 0, L"", L"", 80);
}
#endif

wxString
get_temp_dir() {
  wxString temp_dir;

  wxGetEnv(wxT("TMP"), &temp_dir);
  if (temp_dir == wxEmptyString)
    wxGetEnv(wxT("TEMP"), &temp_dir);
  if ((temp_dir == wxEmptyString) && wxDirExists(wxT("/tmp")))
    temp_dir = wxT("/tmp");
  if (temp_dir != wxEmptyString)
    temp_dir += wxT(PATHSEP);

  return temp_dir;
}

wxString
get_installation_dir() {
  wxConfig cfg(wxT("mkvmergeGUI"), wxEmptyString, wxEmptyString, wxEmptyString, wxCONFIG_USE_GLOBAL_FILE);
  cfg.SetPath(wxT("/GUI"));

  wxString path;
  if (cfg.Read(wxT("installation_path"), &path))
    return path;

  return wxT("");
}

wxString
create_track_order(bool all) {
  int i;
  wxString s, format;
  std::string temp;

  fix_format("%d:" LLD, temp);
  format = wxU(temp);
  for (i = 0; i < tracks.size(); i++) {
    if (!all && (!tracks[i]->enabled || tracks[i]->appending || ('c' == tracks[i]->type) || ('t' == tracks[i]->type)))
      continue;

    if (s.length() > 0)
      s += wxT(",");
    s += wxString::Format(format, tracks[i]->source, tracks[i]->id);
  }

  return s;
}

wxString
create_append_mapping() {
  int i;
  wxString s, format;
  std::string temp;

  fix_format("%d:" LLD ":%d:" LLD, temp);
  format = wxU(temp);
  for (i = 1; i < tracks.size(); i++) {
    if (!tracks[i]->enabled || !tracks[i]->appending || ('c' == tracks[i]->type) || ('t' == tracks[i]->type))
      continue;

    if (s.length() > 0)
      s += wxT(",");
    s += wxString::Format(format, tracks[i]->source, tracks[i]->id, tracks[i - 1]->source, tracks[i - 1]->id);
  }

  return s;
}

int
default_track_checked(char type) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i]->type == type) && (1 == tracks[i]->default_track))
      return i;
  return -1;
}

void
set_combobox_selection(wxComboBox *cb,
                       const wxString wanted) {
  int i, count;

  cb->SetValue(wanted);
  count = cb->GetCount();
  for (i = 0; count > i; ++i)
    if (cb->GetString(i) == wanted) {
      cb->SetSelection(i);
      break;
    }
}

#if defined(USE_WXBITMAPCOMBOBOX)
void
set_combobox_selection(wxBitmapComboBox *cb,
                       const wxString wanted) {
  int i, count;

  cb->SetValue(wanted);
  count = cb->GetCount();
  for (i = 0; count > i; ++i)
    if (cb->GetString(i) == wanted) {
      cb->SetSelection(i);
      break;
    }
}
#endif  // USE_WXBITMAPCOMBOBOX

void
wxdie(const wxString &errmsg) {
  wxMessageBox(errmsg, wxT("A serious error has occured"),
               wxOK | wxICON_ERROR);
  exit(1);
}

void
set_menu_item_strings(wxFrame *frame,
                      int id,
                      const wxString &title,
                      const wxString &help_text) {
  wxMenuItem *item = frame->GetMenuBar()->FindItem(id);
  if (NULL != item) {
    item->SetItemLabel(title);
    if (!help_text.IsEmpty())
      item->SetHelp(help_text);
  }
}
