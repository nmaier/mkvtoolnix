#!/usr/bin/ruby -w

class T_254avi_with_subs < Test
  def description
    return "mkvmerge / AVI with SRT & SSA subtitles / in(AVI)"
  end

  def run
    file = "data/avi/with-subs.avi"
    merge(file)
    hash = hash_tmp
    merge("-s 2,3,4 #{file}")
    hash += "-" + hash_tmp
    merge("-s 2,4 #{file}")
    hash += "-" + hash_tmp
    merge("-s 4 #{file}")
    hash += "-" + hash_tmp
    merge("-S #{file}")
    hash += "-" + hash_tmp

    return hash
  end
end

