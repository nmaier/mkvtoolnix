#!/usr/bin/ruby -w

class T_028compression < Test
  def description
    return "mkvmerge / subtitle compression / in(VobSub)"
  end

  def run
    merge("--compression 1:none --compression 2:zlib --compression 3:bz2 " +
           "--compression 4:lzo data/vobsub/ally1-short.idx")
    return hash_tmp
  end
end

