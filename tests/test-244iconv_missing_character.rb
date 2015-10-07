#!/usr/bin/ruby -w

class T_244iconv_missing_character < Test
  def description
    return "mkvmerge / iconv cuts the last character (e.g. Hebrew) / in(SRT)"
  end

  def run
    merge("--sub-charset -1:CP1255 data/subtitles/srt/Madagascar.Hebrew.Encoding.srt")
    return hash_tmp
  end
end

