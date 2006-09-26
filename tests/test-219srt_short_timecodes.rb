#!/usr/bin/ruby -w

class T_219srt_short_timecodes < Test
  def description
    return "mkvmerge / SRT subs with short timecodes / in(SRT)"
  end

  def run
    merge("'data/textsubs/Marchen Awakens Romance - 40.srt' " +
           "'data/textsubs/Marchen Awakens Romance - 41.srt'")
    return hash_tmp
  end
end

