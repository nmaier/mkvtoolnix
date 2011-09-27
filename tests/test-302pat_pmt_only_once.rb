#!/usr/bin/ruby -w

class T_302pat_pmt_only_once < Test
  def description
    "mkvmerge / TS with only one copy of PAT & PMT"
  end

  def run
    merge "data/ts/pat_pmt_only_once.ts"
    hash_tmp
  end
end

