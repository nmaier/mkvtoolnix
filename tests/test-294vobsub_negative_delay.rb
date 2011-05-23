#!/usr/bin/ruby -w

class T_294vobsub_negative_delay < Test
  def description
    "mkvmerge / VobSub with negative \"delay\" fields"
  end

  def run
    merge "data/vobsub/House.S07E22.idx"
    hash_tmp
  end
end

