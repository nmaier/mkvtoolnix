#!/usr/bin/ruby -w

class T_205X_cuesheets < Test
  def description
    return "mkvextract / cue sheets / in(MKV)"
  end

  def run
    merge("#{tmp}-src", "data/simple/v.mp3 --chapters " +
           "data/text/cuewithtags2.cue")
    sys("../src/mkvextract cuesheet #{tmp}-src --no-variable-data &> #{tmp}")
    hash = hash_tmp(false)
    sys("../src/mkvextract tracks #{tmp}-src --no-variable-data " +
         "--cuesheet 1:#{tmp} > /dev/null")
    hash += "-" + hash_tmp("#{tmp}.cue")
    unlink_tmp_files
    return hash
  end
end

