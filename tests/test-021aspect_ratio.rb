#!/usr/bin/ruby -w

class T_021aspect_ratio < Test
  def description
    return "mkvmerge / aspect ratio / in(AVI)"
  end

  def run
    merge("-A --aspect-ratio 0:16/9 data/avi/v.avi")
    hash = hash_tmp
    merge("-A --aspect-ratio 0:4.333 data/avi/v.avi")
    return hash + "-" + hash_tmp
  end
end

