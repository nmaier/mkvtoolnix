#!/usr/bin/ruby -w

class T_014splitting_by_size < Test
  def description
    return "mkvmerge / splitting by file size / in(AVI)"
  end

  def run
    merge(tmp + "-%03d ", "--split-max-files 2 --split 4m data/avi/v.avi")
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

