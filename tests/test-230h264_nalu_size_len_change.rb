#!/usr/bin/ruby -w

class T_230h264_nalu_size_len_change < Test
  def description
    return "mkvmerge / NALU size length changes / in(MP4)"
  end

  def run
    merge("--nalu-size-length 3:2 data/mp4/test_2000_inloop.mp4")
    hash = hash_file(tmp) + "-"
    merge("#{tmp}.1", "--nalu-size-length 1:4 #{tmp}")
    hash += hash_file("#{tmp}.1")
    return hash
  end
end

