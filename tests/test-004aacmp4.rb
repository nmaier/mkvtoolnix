#!/usr/bin/ruby -w

class T_004aacmp4 < Test
  def initialize
    @description = "mkvmerge / audio only / in(AAC in MP4)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/v.mp4")
    return hash_tmp
  end
end
