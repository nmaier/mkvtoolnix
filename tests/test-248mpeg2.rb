#!/usr/bin/ruby -w

class T_248mpeg2 < Test
  def description
    return "mkvmerge / MPEG-1/2 video files / in(M2V)"
  end

  def run
    merge("data/mpeg12/1080i60.m2v")
    hash = hash_tmp
    merge("data/mpeg12/changing_sequence_headers.m2v")
    return hash + "-" + hash_tmp
  end
end

