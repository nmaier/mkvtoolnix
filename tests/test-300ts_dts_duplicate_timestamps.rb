#!/usr/bin/ruby -w

class T_300ts_dts_duplicate_timestamps < Test
  def description
    "mkvmerge / MPEG TS with DTS-HD MA"
  end

  def run
    merge "-D -S -a 1,2 data/ts/h264_dts_hd_ma_pgsub.m2ts"
    hash_tmp
  end
end

