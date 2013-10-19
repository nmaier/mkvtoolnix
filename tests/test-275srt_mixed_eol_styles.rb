#!/usr/bin/ruby -w

class T_275srt_mixed_eol_styles < Test
  def description
    return "mkvmerge / SRT files with mixed end-of-line markings"
  end

  def run
    merge "'data/subtitles/srt/Space Buddies (2009) AVCHD 1080p DTS.srt'"
    return hash_tmp
  end
end

