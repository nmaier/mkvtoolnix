#!/usr/bin/ruby -w

class T_303mpeg_ts_eac3_pmt_descriptor_tag_0x7a < Test
  def description
    "mkvmerge / MPEG TS with EAC3 stored with PMT descriptor tag 0x7a"
  end

  def run
    merge "data/ts/eac3_pmt_descriptor_tag_0x7a.ts"
    hash_tmp
  end
end

