#!/usr/bin/ruby -w

class T_330dts_detection < Test
  def description
    "mkvmerge / segfault during DTS detection"
  end

  def run
    merge "data/dts/Sherlock_Holmes_first_20mb.dtshd"
    hash_tmp
  end
end

