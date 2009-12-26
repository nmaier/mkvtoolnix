#!/usr/bin/ruby -w

class T_258srt_negative_timecodes < Test
  def description
    return "mkvmerge / SRT files with negative timecodes / in(SRT)"
  end

  def run
    merge(1, "data/textsubs/negative_timecodes.srt")
    return hash_tmp
  end
end

