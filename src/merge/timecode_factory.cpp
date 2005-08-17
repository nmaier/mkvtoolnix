/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   the timecode factory

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modifications by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include <map>

#include "common.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "timecode_factory.h"

using namespace std;

timecode_factory_c *
timecode_factory_c::create(const string &file_name,
                           const string &source_name,
                           int64_t tid) {
  mm_io_c *in;
  string line;
  int version;
  timecode_factory_c *factory;

  if (file_name == "")
    return NULL;

  in = NULL;                    // avoid gcc warning
  try {
    in = new mm_text_io_c(new mm_file_io_c(file_name));
  } catch(...) {
    mxerror(_("The timecode file '%s' could not be opened for reading.\n"),
            file_name.c_str());
  }

  if (!in->getline2(line) || !starts_with_case(line, "# timecode format v") ||
      !parse_int(&line[strlen("# timecode format v")], version))
    mxerror(_("The timecode file '%s' contains an unsupported/unrecognized "
              "format line. The very first line must look like "
              "'# timecode format v1'.\n"), file_name.c_str());
  factory = NULL;               // avoid gcc warning
  if (version == 1)
    factory = new timecode_factory_v1_c(file_name, source_name, tid);
  else if ((2 == version) || (4 == version))
    factory = new timecode_factory_v2_c(file_name, source_name, tid,
                                        version);
  else if (version == 3)
    factory = new timecode_factory_v3_c(file_name, source_name, tid);
  else
    mxerror(_("The timecode file '%s' contains an unsupported/unrecognized "
              "format (version %d).\n"), file_name.c_str(), version);

  factory->parse(*in);
  delete in;

  return factory;
}

void
timecode_factory_v1_c::parse(mm_io_c &in) {
  string line;
  timecode_range_c t;
  vector<string> fields;
  vector<timecode_range_c>::iterator iit;
  vector<timecode_range_c>::const_iterator pit;
  uint32_t i, line_no;
  bool done;

  line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line"
                " with the default number of frames per second.\n"),
              m_file_name.c_str());
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);

  if (!starts_with_case(line, "assume "))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            m_file_name.c_str());
  line.erase(0, 6);
  strip(line);
  if (!parse_double(line.c_str(), m_default_fps))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            m_file_name.c_str());

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    if (mxsscanf(line, "%lld,%lld,%lf", &t.start_frame, &t.end_frame, &t.fps)
        != 3) {
      mxwarn(_("Line %d of the timecode file '%s' could not be parsed.\n"),
             line_no, m_file_name.c_str());
      continue;
    }

    if ((t.fps <= 0) || (t.start_frame < 0) || (t.end_frame < 0) ||
        (t.end_frame < t.start_frame)) {
      mxwarn(_("Line %d of the timecode file '%s' contains inconsistent data "
               "(e.g. the start frame number is bigger than the end frame "
               "number, or some values are smaller than zero).\n"),
             line_no, m_file_name.c_str());
      continue;
    }

    m_ranges.push_back(t);
  }

  mxverb(3, "ext_timecodes: Version 1, default fps %f, %u entries.\n",
         m_default_fps, (unsigned int)m_ranges.size());

  if (m_ranges.size() == 0)
    t.start_frame = 0;
  else {
    sort(m_ranges.begin(), m_ranges.end());
    do {
      done = true;
      iit = m_ranges.begin();
      for (i = 0; i < (m_ranges.size() - 1); i++) {
        iit++;
        if (m_ranges[i].end_frame < (m_ranges[i + 1].start_frame - 1)) {
          t.start_frame = m_ranges[i].end_frame + 1;
          t.end_frame = m_ranges[i + 1].start_frame - 1;
          t.fps = m_default_fps;
          m_ranges.insert(iit, t);
          done = false;
          break;
        }
      }
    } while (!done);
    if (m_ranges[0].start_frame != 0) {
      t.start_frame = 0;
      t.end_frame = m_ranges[0].start_frame - 1;
      t.fps = m_default_fps;
      m_ranges.insert(m_ranges.begin(), t);
    }
    t.start_frame = m_ranges[m_ranges.size() - 1].end_frame + 1;
  }
  t.end_frame = 0xfffffffffffffffll;
  t.fps = m_default_fps;
  m_ranges.push_back(t);

  m_ranges[0].base_timecode = 0.0;
  pit = m_ranges.begin();
  for (iit = m_ranges.begin() + 1; iit < m_ranges.end(); iit++, pit++)
    iit->base_timecode = pit->base_timecode +
      ((double)pit->end_frame - (double)pit->start_frame + 1) * 1000000000.0 /
      pit->fps;

  for (iit = m_ranges.begin(); iit < m_ranges.end(); iit++)
    mxverb(3, "ranges: entry %lld -> %lld at %f with %f\n",
           iit->start_frame, iit->end_frame, iit->fps, iit->base_timecode);
}

