#!/usr/bin/ruby -w

class T_226h264 < Test
  def description
    return "mkvmerge / h264 raw / in(h264)"
  end

  def run
    merge("data/h264/IcePrincess.h264")
    return hash_tmp
  end
end

