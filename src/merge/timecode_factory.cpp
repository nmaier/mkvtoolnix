/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   the timecode factory

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modifications by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "merge/pr_generic.h"
#include "merge/timecode_factory.h"

timecode_factory_cptr
timecode_factory_c::create(const std::string &file_name,
                           const std::string &source_name,
                           int64_t tid) {
  if (file_name.empty())
    return timecode_factory_cptr(nullptr);

  mm_io_c *in = nullptr;           // avoid gcc warning
  try {
    in = new mm_text_io_c(new mm_file_io_c(file_name));
  } catch(...) {
    mxerror(boost::format(Y("The timecode file '%1%' could not be opened for reading.\n")) % file_name);
  }

  std::string line;
  int version;
  if (!in->getline2(line) || !ba::istarts_with(line, "# timecode format v") || !parse_int(&line[strlen("# timecode format v")], version))
    mxerror(boost::format(Y("The timecode file '%1%' contains an unsupported/unrecognized format line. The very first line must look like '# timecode format v1'.\n"))
            % file_name);

  timecode_factory_c *factory = nullptr; // avoid gcc warning
  if (1 == version)
    factory = new timecode_factory_v1_c(file_name, source_name, tid);

  else if ((2 == version) || (4 == version))
    factory = new timecode_factory_v2_c(file_name, source_name, tid, version);

  else if (3 == version)
    factory = new timecode_factory_v3_c(file_name, source_name, tid);

  else
    mxerror(boost::format(Y("The timecode file '%1%' contains an unsupported/unrecognized format (version %2%).\n")) % file_name % version);

  factory->parse(*in);
  delete in;

  return timecode_factory_cptr(factory);
}

timecode_factory_cptr
timecode_factory_c::create_fps_factory(int64_t default_duration,
                                       const std::string &source_name,
                                       int64_t tid) {
  mm_text_io_c text_io(new mm_mem_io_c(nullptr, 0, 1024));
  text_io.puts("# timecode format v1\n");
  text_io.puts(boost::format("assume %1%\n") % to_string(1000000000.0 / default_duration, 9));
  text_io.setFilePointer(0, seek_beginning);

  timecode_factory_cptr factory(new timecode_factory_v1_c("dummy", source_name, tid));
  factory->parse(text_io);

  return factory;
}

