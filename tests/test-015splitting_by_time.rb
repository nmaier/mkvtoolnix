#!/usr/bin/ruby -w

class T_015splitting_by_time < Test
  def description
    return "mkvmerge / splitting by time / in(AVI)"
  end

  def run
    merge(tmp + "-%03d", "--split-max-files 2 --split 40s data/avi/v.avi")
    hash = hash_file(tmp + "-001") + "-" + hash_file(tmp + "-002")
    File.unlink(tmp + "-001")
    File.unlink(tmp + "-002")

    return hash
  end
end

