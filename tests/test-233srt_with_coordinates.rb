#!/usr/bin/ruby -w

class T_233srt_with_coordinates < Test
  def description
    return "mkvmerge / SRT subtitles with coordinates on the timecode line / in(SRT)"
  end

  def run
    merge(1, "data/subtitles/srt/season2_us_disc1_episode01.srt")
    return hash_tmp
  end
end

