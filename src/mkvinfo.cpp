/*
 * mkvinfo -- utility for gathering information about Matroska files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * retrieves and displays information about a Matroska file
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

// {{{ includes

#include "os.h"

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <typeinfo>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTagMulti.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>
#include <matroska/KaxVersion.h>

#if !defined(MATROSKA_VERSION)
#define MATROSKA_VERSION 2
#endif

#include "mkvinfo.h"
#include "mkvinfo_tag_types.h"

#include "checksums.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

// }}}

// {{{ structs, output/helper functions

typedef struct {
  unsigned int tnum, tuid;
  char type;
  int64_t size;
} kax_track_t;

kax_track_t **tracks = NULL;
int num_tracks = 0;
bool use_gui = false;
bool calc_checksums = false;
bool show_summary = false;
uint64_t tc_scale = TIMECODE_SCALE;

void
add_track(kax_track_t *s) {
  tracks = (kax_track_t **)saferealloc(tracks, sizeof(kax_track_t *) *
                                       (num_tracks + 1));
  tracks[num_tracks] = s;
  num_tracks++;
}

kax_track_t *
find_track(int tnum) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tnum == tnum)
      return tracks[i];

  return NULL;
}

kax_track_t *
find_track_by_uid(int tuid) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tuid == tuid)
      return tracks[i];

  return NULL;
}

void
usage() {
#ifdef HAVE_WXWINDOWS
  mxinfo(_(
    "Usage: mkvinfo [options] inname\n\n"
    " options:\n"
    "  -g, --gui      Start the GUI (and open inname if it was given).\n"
    "  inname         Use 'inname' as the source.\n"
    "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
    "                 description of what mkvinfo outputs.\n"
    "  -c, --checksum Calculate and display checksums of frame contents.\n"
    "  -s, --summary  Only show summaries of the contents, not each element.\n"
    "  -h, --help     Show this help.\n"
    "  -V, --version  Show version information.\n"));
#else
  mxinfo(_(
    "Usage: mkvinfo [options] inname\n\n"
    " options:\n"
    "  inname         Use 'inname' as the source.\n"
    "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
    "                 description of what mkvinfo outputs.\n"
    "  -c, --checksum Calculate and display checksums of frame contents.\n"
    "  -s, --summary  Only show summaries of the contents, not each element.\n"
    "  -h, --help     Show this help.\n"
    "  -V, --version  Show version information.\n"));
#endif
}

string
UTFstring_to_string_local(const UTFstring &src) {
  string retval;
  char *str;
  str = UTFstring_to_cstr(src);
  retval = str;
  safefree(str);
  return retval;
}

#if defined(HAVE_WXWINDOWS) && WXUNICODE
string
UTFstring_to_string(const UTFstring &src) {
  string retval;
  char *str;

  if (!use_gui)
    return UTFstring_to_string_local(src);
  str = UTFstring_to_cstrutf8(src);
  retval = str;
  safefree(str);
  return retval;
}
#else  // HAVE_WXWINDOWS && WXUNICODE
#define UTFstring_to_string(src) UTFstring_to_string_local(src)
#endif // HAVE_WXWINDOWS && WXUNICODE
#define UTF2STR(s) UTFstring_to_string(UTFstring(s)).c_str()

#define ARGS_BUFFER_LEN (200 * 1024) // Ok let's be ridiculous here :)
static char args_buffer[ARGS_BUFFER_LEN];

void
show_error(const char *fmt,
           ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, new_fmt.c_str(), ap);
  va_end(ap);

#ifdef HAVE_WXWINDOWS
  if (use_gui)
    frame->show_error(wxU(args_buffer));
  else
#endif
    mxinfo("(%s) %s\n", NAME, args_buffer);
}

#define show_warning(l, f, args...) _show_element(NULL, NULL, false, l, f, \
                                                  ## args)
#define show_unknown_element(e, l) \
  _show_element(e, es, true, l, _("Unknown element: %s"), \
                e->Generic().DebugName)
#define show_element(e, l, s, args...) _show_element(e, es, false, l, s, \
                                                     ## args)

void
_show_element(EbmlElement *l,
              EbmlStream *es,
              bool skip,
              int level,
              const char *fmt,
              ...) {
  va_list ap;
  char level_buffer[10];
  string new_fmt;
  EbmlMaster *m;
  int i;

  if (show_summary)
    return;

  if (level > 9)
    die("mkvinfo.cpp/show_element(): level > 9: %d", level);

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, new_fmt.c_str(), ap);
  va_end(ap);

  if (!use_gui) {
    memset(&level_buffer[1], ' ', 9);
    level_buffer[0] = '|';
    level_buffer[level] = 0;
    mxinfo("%s+ %s", level_buffer, args_buffer);
    if ((verbose > 1) && (l != NULL))
      mxinfo(" at %llu", l->GetElementPosition());
    mxinfo("\n");
  }
#ifdef HAVE_WXWINDOWS
  else {
    if (l != NULL)
      mxprints(&args_buffer[strlen(args_buffer)], " at %llu",
               l->GetElementPosition());
    frame->add_item(level, wxU(args_buffer));
  }
#endif // HAVE_WXWINDOWS

  if ((l != NULL) && skip) {
    // Dump unknown elements recursively.
    m = dynamic_cast<EbmlMaster *>(l);
    if (m != NULL)
      for (i = 0; i < m->ListSize(); i++)
        show_unknown_element((*m)[i], level + 1);
    l->SkipData(*es, l->Generic().Context);
  }
}

// }}}

// {{{ FUNCTION parse_args

void
parse_args(int argc,
           char **argv,
           char *&file_name) {
  int i;

  verbose = 0;
  file_name = NULL;

  use_gui = false;

  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gui")) {
#ifndef HAVE_WXWINDOWS
      mxerror(_("mkvinfo was compiled without GUI support.\n"));
#else // HAVE_WXWINDOWS
      use_gui = true;
#endif // HAVE_WXWINDOWS
    } else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      mxinfo("mkvinfo v" VERSION "\n");
      mxexit(0);
    } else if (!strcmp(argv[i], "-v") || ! strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
             !strcmp(argv[i], "--help")) {
      usage();
      mxexit(0);
    } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--checksum"))
      calc_checksums = true;
    else if (!strcmp(argv[i], "-C") || !strcmp(argv[i], "--check-mode")) {
      calc_checksums = true;
      verbose = 4;
    } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--summary")) {
      calc_checksums = true;
      show_summary = true;
    } else if (file_name != NULL)
      mxerror(_("Only one input file is allowed.\n"));
    else
      file_name = argv[i];
}

// }}}

// {{{ is_global, parse_multicomment/chapter_atom, asctime_r, gmtime_r

#define fits_parent(l, p) (l->GetElementPosition() < \
                           (p->GetElementPosition() + p->ElementSize()))
#define in_parent(p) (in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

bool
is_global(EbmlStream *es,
          EbmlElement *l,
          int level) {
  if (is_id(l, EbmlVoid)) {
    EbmlVoid *v;
    string s;

    v = static_cast<EbmlVoid *>(l);
    s = string("EbmlVoid (size: ") +
      to_string(l->ElementSize() - l->HeadSize()) + string(")");
    show_element(l, level, s.c_str());

    return true;

  } else if (is_id(l, EbmlCrc32)) {
    show_element(l, level, "EbmlCrc32");

    return true;
  }

  return false;
}

bool
parse_multicomment(EbmlStream *es,
                   EbmlElement *l0,
                   int level) {
  EbmlMaster *m0;
  EbmlElement *l1;
  int i0;

  if (is_id(l0, KaxTagMultiComment)) {
    show_element(l0, level, "Multi comment");

    m0 = static_cast<EbmlMaster *>(l0);
    for (i0 = 0; i0 < m0->ListSize(); i0++) {
      l1 = (*m0)[i0];

      if (is_id(l1, KaxTagMultiCommentName)) {
        KaxTagMultiCommentName &c_name =
          *static_cast<KaxTagMultiCommentName *>(l1);
        show_element(l1, level + 1, "Name: %s", string(c_name).c_str());

      } else if (is_id(l1, KaxTagMultiCommentComments)) {
        KaxTagMultiCommentComments &c_comments =
          *static_cast<KaxTagMultiCommentComments *>(l1);
        show_element(l1, level + 1, "Comments: %s", UTF2STR(c_comments));

      } else if (is_id(l1, KaxTagMultiCommentLanguage)) {
        KaxTagMultiCommentLanguage &c_language =
          *static_cast<KaxTagMultiCommentLanguage *>(l1);
        show_element(l1, level + 1, "Language: %s",
                     string(c_language).c_str());

      } else if (!is_global(es, l1, level + 1))
        show_unknown_element(l1, level + 1);

    } // while (l1 != NULL)

    return true;

  } 

  return false;
}

string format_binary(EbmlBinary &bin, int max_len = 10);

bool parse_simpletag(EbmlStream *es,
                     EbmlElement *l0,
                     int level) {
  EbmlMaster *m0;
  EbmlElement *l1;
  int i0;
  string s;

  if (is_id(l0, KaxTagSimple)) {
    show_element(l0, level, "Simple tag");

    m0 = static_cast<EbmlMaster *>(l0);
    for (i0 = 0; i0 < m0->ListSize(); i0++) {
      l1 = (*m0)[i0];

      if (is_id(l1, KaxTagName)) {
        KaxTagName &c_name = *static_cast<KaxTagName *>(l1);
        show_element(l1, level + 1, "Name: %s", UTF2STR(c_name));

      } else if (is_id(l1, KaxTagString)) {
        KaxTagString &c_string = *static_cast<KaxTagString *>(l1);
        show_element(l1, level + 1, "String: %s", UTF2STR(c_string));

      } else if (is_id(l1, KaxTagBinary)) {
        KaxTagBinary &c_binary = *static_cast<KaxTagBinary *>(l1);
        s = format_binary(c_binary);
        show_element(l1, level + 1, "Binary: %s", s.c_str());

      } else if (!is_global(es, l1, level + 1) &&
                 !parse_simpletag(es, l1, level + 1))
        show_unknown_element(l1, level + 1);

    } // while (l1 != NULL)

    return true;

  } 

  return false;
}

bool
parse_chapter_atom(EbmlStream *es,
                   EbmlElement *l0,
                   int level) {
  EbmlMaster *m0, *m1;
  EbmlElement *l1, *l2;
  int i0, i1;

  if (is_id(l0, KaxChapterAtom)) {
    show_element(l0, level, "Chapter atom");

    m0 = static_cast<EbmlMaster *>(l0);
    for (i0 = 0; i0 < m0->ListSize(); i0++) {
      l1 = (*m0)[i0];

      if (is_id(l1, KaxChapterUID)) {
        KaxChapterUID &c_uid = *static_cast<KaxChapterUID *>(l1);
        show_element(l1, level + 1, "UID: %u", uint32(c_uid));

      } else if (is_id(l1, KaxChapterTimeStart)) {
        uint64_t s;
        KaxChapterTimeStart &start =
          *static_cast<KaxChapterTimeStart *>(l1);
        s = uint64(start);
        show_element(l1, level + 1, "Start: " FMT_TIMECODEN, ARG_TIMECODEN(s));

      } else if (is_id(l1, KaxChapterTimeEnd)) {
        uint64_t e;
        KaxChapterTimeEnd &end =
          *static_cast<KaxChapterTimeEnd *>(l1);
        e = uint64(end);
        show_element(l1, level + 1, "End: " FMT_TIMECODEN, ARG_TIMECODEN(e));

      } else if (is_id(l1, KaxChapterTrack)) {
        show_element(l1, level + 1, "Track");

        m1 = static_cast<EbmlMaster *>(l1);
        for (i1 = 0; i1 < m1->ListSize(); i1++) {
          l2 = (*m1)[i1];

          if (is_id(l2, KaxChapterTrackNumber)) {
            KaxChapterTrackNumber &c_tnumber =
              *static_cast<KaxChapterTrackNumber *>(l2);
            show_element(l2, level + 2, "Track number: %u", uint32(c_tnumber));

          } else if (!is_global(es, l2, level + 2))
            show_unknown_element(l2, level + 2);

        } // while (l2 != NULL)

      } else if (is_id(l1, KaxChapterDisplay)) {
        show_element(l1, level + 1, "Display");

        m1 = static_cast<EbmlMaster *>(l1);
        for (i1 = 0; i1 < m1->ListSize(); i1++) {
          l2 = (*m1)[i1];

          if (is_id(l2, KaxChapterString)) {
            KaxChapterString &c_string =
              *static_cast<KaxChapterString *>(l2);
            show_element(l2, level + 2, "String: %s", UTF2STR(c_string));

          } else if (is_id(l2, KaxChapterLanguage)) {
            KaxChapterLanguage &c_lang =
              *static_cast<KaxChapterLanguage *>(l2);
            show_element(l2, level + 2, "Language: %s",
                         string(c_lang).c_str());

          } else if (is_id(l2, KaxChapterCountry)) {
            KaxChapterCountry &c_country =
              *static_cast<KaxChapterCountry *>(l2);
            show_element(l2, level + 2, "Country: %s",
                         string(c_country).c_str());

          } else if (!is_global(es, l2, level + 2))
            show_unknown_element(l2, level + 2);

        } // while (l2 != NULL)

      } else if (is_id(l1, KaxChapterFlagHidden)) {
        show_element(l1, level + 1, "Hidden: %u",
                     uint8(*static_cast<KaxChapterFlagHidden *>(l1)));

      } else if (is_id(l1, KaxChapterFlagEnabled)) {
        show_element(l1, level + 1, "Enabled: %u",
                     uint8(*static_cast<KaxChapterFlagEnabled *>(l1)));

      } else if (!parse_chapter_atom(es, l1, level + 1) &&
                 !is_global(es, l1, level + 1))
        show_unknown_element(l1, level + 1);

    } // while (l1 != NULL)

    return true;

  } 

  return false;
}

#if defined(COMP_MSC) || defined(COMP_MINGW)
struct tm *
gmtime_r(const time_t *timep,
         struct tm *result) {
  struct tm *aresult;

  aresult = gmtime(timep);
  memcpy(result, aresult, sizeof(struct tm));

  return result;
}

char *
asctime_r(const struct tm *tm,
          char *buf) {
  char *abuf;

  abuf = asctime(tm);
  strcpy(buf, abuf);

  return abuf;
}
#endif

// }}}

void
sort_master(EbmlMaster &m) {
  int i, sp_idx;
  vector<EbmlElement *> tmp;
  int64_t smallest_pos;
  EbmlMaster *m2;

  while (m.ListSize() > 0) {
    sp_idx = 0;
    smallest_pos = m[0]->GetElementPosition();

    for (i = 1; i < m.ListSize(); i++)
      if (m[i]->GetElementPosition() < smallest_pos) {
        sp_idx = i;
        smallest_pos = m[i]->GetElementPosition();
      }

    tmp.push_back(m[sp_idx]);
    m.Remove(sp_idx);
  }

  for (i = 0; i < tmp.size(); i++) {
    m.PushElement(*tmp[i]);
    if ((m2 = dynamic_cast<EbmlMaster *>(tmp[i])) != NULL)
      sort_master(*m2);
  }
}

void
read_master(EbmlMaster *m,
            EbmlStream *es,
            const EbmlSemanticContext &ctx,
            int &upper_lvl_el,
            EbmlElement *&l2) {

  m->Read(*es, ctx, upper_lvl_el, l2, true);
  if (m->ListSize() == 0)
    return;

  sort_master(*m);
}

string
format_binary(EbmlBinary &bin,
              int max_len) {
  int len, i;
  string result;

  if (bin.GetSize() > max_len)
    len = max_len;
  else
    len = bin.GetSize();
  char buffer[40 + len * 5 + 1 + 3 + 24];
  const unsigned char *b = (const unsigned char *)&binary(bin);
  buffer[0] = 0;
  mxprints(buffer, "length %lld, data:", bin.GetSize());
  for (i = 0; i < len; i++)
    mxprints(&buffer[strlen(buffer)], " 0x%02x", b[i]);
  if (len < bin.GetSize())
    mxprints(&buffer[strlen(buffer)], "...");
  if (calc_checksums)
    mxprints(&buffer[strlen(buffer)], " (adler: 0x%08x)",
             calc_adler32(&binary(bin), bin.GetSize()));
  result = buffer;
  strip(result);

  return result;
}

#define handle(f) handle_##f(in, es, upper_lvl_el, l1, l2, l3, l4, l5, l6)
#define def_handle(f) handle_##f(mm_io_c *&in, \
                                 EbmlStream *&es, \
                                 int &upper_lvl_el, \
                                 EbmlElement *&l1, \
                                 EbmlElement *&l2, \
                                 EbmlElement *&l3, \
                                 EbmlElement *&l4, \
                                 EbmlElement *&l5, \
                                 EbmlElement *&l6)
#define handle2(f, args...) \
  handle_##f(in, es, upper_lvl_el, l1, l2, l3, l4, l5, l6, ##args)
#define def_handle2(f, args...) handle_##f(mm_io_c *&in, \
                                           EbmlStream *&es, \
                                           int &upper_lvl_el, \
                                           EbmlElement *&l1, \
                                           EbmlElement *&l2, \
                                           EbmlElement *&l3, \
                                           EbmlElement *&l4, \
                                           EbmlElement *&l5, \
                                           EbmlElement *&l6, \
                                           ##args)


void
def_handle(info) {
  EbmlMaster *m1;
  int i1, i;

  // General info about this Matroska file
  show_element(l1, 1, "Segment information");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxTimecodeScale)) {
      KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
      tc_scale = uint64(ktc_scale);
      show_element(l2, 2, "Timecode scale: %llu", tc_scale);

    } else if (is_id(l2, KaxDuration)) {
      KaxDuration &duration = *static_cast<KaxDuration *>(l2);
      show_element(l2, 2, "Duration: %.3fs (" FMT_TIMECODEN ")",
                   float(duration) * tc_scale / 1000000000.0,
                   ARG_TIMECODEN(float(duration) * tc_scale));

    } else if (is_id(l2, KaxMuxingApp)) {
      KaxMuxingApp &muxingapp = *static_cast<KaxMuxingApp *>(l2);
      show_element(l2, 2, "Muxing application: %s", UTF2STR(muxingapp));

    } else if (is_id(l2, KaxWritingApp)) {
      KaxWritingApp &writingapp = *static_cast<KaxWritingApp *>(l2);
      show_element(l2, 2, "Writing application: %s",
                   UTF2STR(writingapp));

    } else if (is_id(l2, KaxDateUTC)) {
      struct tm tmutc;
      time_t temptime;
      char buffer[40];
      KaxDateUTC &dateutc = *static_cast<KaxDateUTC *>(l2);
      temptime = dateutc.GetEpochDate();
      if ((gmtime_r(&temptime, &tmutc) != NULL) &&
          (asctime_r(&tmutc, buffer) != NULL)) {
        buffer[strlen(buffer) - 1] = 0;
        show_element(l2, 2, "Date: %s UTC", buffer);
      } else
        show_element(l2, 2, "Date (invalid, value: %d)", temptime);

    } else if (is_id(l2, KaxSegmentUID)) {
      KaxSegmentUID &uid = *static_cast<KaxSegmentUID *>(l2);
      char buffer[uid.GetSize() * 5 + 1];
      const unsigned char *b = (const unsigned char *)&binary(uid);
      buffer[0] = 0;
      for (i = 0; i < uid.GetSize(); i++)
        mxprints(&buffer[strlen(buffer)], " 0x%02x", b[i]);
      show_element(l2, 2, "Segment UID:%s", buffer);

    } else if (is_id(l2, KaxPrevUID)) {
      KaxPrevUID &uid = *static_cast<KaxPrevUID *>(l2);
      char buffer[uid.GetSize() * 5 + 1];
      const unsigned char *b = (const unsigned char *)&binary(uid);
      buffer[0] = 0;
      for (i = 0; i < uid.GetSize(); i++)
        mxprints(&buffer[strlen(buffer)], " 0x%02x", b[i]);
      show_element(l2, 2, "Previous segment UID:%s", buffer);

    } else if (is_id(l2, KaxPrevFilename)) {
      KaxPrevFilename &filename = *static_cast<KaxPrevFilename *>(l2);
      show_element(l2, 2, "Previous filename: %s", UTF2STR(filename));

    } else if (is_id(l2, KaxNextUID)) {
      KaxNextUID &uid = *static_cast<KaxNextUID *>(l2);
      char buffer[uid.GetSize() * 5 + 1];
      const unsigned char *b = (const unsigned char *)&binary(uid);
      buffer[0] = 0;
      for (i = 0; i < uid.GetSize(); i++)
        mxprints(&buffer[strlen(buffer)], " 0x%02x", b[i]);
      show_element(l2, 2, "Next segment UID:%s", buffer);

    } else if (is_id(l2, KaxNextFilename)) {
      KaxNextFilename &filename = *static_cast<KaxNextFilename *>(l2);
      show_element(l2, 2, "Next filename: %s", UTF2STR(filename));

    } else if (is_id(l2, KaxSegmentFilename)) {
      KaxSegmentFilename &filename =
        *static_cast<KaxSegmentFilename *>(l2);
      show_element(l2, 2, "Segment filename: %s", UTF2STR(filename));

    } else if (is_id(l2, KaxTitle)) {
      KaxTitle &title = *static_cast<KaxTitle *>(l2);
      show_element(l2, 2, "Title: %s", UTF2STR(title));

    }
  }
}

void
def_handle2(audio_track,
            vector<string> &summary) {
  EbmlMaster *m3;
  int i3;

  show_element(l3, 3, "Audio track");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxAudioSamplingFreq)) {
      KaxAudioSamplingFreq &freq =
        *static_cast<KaxAudioSamplingFreq *>(l4);
      show_element(l4, 4, "Sampling frequency: %f",
                   float(freq));
      summary.push_back(mxsprintf("sampling freq: %f",
                                  float(freq)));

    } else if (is_id(l4, KaxAudioOutputSamplingFreq)) {
      KaxAudioOutputSamplingFreq &ofreq =
        *static_cast<KaxAudioOutputSamplingFreq *>(l4);
      show_element(l4, 4, "Output sampling frequency: %f",
                   float(ofreq));
      summary.push_back(mxsprintf("output sampling freq: %f",
                                  float(ofreq)));

    } else if (is_id(l4, KaxAudioChannels)) {
      KaxAudioChannels &channels =
        *static_cast<KaxAudioChannels *>(l4);
      show_element(l4, 4, "Channels: %u", uint32(channels));
      summary.push_back(mxsprintf("channels: %u",
                                  uint32(channels)));

#if MATROSKA_VERSION >= 2
    } else if (is_id(l4, KaxAudioPosition)) {
      string strc;

      KaxAudioPosition &positions =
        *static_cast<KaxAudioPosition *>(l4);
      strc = format_binary(positions);
      show_element(l4, 4, "Channel positions: %s",
                   strc.c_str());
#endif

    } else if (is_id(l4, KaxAudioBitDepth)) {
      KaxAudioBitDepth &bps =
        *static_cast<KaxAudioBitDepth *>(l4);
      show_element(l4, 4, "Bit depth: %u", uint32(bps));
      summary.push_back(mxsprintf("bits per sample: %u",
                                  uint32(bps)));

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle2(video_track,
            vector<string> &summary) {
  EbmlMaster *m3;
  int i3;
  string strc;

  show_element(l3, 3, "Video track");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxVideoPixelWidth)) {
      KaxVideoPixelWidth &width =
        *static_cast<KaxVideoPixelWidth *>(l4);
      show_element(l4, 4, "Pixel width: %u", uint16(width));
      summary.push_back(mxsprintf("pixel width: %u", uint32(width)));

    } else if (is_id(l4, KaxVideoPixelHeight)) {
      KaxVideoPixelHeight &height =
        *static_cast<KaxVideoPixelHeight *>(l4);
      show_element(l4, 4, "Pixel height: %u", uint16(height));
      summary.push_back(mxsprintf("pixel height: %u", uint32(height)));

    } else if (is_id(l4, KaxVideoDisplayWidth)) {
      KaxVideoDisplayWidth &width =
        *static_cast<KaxVideoDisplayWidth *>(l4);
      show_element(l4, 4, "Display width: %u", uint16(width));
      summary.push_back(mxsprintf("display width: %u", uint32(width)));

    } else if (is_id(l4, KaxVideoDisplayHeight)) {
      KaxVideoDisplayHeight &height =
        *static_cast<KaxVideoDisplayHeight *>(l4);
      show_element(l4, 4, "Display height: %u", uint16(height));
      summary.push_back(mxsprintf("display height: %u", uint32(height)));

#if MATROSKA_VERSION >= 2
    } else if (is_id(l4, KaxVideoDisplayUnit)) {
      KaxVideoDisplayUnit &unit =
        *static_cast<KaxVideoDisplayUnit *>(l4);
      show_element(l4, 4, "Display unit: %u%s", uint16(unit),
                   uint16(unit) == 0 ? " (pixels)" :
                   uint16(unit) == 1 ? " (centimeters)" :
                   uint16(unit) == 2 ? " (inches)" : "");

    } else if (is_id(l4, KaxVideoGamma)) {
      KaxVideoGamma &gamma =
        *static_cast<KaxVideoGamma *>(l4);
      show_element(l4, 4, "Gamma: %f", float(gamma));

    } else if (is_id(l4, KaxVideoFlagInterlaced)) {
      KaxVideoFlagInterlaced &f_interlaced =
        *static_cast<KaxVideoFlagInterlaced *>(l4);
      show_element(l4, 4, "Interlaced: %u",
                   uint8(f_interlaced));

    } else if (is_id(l4, KaxVideoStereoMode)) {
      KaxVideoStereoMode &stereo =
        *static_cast<KaxVideoStereoMode *>(l4);
      show_element(l4, 4, "Stereo mode: %u%s", uint8(stereo),
                   uint8(stereo) == 0 ? " (mono)" :
                   uint8(stereo) == 1 ? " (right eye)" :
                   uint8(stereo) == 2 ? " (left eye)" :
                   uint8(stereo) == 3 ? " (both eyes)" : "");

    } else if (is_id(l4, KaxVideoAspectRatio)) {
      KaxVideoAspectRatio &ar_type =
        *static_cast<KaxVideoAspectRatio *>(l4);
      show_element(l4, 4, "Aspect ratio type: %u%s",
                   uint8(ar_type),
                   uint8(ar_type) == 0 ? " (free resizing)" :
                   uint8(ar_type) == 1 ? " (keep aspect ratio)" :
                   uint8(ar_type) == 2 ? " (fixed)" : "");
#endif
    } else if (is_id(l4, KaxVideoColourSpace)) {
      KaxVideoColourSpace &cspace =
        *static_cast<KaxVideoColourSpace *>(l4);
      strc = format_binary(cspace);
      show_element(l4, 4, "Colour space: %s", strc.c_str());

    } else if (is_id(l4, KaxVideoFrameRate)) {
      KaxVideoFrameRate &framerate =
        *static_cast<KaxVideoFrameRate *>(l4);
      show_element(l4, 4, "Frame rate: %f", float(framerate));

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(content_encodings) {
  EbmlMaster *m3, *m4, *m5;
  int i3, i4, i5;
  string strc;

  show_element(l3, 3, "Content encodings");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxContentEncoding)) {
      show_element(l4, 4, "Content encoding");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxContentEncodingOrder)) {
          KaxContentEncodingOrder &ce_order =
            *static_cast<KaxContentEncodingOrder *>(l5);
          show_element(l5, 5, "Order: %u", uint32(ce_order));

        } else if (is_id(l5,  KaxContentEncodingScope)) {
          string scope;
          KaxContentEncodingScope &ce_scope =
            *static_cast<KaxContentEncodingScope *>(l5);
          if ((uint32(ce_scope) & 0x01) == 0x01)
            scope = "1: all frames";
          if ((uint32(ce_scope) & 0x02) == 0x02) {
            if (scope.length() > 0)
              scope += ", ";
            scope += "2: codec private data";
          }
          if ((uint32(ce_scope) & 0xfc) != 0x00) {
            if (scope.length() > 0)
              scope += ", ";
            scope += "rest: unknown";
          }
          if (scope.length() == 0)
            scope = "unknown";
          show_element(l5, 5, "Scope: %u (%s)", uint32(ce_scope),
                       scope.c_str());

        } else if (is_id(l5,  KaxContentEncodingType)) {
          uint32_t ce_type;
          ce_type =
            uint32(*static_cast<KaxContentEncodingType *>(l5));
          show_element(l5, 5, "Type: %u (%s)", ce_type,
                       ce_type == 0 ? "compression" :
                       ce_type == 1 ? "encryption" :
                       "unknown");

        } else if (is_id(l5, KaxContentCompression)) {
          show_element(l5, 5, "Content compression");

          m5 = static_cast<EbmlMaster *>(l5);
          for (i5 = 0; i5 < m5->ListSize(); i5++) {
            l6 = (*m5)[i5];

            if (is_id(l6, KaxContentCompAlgo)) {
              uint32_t c_algo =
                uint32(*static_cast<KaxContentCompAlgo *>(l6));
              show_element(l6, 6, "Algorithm: %u (%s)", c_algo,
                           c_algo == 0 ? "ZLIB" :
                           c_algo == 1 ? "bzLib" :
                           "unknown");

            } else if (is_id(l6, KaxContentCompSettings)) {
              KaxContentCompSettings &c_settings =
                *static_cast<KaxContentCompSettings *>(l6);
              strc = format_binary(c_settings);
              show_element(l6, 6, "Settings: %s", strc.c_str());

            } else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);

          }

        } else if (is_id(l5, KaxContentEncryption)) {
          show_element(l5, 5, "Content encryption");

          m5 = static_cast<EbmlMaster *>(l5);
          for (i5 = 0; i5 < m5->ListSize(); i5++) {
            l6 = (*m5)[i5];

            if (is_id(l6, KaxContentEncAlgo)) {
              uint32_t e_algo =
                uint32(*static_cast<KaxContentEncAlgo *>(l6));
              show_element(l6, 6, "Encryption algorithm: %u "
                           "(%s)", e_algo,
                           e_algo == 0 ? "none" :
                           e_algo == 1 ? "DES" :
                           e_algo == 2 ? "3DES" :
                           e_algo == 3 ? "Twofish" :
                           e_algo == 4 ? "Blowfish" :
                           e_algo == 5 ? "AES" :
                           "unknown");

            } else if (is_id(l6, KaxContentEncKeyID)) {
              KaxContentEncKeyID &e_keyid =
                *static_cast<KaxContentEncKeyID *>(l6);
              strc = format_binary(e_keyid);
              show_element(l6, 6, "Encryption key ID: %s",
                           strc.c_str());

            } else if (is_id(l6, KaxContentSigAlgo)) {
              uint32_t s_algo =
                uint32(*static_cast<KaxContentSigAlgo *>(l6));
              show_element(l6, 6, "Signature algorithm: %u (%s)",
                           s_algo,
                           s_algo == 0 ? "none" :
                           s_algo == 1 ? "RSA" :
                           "unknown");

            } else if (is_id(l6, KaxContentSigHashAlgo)) {
              uint32_t s_halgo =
                uint32(*static_cast<KaxContentSigHashAlgo *>
                       (l6));
              show_element(l6, 6, "Signature hash algorithm: %u "
                           "(%s)", s_halgo,
                           s_halgo == 0 ? "none" :
                           s_halgo == 1 ? "SHA1-160" :
                           s_halgo == 2 ? "MD5" :
                           "unknown");

            } else if (is_id(l6, KaxContentSigKeyID)) {
              KaxContentSigKeyID &s_keyid =
                *static_cast<KaxContentSigKeyID *>(l6);
              strc = format_binary(s_keyid);
              show_element(l6, 6, "Signature key ID: %s",
                           strc.c_str());

            } else if (is_id(l6, KaxContentSignature)) {
              KaxContentSignature &sig =
                *static_cast<KaxContentSignature *>(l6);
              strc = format_binary(sig);
              show_element(l6, 6, "Signature: %s", strc.c_str());

            } else if (!is_global(es, l6, 6))
              show_unknown_element(l6, 6);

          }

        } else if (!is_global(es, l5, 5))
          show_unknown_element(l5, 5);

      }

    } else if (!is_global(es, l4, 4))
      show_unknown_element(l4, 4);

  }
}

void
def_handle(tracks) {
  EbmlMaster *m1, *m2;
  int i1, i2;
  string strc;

  // Yep, we've found our KaxTracks element. Now find all tracks
  // contained in this segment.
  show_element(l1, 1, "Segment tracks");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxTrackEntry)) {
      int64_t kax_track_number;
      vector<string> summary;
      string fourcc_buffer, kax_codec_id;
      char kax_track_type;
      bool ms_compat;

      // We actually found a track entry :) We're happy now.
      show_element(l2, 2, "A track");

      kax_track_type = '?';
      kax_track_number = -1;
      kax_codec_id = "";
      fourcc_buffer = "";
      ms_compat = false;

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        // Now evaluate the data belonging to this track
        if (is_id(l3, KaxTrackAudio))
          handle2(audio_track, summary);

        else if (is_id(l3, KaxTrackVideo))
          handle2(video_track, summary);

        else if (is_id(l3, KaxTrackNumber)) {
          KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
          show_element(l3, 3, "Track number: %u", uint32(tnum));
          if (find_track(uint32(tnum)) != NULL)
            show_warning(3, "Warning: There's more than one "
                         "track with the number %u.", uint32(tnum));
          kax_track_number = uint32(tnum);

        } else if (is_id(l3, KaxTrackUID)) {
          KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
          show_element(l3, 3, "Track UID: %u", uint32(tuid));
          if (find_track_by_uid(uint32(tuid)) != NULL)
            show_warning(3, "Warning: There's more than one "
                         "track with the UID %u.", uint32(tuid));

        } else if (is_id(l3, KaxTrackType)) {
          KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);

          switch (uint8(ttype)) {
            case track_audio:
              kax_track_type = 'a';
              break;
            case track_video:
              kax_track_type = 'v';
              break;
            case track_subtitle:
              kax_track_type = 's';
              break;
            default:
              kax_track_type = '?';
              break;
          }
          show_element(l3, 3, "Track type: %s",
                       kax_track_type == 'a' ? "audio" :
                       kax_track_type == 'v' ? "video" :
                       kax_track_type == 's' ? "subtitles" :
                       "unknown");

#if MATROSKA_VERSION >= 2
        } else if (is_id(l3, KaxTrackFlagEnabled)) {
          KaxTrackFlagEnabled &fenabled =
            *static_cast<KaxTrackFlagEnabled *>(l3);
          show_element(l3, 3, "Enabled: %u", uint8(fenabled));
#endif

        } else if (is_id(l3, KaxTrackName)) {
          KaxTrackName &name = *static_cast<KaxTrackName *>(l3);
          show_element(l3, 3, "Name: %s", UTF2STR(name));

        } else if (is_id(l3, KaxCodecID)) {
          KaxCodecID &codec_id = *static_cast<KaxCodecID *>(l3);
          show_element(l3, 3, "Codec ID: %s", string(codec_id).c_str());
          if ((!strcmp(string(codec_id).c_str(), MKV_V_MSCOMP) &&
               (kax_track_type == 'v')) ||
              (!strcmp(string(codec_id).c_str(), MKV_A_ACM) &&
               (kax_track_type == 'a')))
            ms_compat = true;
          kax_codec_id = string(codec_id);

        } else if (is_id(l3, KaxCodecPrivate)) {
          fourcc_buffer = "";
          KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate *>(l3);
          if (ms_compat && (kax_track_type == 'v') &&
              (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
            alBITMAPINFOHEADER *bih =
              (alBITMAPINFOHEADER *)&binary(c_priv);
            unsigned char *fcc = (unsigned char *)&bih->bi_compression;
            fourcc_buffer =
              mxsprintf(" (FourCC: %c%c%c%c, 0x%08x)",
                        fcc[0], fcc[1], fcc[2], fcc[3],
                        get_uint32(&bih->bi_compression));
          } else if (ms_compat && (kax_track_type == 'a') &&
                     (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
            alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)&binary(c_priv);
            fourcc_buffer =
              mxsprintf(" (format tag: 0x%04x)",
                        get_uint16(&wfe->w_format_tag));
          }
          if (calc_checksums && !show_summary)
            fourcc_buffer +=
              mxsprintf(" (adler: 0x%08x)",
                        calc_adler32(&binary(c_priv), c_priv.GetSize()));
          show_element(l3, 3, "CodecPrivate, length %d%s",
                       (int)c_priv.GetSize(), fourcc_buffer.c_str());

        } else if (is_id(l3, KaxCodecName)) {
          KaxCodecName &c_name = *static_cast<KaxCodecName *>(l3);
          show_element(l3, 3, "Codec name: %s", UTF2STR(c_name));

#if MATROSKA_VERSION >= 2
        } else if (is_id(l3, KaxCodecSettings)) {
          KaxCodecSettings &c_sets =
            *static_cast<KaxCodecSettings *>(l3);
          show_element(l3, 3, "Codec settings: %s", UTF2STR(c_sets));

        } else if (is_id(l3, KaxCodecInfoURL)) {
          KaxCodecInfoURL &c_infourl =
            *static_cast<KaxCodecInfoURL *>(l3);
          show_element(l3, 3, "Codec info URL: %s",
                       string(c_infourl).c_str());

        } else if (is_id(l3, KaxCodecDownloadURL)) {
          KaxCodecDownloadURL &c_downloadurl =
            *static_cast<KaxCodecDownloadURL *>(l3);
          show_element(l3, 3, "Codec download URL: %s",
                       string(c_downloadurl).c_str());

        } else if (is_id(l3, KaxCodecDecodeAll)) {
          KaxCodecDecodeAll &c_decodeall =
            *static_cast<KaxCodecDecodeAll *>(l3);
          show_element(l3, 3, "Codec decode all: %u",
                       uint16(c_decodeall));

        } else if (is_id(l3, KaxTrackOverlay)) {
          KaxTrackOverlay &overlay = *static_cast<KaxTrackOverlay *>(l3);
          show_element(l3, 3, "Track overlay: %u", uint16(overlay));
#endif // MATROSKA_VERSION >= 2

        } else if (is_id(l3, KaxTrackMinCache)) {
          KaxTrackMinCache &min_cache =
            *static_cast<KaxTrackMinCache *>(l3);
          show_element(l3, 3, "MinCache: %u", uint32(min_cache));

        } else if (is_id(l3, KaxTrackMaxCache)) {
          KaxTrackMaxCache &max_cache =
            *static_cast<KaxTrackMaxCache *>(l3);
          show_element(l3, 3, "MaxCache: %u", uint32(max_cache));

        } else if (is_id(l3, KaxTrackDefaultDuration)) {
          KaxTrackDefaultDuration &def_duration =
            *static_cast<KaxTrackDefaultDuration *>(l3);
          show_element(l3, 3, "Default duration: %.3fms (%.3f fps for "
                       "a video track)",
                       (float)uint64(def_duration) / 1000000.0,
                       1000000000.0 / (float)uint64(def_duration));
          summary.push_back(mxsprintf("default duration: %.3fms (%.3f "
                                      "fps for a video track)",
                                      (float)uint64(def_duration) /
                                      1000000.0, 1000000000.0 /
                                      (float)uint64(def_duration)));

        } else if (is_id(l3, KaxTrackFlagLacing)) {
          KaxTrackFlagLacing &f_lacing =
            *static_cast<KaxTrackFlagLacing *>(l3);
          show_element(l3, 3, "Lacing flag: %d", uint32(f_lacing));

        } else if (is_id(l3, KaxTrackFlagDefault)) {
          KaxTrackFlagDefault &f_default =
            *static_cast<KaxTrackFlagDefault *>(l3);
          show_element(l3, 3, "Default flag: %d", uint32(f_default));

        } else if (is_id(l3, KaxTrackLanguage)) {
          KaxTrackLanguage &language =
            *static_cast<KaxTrackLanguage *>(l3);
          show_element(l3, 3, "Language: %s", string(language).c_str());
          summary.push_back(mxsprintf("language: %s",
                                      string(language).c_str()));

        } else if (is_id(l3, KaxTrackTimecodeScale)) {
          KaxTrackTimecodeScale &ttc_scale =
            *static_cast<KaxTrackTimecodeScale *>(l3);
          show_element(l3, 3, "Timecode scale: %f", float(ttc_scale));

        } else if (is_id(l3, KaxContentEncodings))
          handle(content_encodings);

        else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      }

      if (show_summary)
        mxinfo(_("Track %lld: %s, codec ID: %s%s%s%s\n"), kax_track_number,
               kax_track_type == 'a' ? _("audio") :
               kax_track_type == 'v' ? _("video") :
               kax_track_type == 's' ? _("subtitles") : _("unknown"),
               kax_codec_id.c_str(), fourcc_buffer.c_str(),
               summary.size() > 0 ? ", " : "",
               join(", ", summary).c_str());

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);
  }
}

void
def_handle(seek_head) {
  EbmlMaster *m1, *m2;
  int i1, i2, i;

  if ((verbose < 2) && !use_gui) {
    show_element(l1, 1, "Seek head (subentries will be skipped)");
    return;
  }

  show_element(l1, 1, "Seek head");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxSeek)) {
      show_element(l2, 2, "Seek entry");

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxSeekID)) {
          binary *b;
          int s;
          char pbuffer[100];
          KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
          b = seek_id.GetBuffer();
          s = seek_id.GetSize();
          EbmlId id(b, s);
          pbuffer[0] = 0;
          for (i = 0; i < s; i++)
            mxprints(&pbuffer[strlen(pbuffer)], "0x%02x ",
                     ((unsigned char *)b)[i]);
          show_element(l3, 3, "Seek ID: %s (%s)", pbuffer,
                       (id == KaxInfo::ClassInfos.GlobalId) ?
                       "KaxInfo" :
                       (id == KaxCluster::ClassInfos.GlobalId) ?
                       "KaxCluster" :
                       (id == KaxTracks::ClassInfos.GlobalId) ?
                       "KaxTracks" :
                       (id == KaxCues::ClassInfos.GlobalId) ?
                       "KaxCues" :
                       (id == KaxAttachments::ClassInfos.GlobalId) ?
                       "KaxAttachments" :
                       (id == KaxChapters::ClassInfos.GlobalId) ?
                       "KaxChapters" :
                       (id == KaxTags::ClassInfos.GlobalId) ?
                       "KaxTags" :
                       (id == KaxSeekHead::ClassInfos.GlobalId) ?
                       "KaxSeekHead" :
                       "unknown");

        } else if (is_id(l3, KaxSeekPosition)) {
          KaxSeekPosition &seek_pos =
            static_cast<KaxSeekPosition &>(*l3);
          show_element(l3, 3, "Seek position: %llu", uint64(seek_pos));

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)


    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(cues) {
  EbmlMaster *m1, *m2, *m3;
  int i1, i2, i3;

  if (verbose < 2) {
    show_element(l1, 1, "Cues (subentries will be skipped)");
    return;
  }

  show_element(l1, 1, "Cues");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxCuePoint)) {
      show_element(l2, 2, "Cue point");

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxCueTime)) {
          KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
          show_element(l3, 3, "Cue time: %.3fs", tc_scale * 
                       ((float)uint64(cue_time)) / 1000000000.0);

        } else if (is_id(l3, KaxCueTrackPositions)) {
          show_element(l3, 3, "Cue track positions");

          m3 = static_cast<EbmlMaster *>(l3);
          for (i3 = 0; i3 < m3->ListSize(); i3++) {
            l4 = (*m3)[i3];

            if (is_id(l4, KaxCueTrack)) {
              KaxCueTrack &cue_track = *static_cast<KaxCueTrack *>(l4);
              show_element(l4, 4, "Cue track: %u", uint32(cue_track));

            } else if (is_id(l4, KaxCueClusterPosition)) {
              KaxCueClusterPosition &cue_cp =
                *static_cast<KaxCueClusterPosition *>(l4);
              show_element(l4, 4, "Cue cluster position: %llu",
                           uint64(cue_cp));

            } else if (is_id(l4, KaxCueBlockNumber)) {
              KaxCueBlockNumber &cue_bn =
                *static_cast<KaxCueBlockNumber *>(l4);
              show_element(l4, 4, "Cue block number: %llu",
                           uint64(cue_bn));

#if MATROSKA_VERSION >= 2
            } else if (is_id(l4, KaxCueCodecState)) {
              KaxCueCodecState &cue_cs =
                *static_cast<KaxCueCodecState *>(l4);
              show_element(l4, 4, "Cue codec state: %llu",
                           uint64(cue_cs));

            } else if (is_id(l4, KaxCueReference)) {
              EbmlMaster *m4;

              int i4;
              show_element(l4, 4, "Cue reference");

              m4 = static_cast<EbmlMaster *>(l4);
              for (i4 = 0; i4 < m4->ListSize(); i4++) {
                l5 = (*m4)[i4];

                if (is_id(l5, KaxCueRefTime)) {
                  KaxCueRefTime &cue_rt =
                    *static_cast<KaxCueRefTime *>(l5);
                  show_element(l5, 5, "Cue ref time: %.3fs", tc_scale,
                               ((float)uint64(cue_rt)) / 1000000000.0);

                } else if (is_id(l5, KaxCueRefCluster)) {
                  KaxCueRefCluster &cue_rc =
                    *static_cast<KaxCueRefCluster *>(l5);
                  show_element(l5, 5, "Cue ref cluster: %llu",
                               uint64(cue_rc));

                } else if (is_id(l5, KaxCueRefNumber)) {
                  KaxCueRefNumber &cue_rn =
                    *static_cast<KaxCueRefNumber *>(l5);
                  show_element(l5, 5, "Cue ref number: %llu",
                               uint64(cue_rn));

                } else if (is_id(l5, KaxCueRefCodecState)) {
                  KaxCueRefCodecState &cue_rcs =
                    *static_cast<KaxCueRefCodecState *>(l5);
                  show_element(l5, 5, "Cue ref codec state: %llu",
                               uint64(cue_rcs));

                } else if (!is_global(es, l5, 5))
                  show_unknown_element(l5, 5);

              } // while (l5 != NULL)
#endif // MATROSKA_VERSION >= 2

            } else if (!is_global(es, l4, 4))
              show_unknown_element(l4, 4);

          } // while (l4 != NULL)

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(attachments) {
  EbmlMaster *m1, *m2;
  int i1, i2;

  show_element(l1, 1, "Attachments");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxAttached)) {
      show_element(l2, 2, "Attached");

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxFileDescription)) {
          KaxFileDescription &f_desc =
            *static_cast<KaxFileDescription *>(l3);
          show_element(l3, 3, "File description: %s", UTF2STR(f_desc));

        } else if (is_id(l3, KaxFileName)) {
          KaxFileName &f_name =
            *static_cast<KaxFileName *>(l3);
          show_element(l3, 3, "File name: %s", UTF2STR(f_name));

        } else if (is_id(l3, KaxMimeType)) {
          KaxMimeType &mime_type =
            *static_cast<KaxMimeType *>(l3);
          show_element(l3, 3, "Mime type: %s",
                       string(mime_type).c_str());

        } else if (is_id(l3, KaxFileData)) {
          KaxFileData &f_data =
            *static_cast<KaxFileData *>(l3);
          show_element(l3, 3, "File data, size: %u", f_data.GetSize());

        } else if (is_id(l3, KaxFileUID)) {
          KaxFileUID &f_uid =
            *static_cast<KaxFileUID *>(l3);
          show_element(l3, 3, "File UID: %u", uint32(f_uid));

        } else if (!is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle2(block_group,
            KaxCluster *&cluster) {
  EbmlMaster *m2, *m3, *m4;
  int i2, i3, i4, i;
  vector<int> frame_sizes;
  vector<uint32_t> frame_adlers;
  bool fref_found, bref_found;
  uint32_t lf_tnum;
  uint64_t lf_timecode;
  float bduration;

  show_element(l2, 2, "Block group");

  bref_found = false;
  fref_found = false;

  lf_timecode = 0;
  lf_tnum = 0;

  bduration = -1.0;

  m2 = static_cast<EbmlMaster *>(l2);
  for (i2 = 0; i2 < m2->ListSize(); i2++) {
    l3 = (*m2)[i2];

    if (is_id(l3, KaxBlock)) {
      char adler[100];
      KaxBlock &block = *static_cast<KaxBlock *>(l3);
      block.SetParent(*cluster);
      show_element(l3, 3, "Block (track number %u, %d frame(s), "
                   "timecode %.3fs = " FMT_TIMECODEN ")",
                   block.TrackNum(), block.NumberFrames(),
                   (float)block.GlobalTimecode() / 1000000000.0,
                   ARG_TIMECODEN(block.GlobalTimecode()));
      lf_timecode = block.GlobalTimecode() / 1000000;
      lf_tnum = block.TrackNum();
      bduration = -1.0;
      for (i = 0; i < (int)block.NumberFrames(); i++) {
        DataBuffer &data = block.GetBuffer(i);
        if (calc_checksums)
          mxprints(adler, " (adler: 0x%08x)",
                   calc_adler32(data.Buffer(), data.Size()));
        else
          adler[0] = 0;
        show_element(NULL, 4, "Frame with size %u%s", data.Size(),
                     adler);
        frame_sizes.push_back(data.Size());
        frame_adlers.push_back(calc_adler32(data.Buffer(),
                                            data.Size()));
      }

    } else if (is_id(l3, KaxBlockDuration)) {
      KaxBlockDuration &duration =
        *static_cast<KaxBlockDuration *>(l3);
      bduration = ((float)uint64(duration)) * tc_scale / 1000000.0;
      show_element(l3, 3, "Block duration: %.3fms", bduration);

    } else if (is_id(l3, KaxReferenceBlock)) {
      KaxReferenceBlock &reference =
        *static_cast<KaxReferenceBlock *>(l3);
      show_element(l3, 3, "Reference block: %.3fms", 
                   ((float)int64(reference)) * tc_scale / 1000000.0);
      if (int64(reference) <= 0)
        bref_found = true;
      else if (int64(reference) > 0)
        fref_found = true;

    } else if (is_id(l3, KaxReferencePriority)) {
      KaxReferencePriority &priority =
        *static_cast<KaxReferencePriority *>(l3);
      show_element(l3, 3, "Reference priority: %u",
                   uint32(priority));
                
#if MATROSKA_VERSION >= 2
    } else if (is_id(l3, KaxBlockVirtual)) {
      string strc;

      KaxBlockVirtual &bvirt = *static_cast<KaxBlockVirtual *>(l3);
      strc = format_binary(bvirt);
      show_element(l3, 3, "Block virtual: %s", strc.c_str());

    } else if (is_id(l3, KaxReferenceVirtual)) {
      KaxReferenceVirtual &ref_virt =
        *static_cast<KaxReferenceVirtual *>(l3);
      show_element(l3, 3, "Reference virtual: %lld",
                   int64(ref_virt));

    } else if (is_id(l3, KaxBlockAdditions)) {
      show_element(l3, 3, "Additions");

      m3 = static_cast<EbmlMaster *>(l3);
      for (i3 = 0; i3 < m3->ListSize(); i3++) {
        l4 = (*m3)[i3];

        if (is_id(l4, KaxBlockMore)) {
          show_element(l4, 4, "More");

          m4 = static_cast<EbmlMaster *>(l4);
          for (i4 = 0; i4 < m4->ListSize(); i4++) {
            l5 = (*m4)[i4];

            if (is_id(l5, KaxBlockAddID)) {
              KaxBlockAddID &add_id =
                *static_cast<KaxBlockAddID *>(l5);
              show_element(l5, 5, "AdditionalID: %llu",
                           uint64(add_id));

            } else if (is_id(l5, KaxBlockAdditional)) {
              string strc;

              KaxBlockAdditional &block =
                *static_cast<KaxBlockAdditional *>(l5);
              strc = format_binary(block);
              show_element(l5, 5, "Block additional: %s",
                           strc.c_str());
                        
            } else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

          } // while (l5 != NULL)

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

      } // while (l4 != NULL)
#endif // MATROSKA_VERSION >= 2

    } else if (is_id(l3, KaxSlices)) {
      show_element(l3, 3, "Slices");

      m3 = static_cast<EbmlMaster *>(l3);
      for (i3 = 0; i3 < m3->ListSize(); i3++) {
        l4 = (*m3)[i3];

        if (is_id(l4, KaxTimeSlice)) {
          show_element(l4, 4, "Time slice");

          m4 = static_cast<EbmlMaster *>(l4);
          for (i4 = 0; i4 < m4->ListSize(); i4++) {
            l5 = (*m4)[i4];

            if (is_id(l5, KaxSliceLaceNumber)) {
              KaxSliceLaceNumber &slace_number =
                *static_cast<KaxSliceLaceNumber *>(l5);
              show_element(l5, 5, "Lace number: %u",
                           uint32(slace_number));

            } else if (is_id(l5, KaxSliceFrameNumber)) {
              KaxSliceFrameNumber &sframe_number =
                *static_cast<KaxSliceFrameNumber *>(l5);
              show_element(l5, 5, "Frame number: %u",
                           uint32(sframe_number));

            } else if (is_id(l5, KaxSliceDelay)) {
              KaxSliceDelay &sdelay =
                *static_cast<KaxSliceDelay *>(l5);
              show_element(l5, 5, "Delay: %.3fms",
                           ((float)uint64(sdelay)) * tc_scale /
                           1000000.0);

            } else if (is_id(l5, KaxSliceDuration)) {
              KaxSliceDuration &sduration =
                *static_cast<KaxSliceDuration *>(l5);
              show_element(l5, 5, "Duration: %.3fms",
                           ((float)uint64(sduration)) * tc_scale /
                           1000000.0);

            } else if (is_id(l5, KaxSliceBlockAddID)) {
              KaxSliceBlockAddID &sbaid =
                *static_cast<KaxSliceBlockAddID *>(l5);
              show_element(l5, 5, "Block additional ID: %u",
                           uint64(sbaid));

            } else if (!is_global(es, l5, 5))
              show_unknown_element(l5, 5);

          } // while (l5 != NULL)

        } else if (!is_global(es, l4, 4))
          show_unknown_element(l4, 4);

      } // while (l4 != NULL)

    } else if (!is_global(es, l3, 3))
      show_unknown_element(l3, 3);

  } // while (l3 != NULL)

  if (show_summary) {
    int fidx;

    for (fidx = 0; fidx < frame_sizes.size(); fidx++) {
      if (bduration != -1.0)
        mxinfo(_("%c frame, track %u, timecode %lld, duration %.3f, size %d, "
                 "adler 0x%08x\n"), bref_found && fref_found ? 'B' :
               bref_found ? 'P' : !fref_found ? 'I' : '?',
               lf_tnum, lf_timecode, bduration, frame_sizes[fidx],
               frame_adlers[fidx]);
      else
        mxinfo(_("%c frame, track %u, timecode %lld, size %d, adler "
                 "0x%08x\n"), bref_found && fref_found ? 'B' :
               bref_found ? 'P' : !fref_found ? 'I' : '?',
               lf_tnum, lf_timecode, frame_sizes[fidx],
               frame_adlers[fidx]);
    }
  } else if (verbose > 2)
    show_element(NULL, 2, _("[%c frame for track %u, timecode %lld]"),
                 bref_found && fref_found ? 'B' :
                 bref_found ? 'P' : !fref_found ? 'I' : '?',
                 lf_tnum, lf_timecode);
}

void
def_handle2(cluster,
            KaxCluster *&cluster,
            int64_t file_size) {
  EbmlMaster *m1;
  int i1;

  cluster = (KaxCluster *)l1;

#ifdef HAVE_WXWINDOWS
  if (use_gui)
    frame->show_progress(100 * cluster->GetElementPosition() /
                         file_size, wxT("Parsing file"));
#endif // HAVE_WXWINDOWS

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxClusterTimecode)) {
      int64_t cluster_tc;
      KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
      cluster_tc = uint64(ctc);
      show_element(l2, 2, "Cluster timecode: %.3fs", 
                   (float)cluster_tc * (float)tc_scale / 1000000000.0);
      cluster->InitTimecode(cluster_tc, tc_scale);

#if MATROSKA_VERSION >= 2
    } else if (is_id(l2, KaxClusterPosition)) {
      KaxClusterPosition &c_pos =
        *static_cast<KaxClusterPosition *>(l2);
      show_element(l2, 2, "Cluster position: %llu", uint64(c_pos));
#endif

    } else if (is_id(l2, KaxClusterPrevSize)) {
      KaxClusterPrevSize &c_psize =
        *static_cast<KaxClusterPrevSize *>(l2);
      show_element(l2, 2, "Cluster previous size: %llu",
                   uint64(c_psize));

    } else if (is_id(l2, KaxBlockGroup))
      handle2(block_group, cluster);

    else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(chapters) {
  EbmlMaster *m1, *m2;
  int i1, i2;

  show_element(l1, 1, "Chapters");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxEditionEntry)) {
      show_element(l2, 2, "Edition entry");

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (!parse_chapter_atom(es, l3, 3) &&
            !is_global(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

void
def_handle(tag_targets) {
  EbmlMaster *m3;
  int i3;

  show_element(l3, 3, "Targets");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagTrackUID)) {
      KaxTagTrackUID &trackuid =
        *static_cast<KaxTagTrackUID *>(l4);
      show_element(l4, 4, "Track UID: %llu", uint64(trackuid));

    } else if (is_id(l4, KaxTagChapterUID)) {
      KaxTagChapterUID &chapteruid =
        *static_cast<KaxTagChapterUID *>(l4);
      show_element(l4, 4, "Chapter UID: %llu",
                   uint64(chapteruid));

    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_general) {
  EbmlMaster *m3;
  int i3;
  string strc;

  show_element(l3, 3, "General");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagSubject)) {
      KaxTagSubject &subject =
        *static_cast<KaxTagSubject *>(l4);
      show_element(l4, 4, "Subject: %s", UTF2STR(subject));

    } else if (is_id(l4, KaxTagBibliography)) {
      KaxTagBibliography &bibliography =
        *static_cast<KaxTagBibliography *>(l4);
      show_element(l4, 4, "Bibliography: %s",
                   UTF2STR(bibliography));

    } else if (is_id(l4, KaxTagLanguage)) {
      KaxTagLanguage &language =
        *static_cast<KaxTagLanguage *>(l4);
      show_element(l4, 4, "Language: %s",
                   string(language).c_str());

    } else if (is_id(l4, KaxTagRating)) {
      KaxTagRating &rating =
        *static_cast<KaxTagRating *>(l4);
      strc = "Rating: " + format_binary(rating);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagEncoder)) {
      KaxTagEncoder &encoder =
        *static_cast<KaxTagEncoder *>(l4);
      show_element(l4, 4, "Encoder: %s", UTF2STR(encoder));

    } else if (is_id(l4, KaxTagEncodeSettings)) {
      KaxTagEncodeSettings &encode_settings =
        *static_cast<KaxTagEncodeSettings *>(l4);
      show_element(l4, 4, "Encode settings: %s",
                   UTF2STR(encode_settings));

    } else if (is_id(l4, KaxTagFile)) {
      KaxTagFile &file =
        *static_cast<KaxTagFile *>(l4);
      show_element(l4, 4, "File: %s", UTF2STR(file));

    } else if (is_id(l4, KaxTagArchivalLocation)) {
      KaxTagArchivalLocation &archival_location =
        *static_cast<KaxTagArchivalLocation *>(l4);
      show_element(l4, 4, "Archival location: %s",
                   UTF2STR(archival_location));

    } else if (is_id(l4, KaxTagKeywords)) {
      KaxTagKeywords &keywords =
        *static_cast<KaxTagKeywords *>(l4);
      show_element(l4, 4, "Keywords: %s", UTF2STR(keywords));

    } else if (is_id(l4, KaxTagMood)) {
      KaxTagMood &mood =
        *static_cast<KaxTagMood *>(l4);
      show_element(l4, 4, "Mood: %s", UTF2STR(mood));

    } else if (is_id(l4, KaxTagRecordLocation)) {
      KaxTagRecordLocation &record_location =
        *static_cast<KaxTagRecordLocation *>(l4);
      show_element(l4, 4, "Record location: %s",
                   string(record_location).c_str());

    } else if (is_id(l4, KaxTagSource)) {
      KaxTagSource &source =
        *static_cast<KaxTagSource *>(l4);
      show_element(l4, 4, "Source: %s", UTF2STR(source));

    } else if (is_id(l4, KaxTagSourceForm)) {
      KaxTagSourceForm &source_form =
        *static_cast<KaxTagSourceForm *>(l4);
      show_element(l4, 4, "Source form: %s",
                   UTF2STR(source_form));

    } else if (is_id(l4, KaxTagProduct)) {
      KaxTagProduct &product =
        *static_cast<KaxTagProduct *>(l4);
      show_element(l4, 4, "Product: %s", UTF2STR(product));

    } else if (is_id(l4, KaxTagOriginalMediaType)) {
      KaxTagOriginalMediaType &original_media_type =
        *static_cast<KaxTagOriginalMediaType *>(l4);
      show_element(l4, 4, "Original media type: %s",
                   UTF2STR(original_media_type));

    } else if (is_id(l4, KaxTagEncoder)) {
      KaxTagEncoder &encoder =
        *static_cast<KaxTagEncoder *>(l4);
      show_element(l4, 4, "Encoder: %s", UTF2STR(encoder));

    } else if (is_id(l4, KaxTagPlayCounter)) {
      KaxTagPlayCounter &play_counter =
        *static_cast<KaxTagPlayCounter *>(l4);
      show_element(l4, 4, "Play counter: %llu", 
                   uint64(play_counter));

    } else if (is_id(l4, KaxTagPopularimeter)) {
      KaxTagPopularimeter &popularimeter =
        *static_cast<KaxTagPopularimeter *>(l4);
      show_element(l4, 4, "Popularimeter: %lld", 
                   int64(popularimeter));

    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_genres) {
  EbmlMaster *m3;
  int i3;
  string strc;

  show_element(l3, 3, "Genres");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagAudioGenre)) {
      KaxTagAudioGenre &audio_genre =
        *static_cast<KaxTagAudioGenre *>(l4);
      show_element(l4, 4, "Audio genre: %s",
                   string(audio_genre).c_str());

    } else if (is_id(l4, KaxTagVideoGenre)) {
      KaxTagVideoGenre &video_genre =
        *static_cast<KaxTagVideoGenre *>(l4);
      strc = "Video genre: " + format_binary(video_genre);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagSubGenre)) {
      KaxTagSubGenre &sub_genre =
        *static_cast<KaxTagSubGenre *>(l4);
      show_element(l4, 4, "Sub genre: %s",
                   string(sub_genre).c_str());

    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_audio_specific) {
  EbmlMaster *m3;
  int i3;
  string strc;

  show_element(l3, 3, "Audio specific");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagAudioEncryption)) {
      KaxTagAudioEncryption &encryption =
        *static_cast<KaxTagAudioEncryption *>(l4);
      strc = "Audio encryption: " + format_binary(encryption);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagAudioGain)) {
      KaxTagAudioGain &audio_gain =
        *static_cast<KaxTagAudioGain *>(l4);
      show_element(l4, 4, "Audio gain: %.3f", float(audio_gain));

    } else if (is_id(l4, KaxTagAudioPeak)) {
      KaxTagAudioPeak &audio_peak =
        *static_cast<KaxTagAudioPeak *>(l4);
      show_element(l4, 4, "Audio peak: %.3f", float(audio_peak));

    } else if (is_id(l4, KaxTagBPM)) {
      KaxTagBPM &bpm = *static_cast<KaxTagBPM *>(l4);
      show_element(l4, 4, "BPM: %.3f", float(bpm));

    } else if (is_id(l4, KaxTagEqualisation)) {
      KaxTagEqualisation &equalisation =
        *static_cast<KaxTagEqualisation *>(l4);
      strc = "Equalisation: " + format_binary(equalisation);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagDiscTrack)) {
      KaxTagDiscTrack &disc_track =
        *static_cast<KaxTagDiscTrack *>(l4);
      show_element(l4, 4, "Disc track: %u", uint32(disc_track));

    } else if (is_id(l4, KaxTagSetPart)) {
      KaxTagSetPart &set_part =
        *static_cast<KaxTagSetPart *>(l4);
      show_element(l4, 4, "Set part: %u", uint32(set_part));

    } else if (is_id(l4, KaxTagInitialKey)) {
      KaxTagInitialKey &initial_key =
        *static_cast<KaxTagInitialKey *>(l4);
      show_element(l4, 4, "Initial key: %s",
                   string(initial_key).c_str());

    } else if (is_id(l4, KaxTagOfficialAudioFileURL)) {
      KaxTagOfficialAudioFileURL &official_file_url =
        *static_cast<KaxTagOfficialAudioFileURL *>(l4);
      show_element(l4, 4, "Official audio file URL: %s",
                   string(official_file_url).c_str());

    } else if (is_id(l4, KaxTagOfficialAudioSourceURL)) {
      KaxTagOfficialAudioSourceURL &official_source_url =
        *static_cast<KaxTagOfficialAudioSourceURL *>(l4);
      show_element(l4, 4, "Official audio source URL: %s",
                   string(official_source_url).c_str());

    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_image_specific) {
  EbmlMaster *m3;
  int i3;
  string strc;

  show_element(l3, 3, "Image specific");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagCaptureDPI)) {
      KaxTagCaptureDPI &capture_dpi =
        *static_cast<KaxTagCaptureDPI *>(l4);
      show_element(l4, 4, "Capture DPI: %u",
                   uint32(capture_dpi));

    } else if (is_id(l4, KaxTagCaptureLightness)) {
      KaxTagCaptureLightness &capture_lightness =
        *static_cast<KaxTagCaptureLightness *>(l4);
      strc = "Capture lightness: " +
        format_binary(capture_lightness);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagCapturePaletteSetting)) {
      KaxTagCapturePaletteSetting &capture_palette_setting =
        *static_cast<KaxTagCapturePaletteSetting *>(l4);
      show_element(l4, 4, "Capture palette setting: %u",
                   uint32(capture_palette_setting));

    } else if (is_id(l4, KaxTagCaptureSharpness)) {
      KaxTagCaptureSharpness &capture_sharpness =
        *static_cast<KaxTagCaptureSharpness *>(l4);
      strc = "Capture sharpness: " +
        format_binary(capture_sharpness);
      show_element(l4, 4, strc.c_str());

    } else if (is_id(l4, KaxTagCropped)) {
      KaxTagCropped &cropped =
        *static_cast<KaxTagCropped *>(l4);
      show_element(l4, 4, "Cropped: %s", UTF2STR(cropped));

    } else if (is_id(l4, KaxTagOriginalDimensions)) {
      KaxTagOriginalDimensions &original_dimensions =
        *static_cast<KaxTagOriginalDimensions *>(l4);
      show_element(l4, 4, "Original dimensions: %s",
                   string(original_dimensions).c_str());

    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_multi_commercial) {
  EbmlMaster *m3, *m4, *m5;
  int i3, i4, i5;

  show_element(l3, 3, "Multi commercial");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagCommercial)) {
      show_element(l4, 4, "Commercial");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiCommercialType)) {
          int type;

          KaxTagMultiCommercialType &c_type =
            *static_cast<KaxTagMultiCommercialType *>(l5);
          type = uint32(c_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_COMMERCIAL_TYPES) ?
                       commercial_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiCommercialAddress)) {
          KaxTagMultiCommercialAddress &c_address =
            *static_cast<KaxTagMultiCommercialAddress *>(l5);
          show_element(l5, 5, "Address: %s", UTF2STR(c_address));

        } else if (is_id(l5, KaxTagMultiCommercialURL)) {
          KaxTagMultiCommercialURL &c_url =
            *static_cast<KaxTagMultiCommercialURL *>(l5);
          show_element(l5, 5, "URL: %s",
                       string(c_url).c_str());

        } else if (is_id(l5, KaxTagMultiCommercialEmail)) {
          KaxTagMultiCommercialEmail &c_email =
            *static_cast<KaxTagMultiCommercialEmail *>(l5);
          show_element(l5, 5, "Email: %s",
                       string(c_email).c_str());

        } else if (is_id(l5, KaxTagMultiPrice)) {
          show_element(l5, 5, "Multi price");

          m5 = static_cast<EbmlMaster *>(l5);
          for (i5 = 0; i5 < m5->ListSize(); i5++) {
            l6 = (*m5)[i5];

            if (is_id(l6, KaxTagMultiPriceCurrency)) {
              KaxTagMultiPriceCurrency &p_currency =
                *static_cast<KaxTagMultiPriceCurrency *>(l6);
              show_element(l6, 6, "Currency: %s",
                           string(p_currency).c_str());

            } else if (is_id(l6, KaxTagMultiPriceAmount)) {
              KaxTagMultiPriceAmount &p_amount =
                *static_cast<KaxTagMultiPriceAmount *>(l6);
              show_element(l6, 6, "Amount: %.3f",
                           float(p_amount));

            } else if (is_id(l6, KaxTagMultiPricePriceDate)) {
              struct tm tmutc;
              time_t temptime;
              char buffer[40];
              KaxTagMultiPricePriceDate &p_pdate =
                *static_cast<KaxTagMultiPricePriceDate *>(l6);

              temptime = p_pdate.GetEpochDate();
              if ((gmtime_r(&temptime, &tmutc) != NULL) &&
                  (asctime_r(&tmutc, buffer) != NULL)) {
                buffer[strlen(buffer) - 1] = 0;
                show_element(l6, 6, "Price date: %s UTC",
                             buffer);
              } else
                show_element(l6, 6, "Price date (invalid, "
                             "value: %d)", temptime);

            } else if (!is_global(es, l6, 6) &&
                       !parse_multicomment(es, l6, 6))
              show_unknown_element(l6, 6);

          } // while (l6 != NULL)

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_multi_date) {
  EbmlMaster *m3, *m4;
  int i3, i4;

  show_element(l3, 3, "Multi date");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagDate)) {
      show_element(l4, 4, "Date");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiDateType)) {
          int type;
          KaxTagMultiDateType &d_type =
            *static_cast<KaxTagMultiDateType *>(l5);
          type = uint32(d_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_DATE_TYPES) ?
                       date_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiDateDateBegin)) {
          struct tm tmutc;
          time_t temptime;
          char buffer[40];
          KaxTagMultiDateDateBegin &d_begin =
            *static_cast<KaxTagMultiDateDateBegin *>(l5);

          temptime = d_begin.GetEpochDate();
          if ((gmtime_r(&temptime, &tmutc) != NULL) &&
              (asctime_r(&tmutc, buffer) != NULL)) {
            buffer[strlen(buffer) - 1] = 0;
            show_element(l5, 5, "Begin: %s UTC", buffer);
          } else
            show_element(l5, 5, "Begin (invalid, value: %d)",
                         temptime);

        } else if (is_id(l5, KaxTagMultiDateDateEnd)) {
          struct tm tmutc;
          time_t temptime;
          char buffer[40];
          KaxTagMultiDateDateEnd &d_end =
            *static_cast<KaxTagMultiDateDateEnd *>(l5);

          temptime = d_end.GetEpochDate();
          if ((gmtime_r(&temptime, &tmutc) != NULL) &&
              (asctime_r(&tmutc, buffer) != NULL)) {
            buffer[strlen(buffer) - 1] = 0;
            show_element(l5, 5, "End: %s UTC", buffer);
          } else
            show_element(l5, 5, "End (invalid, value: %d)",
                         temptime);

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)

}

void
def_handle(tag_multi_entity) {
  EbmlMaster *m3, *m4;
  int i3, i4;

  show_element(l3, 3, "Multi entity");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagEntity)) {
      show_element(l4, 4, "Entity");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiEntityType)) {
          int type;
          KaxTagMultiEntityType &e_type =
            *static_cast<KaxTagMultiEntityType *>(l5);
          type = uint32(e_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_ENTITY_TYPES) ?
                       entity_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiEntityName)) {
          KaxTagMultiEntityName &e_name =
            *static_cast<KaxTagMultiEntityName *>(l5);
          show_element(l5, 5, "Name: %s", UTF2STR(e_name));

        } else if (is_id(l5, KaxTagMultiEntityAddress)) {
          KaxTagMultiEntityAddress &e_address =
            *static_cast<KaxTagMultiEntityAddress *>(l5);
          show_element(l5, 5, "Address: %s", UTF2STR(e_address));

        } else if (is_id(l5, KaxTagMultiEntityURL)) {
          KaxTagMultiEntityURL &e_URL =
            *static_cast<KaxTagMultiEntityURL *>(l5);
          show_element(l5, 5, "URL: %s", string(e_URL).c_str());

        } else if (is_id(l5, KaxTagMultiEntityEmail)) {
          KaxTagMultiEntityEmail &e_email =
            *static_cast<KaxTagMultiEntityEmail *>(l5);
          show_element(l5, 5, "Email: %s",
                       string(e_email).c_str());

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_multi_identifier) {
  EbmlMaster *m3, *m4;
  int i3, i4;
  string strc;

  show_element(l3, 3, "Multi identifier");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagIdentifier)) {
      show_element(l4, 4, "Identifier");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiIdentifierType)) {
          int type;
          KaxTagMultiIdentifierType &i_type =
            *static_cast<KaxTagMultiIdentifierType *>(l5);
          type = uint32(i_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_IDENTIFIER_TYPES) ?
                       identifier_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiIdentifierBinary)) {
          KaxTagMultiIdentifierBinary &i_binary =
            *static_cast<KaxTagMultiIdentifierBinary *>(l5);
          strc = "Binary: " + format_binary(i_binary);
          show_element(l5, 5, strc.c_str());

        } else if (is_id(l5, KaxTagMultiIdentifierString)) {
          KaxTagMultiIdentifierString &i_string =
            *static_cast<KaxTagMultiIdentifierString *>(l5);
          show_element(l5, 5, "String: %s", UTF2STR(i_string));

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_multi_legal) {
  EbmlMaster *m3, *m4;
  int i3, i4;
  string strc;

  show_element(l3, 3, "Multi legal");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagLegal)) {
      show_element(l4, 4, "Legal");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiLegalType)) {
          int type;
          KaxTagMultiLegalType &l_type =
            *static_cast<KaxTagMultiLegalType *>(l5);
          type = uint32(l_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_LEGAL_TYPES) ?
                       legal_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiLegalAddress)) {
          KaxTagMultiLegalAddress &l_address =
            *static_cast<KaxTagMultiLegalAddress *>(l5);
          show_element(l5, 5, "Address: %s", UTF2STR(l_address));

        } else if (is_id(l5, KaxTagMultiLegalURL)) {
          KaxTagMultiLegalURL &l_URL =
            *static_cast<KaxTagMultiLegalURL *>(l5);
          show_element(l5, 5, "URL: %s", string(l_URL).c_str());

        } else if (is_id(l5, KaxTagMultiLegalContent)) {
          KaxTagMultiLegalContent &l_content =
            *static_cast<KaxTagMultiLegalContent *>(l5);
          show_element(l5, 5, "Content: %s", UTF2STR(l_content));

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tag_multi_title) {
  EbmlMaster *m3, *m4;
  int i3, i4;
  string strc;

  show_element(l3, 3, "Multi title");

  m3 = static_cast<EbmlMaster *>(l3);
  for (i3 = 0; i3 < m3->ListSize(); i3++) {
    l4 = (*m3)[i3];

    if (is_id(l4, KaxTagTitle)) {
      show_element(l4, 4, "Title");

      m4 = static_cast<EbmlMaster *>(l4);
      for (i4 = 0; i4 < m4->ListSize(); i4++) {
        l5 = (*m4)[i4];

        if (is_id(l5, KaxTagMultiTitleType)) {
          int type;
          KaxTagMultiTitleType &t_type =
            *static_cast<KaxTagMultiTitleType *>(l5);
          type = uint32(t_type);
          show_element(l5, 5, "Type: %u (%s)", type,
                       (type >= 1) &&
                       (type <= NUM_TITLE_TYPES) ?
                       title_types[type - 1] : "unknown");

        } else if (is_id(l5, KaxTagMultiTitleName)) {
          KaxTagMultiTitleName &t_name =
            *static_cast<KaxTagMultiTitleName *>(l5);
          show_element(l5, 5, "Name: %s", UTF2STR(t_name));

        } else if (is_id(l5, KaxTagMultiTitleSubTitle)) {
          KaxTagMultiTitleSubTitle &t_sub_title =
            *static_cast<KaxTagMultiTitleSubTitle *>(l5);
          show_element(l5, 5, "Sub title: %s",
                       UTF2STR(t_sub_title));

        } else if (is_id(l5, KaxTagMultiTitleEdition)) {
          KaxTagMultiTitleEdition &t_edition =
            *static_cast<KaxTagMultiTitleEdition *>(l5);
          show_element(l5, 5, "Edition: %s", UTF2STR(t_edition));

        } else if (is_id(l5, KaxTagMultiTitleAddress)) {
          KaxTagMultiTitleAddress &t_address =
            *static_cast<KaxTagMultiTitleAddress *>(l5);
          show_element(l5, 5, "Address: %s", UTF2STR(t_address));

        } else if (is_id(l5, KaxTagMultiTitleURL)) {
          KaxTagMultiTitleURL &t_URL =
            *static_cast<KaxTagMultiTitleURL *>(l5);
          show_element(l5, 5, "URL: %s", string(t_URL).c_str());

        } else if (is_id(l5, KaxTagMultiTitleEmail)) {
          KaxTagMultiTitleEmail &t_email =
            *static_cast<KaxTagMultiTitleEmail *>(l5);
          show_element(l5, 5, "Email: %s",
                       string(t_email).c_str());

        } else if (is_id(l5, KaxTagMultiTitleLanguage)) {
          KaxTagMultiTitleLanguage &t_language =
            *static_cast<KaxTagMultiTitleLanguage *>(l5);
          show_element(l5, 5, "Language: %s",
                       string(t_language).c_str());

        } else if (!is_global(es, l5, 5) &&
                   !parse_multicomment(es, l5, 5))
          show_unknown_element(l5, 5);

      } // while (l5 != NULL)
                    
    } else if (!is_global(es, l4, 4) &&
               !parse_multicomment(es, l4, 4))
      show_unknown_element(l4, 4);

  } // while (l4 != NULL)
}

void
def_handle(tags) {
  EbmlMaster *m1, *m2;
  int i1, i2;

  show_element(l1, 1, "Tags");

  upper_lvl_el = 0;
  m1 = static_cast<EbmlMaster *>(l1);
  read_master(m1, es, l1->Generic().Context, upper_lvl_el, l3);

  for (i1 = 0; i1 < m1->ListSize(); i1++) {
    l2 = (*m1)[i1];

    if (is_id(l2, KaxTag)) {
      show_element(l2, 2, "Tag");

      m2 = static_cast<EbmlMaster *>(l2);
      for (i2 = 0; i2 < m2->ListSize(); i2++) {
        l3 = (*m2)[i2];

        if (is_id(l3, KaxTagTargets))
          handle(tag_targets);

        else if (is_id(l3, KaxTagGeneral))
          handle(tag_general);

        else if (is_id(l3, KaxTagGenres))
          handle(tag_genres);

        else if (is_id(l3, KaxTagAudioSpecific))
          handle(tag_audio_specific);

        else if (is_id(l3, KaxTagImageSpecific))
          handle(tag_image_specific);

        else if (is_id(l3, KaxTagMultiCommercial))
          handle(tag_multi_commercial);

        else if (is_id(l3, KaxTagMultiDate))
          handle(tag_multi_date);

        else if (is_id(l3, KaxTagMultiEntity))
          handle(tag_multi_entity);

        else if (is_id(l3, KaxTagMultiIdentifier))
          handle(tag_multi_identifier);

        else if (is_id(l3, KaxTagMultiLegal))
          handle(tag_multi_legal);

        else if (is_id(l3, KaxTagMultiTitle))
          handle(tag_multi_title);

        else if (!is_global(es, l3, 3) &&
                 !parse_multicomment(es, l3, 3) &&
                 !parse_simpletag(es, l3, 3))
          show_unknown_element(l3, 3);

      } // while (l3 != NULL)

    } else if (!is_global(es, l2, 2))
      show_unknown_element(l2, 2);

  } // while (l2 != NULL)
}

bool
process_file(const char *file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlElement *l5 = NULL, *l6 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  uint64_t file_size;
  string strc;
  mm_io_c *in;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error("Error: Couldn't open input file %s (%s).\n", file_name,
               strerror(errno));
    return false;
  }

  in->setFilePointer(0, seek_end);
  file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error("No EBML head found.");
      delete es;

      return false;
    }
    show_element(l0, 0, "EBML head");
      
    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error("No segment/level 0 element found.");
        return false;
      }
      if (is_id(l0, KaxSegment)) {
        show_element(l0, 0, "Segment, size %lld", l0->GetSize() -
                     l0->HeadSize());
        break;
      }

      show_element(l0, 0, "Next level 0 element is not a segment but %s",
                   typeid(*l0).name());

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {
      if (is_id(l1, KaxInfo))
        handle(info);

      else if (is_id(l1, KaxTracks))
        handle(tracks);

      else if (is_id(l1, KaxSeekHead))
        handle(seek_head);

      else if (is_id(l1, KaxCluster)) {
        show_element(l1, 1, "Cluster");
        if ((verbose == 0) && !show_summary) {
          delete l1;
          delete l0;
          delete es;
          delete in;

          return true;
        }
        handle2(cluster, cluster, file_size);

      } else if (is_id(l1, KaxCues))
        handle(cues);

        // Weee! Attachments!
      else if (is_id(l1, KaxAttachments))
        handle(attachments);

      else if (is_id(l1, KaxChapters))
        handle(chapters);

        // Let's handle some TAGS.
      else if (is_id(l1, KaxTags))
        handle(tags);

      else if (!is_global(es, l1, 1))
        show_unknown_element(l1, 1);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    return true;
  } catch (std::exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;

    return false;
  }
}

int
console_main(int argc,
             char **argv) {
  char *file_name;

#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  nice(2);
#endif

  parse_args(argc, argv, file_name);
  if (file_name == NULL) {
    usage();
    mxexit(0);
  }
  if (process_file(file_name))
    return 0;
  else
    return 1;
}

void
setup() {
#if defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("Could not set the locale properly. Check the "
            "LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
#endif

  cc_local_utf8 = utf8_init(NULL);
}

void
cleanup() {
  utf8_done();
}

#if !defined HAVE_WXWINDOWS
int
main(int argc,
     char **argv) {
  char *initial_file;
  int res;

  setup();
  parse_args(argc, argv, initial_file);

  res = console_main(argc, argv);
  cleanup();

  return res;
}

#elif defined(SYS_UNIX) || defined(SYS_APPLE)

int
main(int argc,
     char **argv) {
  char *initial_file;
  int res;

  setup();
  parse_args(argc, argv, initial_file);

  if (use_gui) {
    wxEntry(argc, argv);
    return 0;
  } else
    res = console_main(argc, argv);

  cleanup();

  return res;
}

#endif // HAVE_WXWINDOWS
