#!/usr/bin/ruby -w

class T_015splitting_by_time < Test
  def initialize
    @description = "mkvmerge / splitting by time / in(AVI)"
  end

  def run
    merge(tmp + "-%03d", "--split-max-files 2 --split 20s data/v.avi")
    if (!FileTest.exist?(tmp + "-001"))
      error("First split file does not exist.")
    end
    if (!FileTest.exist?(tmp + "-002"))
      File.unlink(tmp + "-001")
      error("Second split file does not exist.")
    end
    hash = hash_file(tmp + "-001") + "-" + hash_file(tmp + "-002")
    File.unlink(tmp + "-001")
    File.unlink(tmp + "-002")

    return hash
  end
end

