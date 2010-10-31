#!/usr/bin/ruby -w

class T_285h264_misdetected_as_mp3 < Test
  def description
    "mkvmerge / h264 ES mis-detected as MP3"
  end

  def run
    sys "../src/mkvmerge --identify-verbose data/h264/bug574.h264 > #{tmp}", 0
    hash_tmp
  end
end

