#!/usr/bin/ruby -w

class T_304eac3_pes_private_but_no_pmt_descriptor_tag < Test
  def description
    "mkvmerge / MPEG TS with EAC3 in PES private missing its PMT descriptor tag"
  end

  def run
    merge "data/ts/eac3_pes_private_but_no_pmt_descriptor_tag.ts"
    hash_tmp
  end
end

