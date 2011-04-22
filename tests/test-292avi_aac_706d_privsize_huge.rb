#!/usr/bin/ruby -w

class T_292avi_aac_706d_privsize_huge < Test
  def description
    "mkvmerge / AVI with AAC audio codec 706d and bogus private data size"
  end

  def run
    merge "data/avi/divxFaac51.avi"
    hash_tmp
  end
end

