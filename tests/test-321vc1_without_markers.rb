#!/usr/bin/ruby -w

class T_321vc1_without_markers < Test
  def description
    "mkvmerge / VC1 without start markers (0x00 0x00 0x01)"
  end

  def run
    merge "-A data/mkv/vc1-without-markers.mkv"
    hash_tmp
  end
end

