#!/usr/bin/ruby -w

class T_202avc_from_mp4_with_par_bframes < Test
  def description
    return "mkvmerge / AVC from MP4 with aspect ratio & B frames / in(MP4)"
  end

  def run
    merge("data/mp4/test_2000_inloop.mp4")
    return hash_tmp
  end
end

