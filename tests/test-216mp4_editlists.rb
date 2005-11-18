#!/usr/bin/ruby -w

class T_216mp4_editlists < Test
  def description
    return "mkvmerge / edit lists in MP4 and 'colr' before 'avcC' / in(MP4)"
  end

  def run
    merge("-A -S data/mp4/oops.mov")
    return hash_tmp
  end
end