bool
timecode_factory_v1_c::get_next(packet_cptr &packet) {
  packet->assigned_timecode = get_at(m_frameno);
  packet->duration = get_at(m_frameno + 1) - packet->assigned_timecode;
  m_frameno++;
  if ((m_frameno > m_ranges[m_current_range].end_frame) &&
      (m_current_range < (m_ranges.size() - 1)))
    m_current_range++;

  mxverb(4, "ext_timecodes v1: tc %lld dur %lld for %lld\n",
         packet->assigned_timecode, packet->duration, m_frameno - 1);
  return false;
}

int64_t
timecode_factory_v1_c::get_at(int64_t frame) {
  timecode_range_c *t;

  t = &m_ranges[m_current_range];
  if ((frame > t->end_frame) && (m_current_range < (m_ranges.size() - 1)))
    t = &m_ranges[m_current_range + 1];
  return (int64_t)(t->base_timecode + 1000000000.0 *
                   (frame - t->start_frame) / t->fps);
}

void
timecode_factory_v2_c::parse(mm_io_c &in) {
  int line_no;
  string line;
  double timecode, previous_timecode;
  map<int64_t, int64_t> dur_map;
  map<int64_t, int64_t>::const_iterator it;
  int64_t duration, dur_sum;

  dur_sum = 0;
  line_no = 0;
  previous_timecode = 0;
  while (in.getline2(line)) {
    line_no++;
    strip(line);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;
    if (!parse_double(line.c_str(), timecode))
      mxerror(_("The line %d of the timecode file '%s' does not contain a "
                "valid floating point number.\n"), line_no,
              m_file_name.c_str());
    if ((2 == m_version) && (timecode < previous_timecode))
      mxerror("The timecode v2 file '%s' contains timecodes that are not "
              "ordered. Due to a bug in mkvmerge versions up to and including "
              "v1.5.0 this was necessary if the track to which the timecode "
              "file was applied contained B frames. Starting with v1.5.1 "
              "mkvmerge now handles this correctly, and the timecodes in the "
              "timecode file must be ordered normally. For example, the frame "
              "sequence 'IPBBP...' at 25 FPS requires a timecode file with "
              "the first timecodes being '0', '40', '80', '120' etc and not "
              "'0', '120', '40', '80' etc.\n\nIf you really have to specify "
              "non-sorted timecodes then use the timecode format v4. It is "
              "identical to format v2 but allows non-sorted timecodes.\n",
              in.get_file_name().c_str());
    previous_timecode = timecode;
    m_timecodes.push_back((int64_t)(timecode * 1000000));
    if (m_timecodes.size() > 1) {
      duration = m_timecodes[m_timecodes.size() - 1] -
        m_timecodes[m_timecodes.size() - 2];
      if (dur_map.find(duration) == dur_map.end())
        dur_map[duration] = 1;
      else
        dur_map[duration] = dur_map[duration] + 1;
      dur_sum += duration;
      m_durations.push_back(duration);
    }
  }
  if (m_timecodes.size() == 0)
    mxerror(_("The timecode file '%s' does not contain any valid entry.\n"),
            m_file_name.c_str());

  dur_sum = -1;
  foreach(it, dur_map) {
    if ((dur_sum < 0) || (dur_map[dur_sum] < (*it).second))
      dur_sum = (*it).first;
    mxverb(4, "ext_m_timecodes v2 dur_map %lld = %lld\n", (*it).first,
           (*it).second);
  }
  mxverb(4, "ext_m_timecodes v2 max is %lld = %lld\n", dur_sum,
         dur_map[dur_sum]);
  if (dur_sum > 0)
    m_default_fps = (double)1000000000.0 / dur_sum;
  m_durations.push_back(dur_sum);
}