void
timecode_factory_v1_c::parse(mm_io_c &in) {
  std::string line;
  timecode_range_c t;
  std::vector<timecode_range_c>::iterator iit;
  std::vector<timecode_range_c>::const_iterator pit;

  int line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(boost::format(Y("The timecode file '%1%' does not contain a valid 'Assume' line with the default number of frames per second.\n")) % m_file_name);
    line_no++;
    strip(line);
    if (!line.empty() && ('#' != line[0]))
      break;
  } while (true);

  if (!ba::istarts_with(line, "assume "))
    mxerror(boost::format(Y("The timecode file '%1%' does not contain a valid 'Assume' line with the default number of frames per second.\n")) % m_file_name);

  line.erase(0, 6);
  strip(line);

  if (!parse_double(line.c_str(), m_default_fps))
    mxerror(boost::format(Y("The timecode file '%1%' does not contain a valid 'Assume' line with the default number of frames per second.\n")) % m_file_name);

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if (line.empty() || ('#' == line[0]))
      continue;

    std::vector<std::string> parts = split(line, ",", 3);
    if (   (parts.size() != 3)
        || !parse_uint(parts[0], t.start_frame)
        || !parse_uint(parts[1], t.end_frame)
        || !parse_double(parts[2], t.fps)) {
      mxwarn(boost::format(Y("Line %1% of the timecode file '%2%' could not be parsed.\n")) % line_no % m_file_name);
      continue;
    }

    if ((t.fps <= 0) || (t.end_frame < t.start_frame)) {
      mxwarn(boost::format(Y("Line %1% of the timecode file '%2%' contains inconsistent data (e.g. the start frame number is bigger than the end frame "
                             "number, or some values are smaller than zero).\n")) % line_no % m_file_name);
      continue;
    }

    m_ranges.push_back(t);
  }

  mxdebug_if(m_debug, boost::format("ext_timecodes: Version 1, default fps %1%, %2% entries.\n") % m_default_fps % m_ranges.size());

  if (m_ranges.size() == 0)
    t.start_frame = 0;
  else {
    std::sort(m_ranges.begin(), m_ranges.end());
    bool done;
    do {
      done = true;
      iit  = m_ranges.begin();
      size_t i;
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
  t.fps       = m_default_fps;
  m_ranges.push_back(t);

  m_ranges[0].base_timecode = 0.0;
  pit = m_ranges.begin();
  for (iit = m_ranges.begin() + 1; iit < m_ranges.end(); iit++, pit++)
    iit->base_timecode = pit->base_timecode + ((double)pit->end_frame - (double)pit->start_frame + 1) * 1000000000.0 / pit->fps;

  for (iit = m_ranges.begin(); iit < m_ranges.end(); iit++)
    mxdebug_if(m_debug, boost::format("ranges: entry %1% -> %2% at %3% with %4%\n") % iit->start_frame % iit->end_frame % iit->fps % iit->base_timecode);
}

bool
timecode_factory_v1_c::get_next(packet_cptr &packet) {
  packet->assigned_timecode = get_at(m_frameno);
  if (!m_preserve_duration || (0 >= packet->duration))
    packet->duration = get_at(m_frameno + 1) - packet->assigned_timecode;

  m_frameno++;
  if ((m_frameno > m_ranges[m_current_range].end_frame) && (m_current_range < (m_ranges.size() - 1)))
    m_current_range++;

  mxdebug_if(m_debug, boost::format("ext_timecodes v1: tc %1% dur %2% for %3%\n") % packet->assigned_timecode % packet->duration % (m_frameno - 1));

  return false;
}

int64_t
timecode_factory_v1_c::get_at(uint64_t frame) {
  timecode_range_c *t = &m_ranges[m_current_range];
  if ((frame > t->end_frame) && (m_current_range < (m_ranges.size() - 1)))
    t = &m_ranges[m_current_range + 1];

  return (int64_t)(t->base_timecode + 1000000000.0 * (frame - t->start_frame) / t->fps);
}

void
timecode_factory_v2_c::parse(mm_io_c &in) {
  std::string line;
  std::map<int64_t, int64_t> dur_map;

  int64_t dur_sum          = 0;
  int line_no              = 0;
  double previous_timecode = 0;

  while (in.getline2(line)) {
    line_no++;
    strip(line);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    double timecode;
    if (!parse_double(line.c_str(), timecode))
      mxerror(boost::format(Y("The line %1% of the timecode file '%2%' does not contain a valid floating point number.\n")) % line_no % m_file_name);

    if ((2 == m_version) && (timecode < previous_timecode))
      mxerror(boost::format(Y("The timecode v2 file '%1%' contains timecodes that are not ordered. "
                              "Due to a bug in mkvmerge versions up to and including v1.5.0 this was necessary "
                              "if the track to which the timecode file was applied contained B frames. "
                              "Starting with v1.5.1 mkvmerge now handles this correctly, and the timecodes in the timecode file must be ordered normally. "
                              "For example, the frame sequence 'IPBBP...' at 25 FPS requires a timecode file with "
                              "the first timecodes being '0', '40', '80', '120' etc and not '0', '120', '40', '80' etc.\n\n"
                              "If you really have to specify non-sorted timecodes then use the timecode format v4. "
                              "It is identical to format v2 but allows non-sorted timecodes.\n"))
              % in.get_file_name());

    previous_timecode = timecode;
    m_timecodes.push_back((int64_t)(timecode * 1000000));
    if (m_timecodes.size() > 1) {
      int64_t duration = m_timecodes[m_timecodes.size() - 1] - m_timecodes[m_timecodes.size() - 2];
      if (dur_map.find(duration) == dur_map.end())
        dur_map[duration] = 1;
      else
        dur_map[duration] = dur_map[duration] + 1;
      dur_sum += duration;
      m_durations.push_back(duration);
    }
  }

  if (m_timecodes.empty())
    mxerror(boost::format(Y("The timecode file '%1%' does not contain any valid entry.\n")) % m_file_name);

  if (m_debug) {
    mxdebug("Absolute probablities with maximum in separate line:\n");
    mxdebug("Duration  | Absolute probability\n");
    mxdebug("----------+---------------------\n");
  }

  dur_sum = -1;
  std::pair<int64_t, int64_t> entry;
  for (auto entry : dur_map) {
    if ((0 > dur_sum) || (dur_map[dur_sum] < entry.second))
      dur_sum = entry.first;
    mxdebug_if(m_debug, boost::format("%|1$ 9lld| | %|2$ 9lld|\n") % entry.first % entry.second);
  }

  mxdebug_if(m_debug, "Max-------+---------------------\n");
  mxdebug_if(m_debug, boost::format("%|1$ 9lld| | %|2$ 9lld|\n") % dur_sum % dur_map[dur_sum]);

  if (0 < dur_sum)
    m_default_fps = (double)1000000000.0 / dur_sum;

  m_durations.push_back(dur_sum);
}

bool
timecode_factory_v2_c::get_next(packet_cptr &packet) {
  if ((static_cast<size_t>(m_frameno) >= m_timecodes.size()) && !m_warning_printed) {
    mxwarn_tid(m_source_name, m_tid,
               boost::format(Y("The number of external timecodes %1% is smaller than the number of frames in this track. "
                               "The remaining frames of this track might not be timestamped the way you intended them to be. mkvmerge might even crash.\n"))
               % m_timecodes.size());
    m_warning_printed = true;

    if (m_timecodes.empty()) {
      packet->assigned_timecode = 0;
      if (!m_preserve_duration || (0 >= packet->duration))
        packet->duration = 0;

    } else {
      packet->assigned_timecode = m_timecodes.back();
      if (!m_preserve_duration || (0 >= packet->duration))
        packet->duration = m_timecodes.back();
    }

    return false;
  }

  packet->assigned_timecode = m_timecodes[m_frameno];
  if (!m_preserve_duration || (0 >= packet->duration))
    packet->duration = m_durations[m_frameno];
  m_frameno++;

  return false;
}

void
timecode_factory_v3_c::parse(mm_io_c &in) {
  std::string line;
  timecode_duration_c t;
  std::vector<timecode_duration_c>::iterator iit;
  std::vector<timecode_duration_c>::const_iterator pit;

  std::string err_msg_assume = (boost::format(Y("The timecode file '%1%' does not contain a valid 'Assume' line with the default number of frames per second.\n")) % m_file_name).str();

  int line_no = 1;
  do {
    if (!in.getline2(line))
      mxerror(err_msg_assume);
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);

  if (!ba::istarts_with(line, "assume "))
    mxerror(err_msg_assume);
  line.erase(0, 6);
  strip(line);

  if (!parse_double(line.c_str(), m_default_fps))
    mxerror(err_msg_assume);

  while (in.getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    double dur;
    if (ba::istarts_with(line, "gap,")) {
      line.erase(0, 4);
      strip(line);

      t.is_gap = true;
      t.fps    = m_default_fps;

      if (!parse_double(line.c_str(), dur))
        mxerror(boost::format(Y("The timecode file '%1%' does not contain a valid 'Gap' line with the duration of the gap.\n")) % m_file_name);
      t.duration = (int64_t)(1000000000.0 * dur);

    } else {
      t.is_gap = false;
      std::vector<std::string> parts = split(line, ",");

      if ((1 == parts.size()) && parse_double(parts[0], dur))
        t.fps = m_default_fps;

      else if ((2 != parts.size()) || !parse_double(parts[1], t.fps)) {
        mxwarn(boost::format(Y("Line %1% of the timecode file '%2%' could not be parsed.\n")) % line_no % m_file_name);
        continue;
      }
      t.duration = (int64_t)(1000000000.0 * dur);
    }

    if ((t.fps < 0) || (t.duration <= 0)) {
      mxwarn(boost::format(Y("Line %1% of the timecode file '%2%' contains inconsistent data (e.g. the duration or the FPS are smaller than zero).\n"))
             % line_no % m_file_name);
      continue;
    }

    m_durations.push_back(t);
  }

  mxdebug_if(m_debug, boost::format("ext_timecodes: Version 3, default fps %1%, %2% entries.\n") % m_default_fps % m_durations.size());

  if (m_durations.size() == 0)
    mxwarn(boost::format(Y("The timecode file '%1%' does not contain any valid entry.\n")) % m_file_name);

  t.duration = 0xfffffffffffffffll;
  t.is_gap   = false;
  t.fps      = m_default_fps;
  m_durations.push_back(t);

  for (iit = m_durations.begin(); iit < m_durations.end(); iit++)
    mxdebug_if(m_debug, boost::format("durations:%1% entry for %2% with %3% FPS\n") % (iit->is_gap ? " gap" : "") % iit->duration % iit->fps);
}

bool
timecode_factory_v3_c::get_next(packet_cptr &packet) {
  bool result = false;

  if (m_durations[m_current_duration].is_gap) {
    // find the next non-gap
    size_t duration_index = m_current_duration;
    while (m_durations[duration_index].is_gap || (0 == m_durations[duration_index].duration)) {
      m_current_offset += m_durations[duration_index].duration;
      duration_index++;
    }
    m_current_duration = duration_index;
    result             = true;
    // yes, there is a gap before this frame
  }

  packet->assigned_timecode = m_current_offset + m_current_timecode;
  // If default_fps is 0 then the duration is unchanged, usefull for audio.
  if (m_durations[m_current_duration].fps && (!m_preserve_duration || (0 >= packet->duration)))
    packet->duration = (int64_t)(1000000000.0 / m_durations[m_current_duration].fps);

  packet->duration   /= packet->time_factor;
  m_current_timecode += packet->duration;

  if (m_current_timecode >= m_durations[m_current_duration].duration) {
    m_current_offset   += m_durations[m_current_duration].duration;
    m_current_timecode  = 0;
    m_current_duration++;
  }

  mxdebug_if(m_debug, boost::format("ext_timecodes v3: tc %1% dur %2%\n") % packet->assigned_timecode % packet->duration);

  return result;
}
