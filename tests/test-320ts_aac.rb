#!/usr/bin/ruby -w

class T_320ts_aac < Test
  def description
    "mkvmerge / MPEG transport streams with AAC"
  end

  def run
    merge "data/ts/avc_aac.ts", 1
    hash_tmp
  end
end

