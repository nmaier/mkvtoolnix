#!/usr/bin/ruby -w

class T_023no_x < Test
  def description
    return "mkvmerge / various --no-xyz options / in(*)"
  end

  def run
    merge("-A --no-attachments data/mkv/complex.mkv")
    hash = hash_tmp
    merge("-D --no-chapters data/mkv/complex.mkv")
    hash += "-" + hash_tmp
    merge("-S --no-global-tags --no-track-tags data/mkv/complex.mkv")
    return hash + "-" + hash_tmp
  end
end

