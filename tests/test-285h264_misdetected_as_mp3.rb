#!/usr/bin/ruby -w

class T_285h264_misdetected_as_mp3 < SimpleTest
  describe "mkvmerge / h264 ES mis-detected as MP3"

  test_identify "data/h264/bug574.h264"
end