bool
timecode_factory_v2_c::get_next(packet_cptr &packet) {
  if ((m_frameno >= m_timecodes.size()) && !m_warning_printed) {
    mxwarn(FMT_TID "The number of external timecodes %u is "
           "smaller than the number of frames in this track. "
           "The remaining frames of this track might not be timestamped "
           "the way you intended them to be. mkvmerge might even crash.\n",
           m_source_name.c_str(), m_tid, (unsigned int)m_timecodes.size());
    m_warning_printed = true;
    if (m_timecodes.empty()) {
      packet->assigned_timecode = 0;
      packet->duration = 0;
    } else {
      packet->assigned_timecode = m_timecodes.back();
      packet->duration = m_timecodes.back();
    }
    return false;
  }

  packet->assigned_timecode = m_timecodes[m_frameno];
  packet->duration = m_durations[m_frameno];
  m_frameno++;

  return false;
}

void
timecode_factory_v3_c::parse(mm_io_c &in) {
  string line;
  timecode_duration_c t;
  vector<string> fields;
  vector<timecode_duration_c>::iterator iit;
  vector<timecode_duration_c>::const_iterator pit;
  uint32_t line_no;
  double dur;

  line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line"
                " with the default number of frames per second.\n"),
              m_file_name.c_str());
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);

  if (!starts_with_case(line, "assume "))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            m_file_name.c_str());
  line.erase(0, 6);
  strip(line);

  if (!parse_double(line.c_str(), m_default_fps))
    mxerror(_("The timecode file '%s' does not contain a valid 'Assume' line "
              "with the default number of frames per second.\n"),
            m_file_name.c_str());

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    if (starts_with_case(line, "gap,")) {
      line.erase(0, 4);
      strip(line);
      t.is_gap = true;
      t.fps = m_default_fps;
      if (!parse_double(line.c_str(), dur))
        mxerror(_("The timecode file '%s' does not contain a valid 'Gap' line "
                  "with the duration of the gap.\n"),
                m_file_name.c_str());
      t.duration = (int64_t)(1000000000.0 * dur);

    } else {
      int res;

      t.is_gap = false;
      res = mxsscanf(line, "%lf,%lf", &dur, &t.fps);
      if (res == 1) {
        t.fps = m_default_fps;
      } else if (res != 2) {
        mxwarn(_("Line %d of the timecode file '%s' could not be parsed.\n"),
               line_no, m_file_name.c_str());
        continue;
      }
      t.duration = (int64_t)(1000000000.0 * dur);
    }

    if ((t.fps < 0) || (t.duration <= 0)) {
      mxwarn(_("Line %d of the timecode file '%s' contains inconsistent data "
               "(e.g. the duration or the FPS are smaller than zero).\n"),
             line_no, m_file_name.c_str());
      continue;
    }

    m_durations.push_back(t);
  }

  mxverb(3, "ext_timecodes: Version 3, default fps %f, %u entries.\n",
         m_default_fps, (unsigned int)m_durations.size());

  if (m_durations.size() == 0) {
    mxwarn(_("The timecode file '%s' does not contain any valid entry.\n"),
           m_file_name.c_str());
  }
  t.duration = 0xfffffffffffffffll;
  t.is_gap = false;
  t.fps = m_default_fps;
  m_durations.push_back(t);
  for (iit = m_durations.begin(); iit < m_durations.end(); iit++)
    mxverb(4, "durations:%s entry for %lld with %f FPS\n",
           iit->is_gap ? " gap" : "", iit->duration, iit->fps);
}

bool
timecode_factory_v3_c::get_next(packet_cptr &packet) {
  bool result = false;

  if (m_durations[m_current_duration].is_gap) {
    // find the next non-gap
    size_t duration_index = m_current_duration;
    while (m_durations[duration_index].is_gap ||
           (0 == m_durations[duration_index].duration)) {
      m_current_offset += m_durations[duration_index].duration;
      duration_index++;
    }
    m_current_duration = duration_index;
    // yes, there is a gap before this frame
    result = true;
  }

  packet->assigned_timecode = m_current_offset + m_current_timecode;
  // If default_fps is 0 then the duration is unchanged, usefull for audio.
  if (m_durations[m_current_duration].fps) {
    packet->duration =
      (int64_t)(1000000000.0 / m_durations[m_current_duration].fps);
  }
  packet->duration /= packet->time_factor;
  m_current_timecode += packet->duration;
  if (m_current_timecode >= m_durations[m_current_duration].duration) {
    m_current_offset += m_durations[m_current_duration].duration;
    m_current_timecode = 0;
    m_current_duration++;
  }

  mxverb(3, "ext_timecodes v3: tc %lld dur %lld\n", packet->assigned_timecode,
         packet->duration);

  return result;
}
