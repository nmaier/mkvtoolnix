#!/usr/bin/ruby -w

class T_288identify_files_by_amg < Test
  def description
    "mkvmerge / identify on files created by AMG"
  end

  def run
    sys "../src/mkvmerge --identify-verbose data/mkv/amg_sample.mkv | grep '^Track' | wc -l > #{tmp}", 0
    hash_tmp
  end
end

