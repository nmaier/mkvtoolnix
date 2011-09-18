#!/usr/bin/ruby -w

class T_301ts_pgssub < Test
  def description
    "mkvmerge / MPEG TS with HDMV PGS subtitles"
  end

  def run
    merge "-A -D data/ts/h264_dts_hd_ma_pgsub.m2ts"
    hash_tmp
  end
end

