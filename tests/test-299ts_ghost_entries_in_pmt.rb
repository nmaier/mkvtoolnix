#!/usr/bin/ruby -w

class T_299ts_ghost_entries_in_pmt < Test
  def description
    "mkvmerge / MPEG ts: ghost entries in PMT"
  end

  def run
    merge "data/ts/mp3_listed_in_pmt_but_no_data.ts"
    hash_tmp
  end
end

