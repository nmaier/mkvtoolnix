#!/usr/bin/ruby -w

class T_205X_cuesheets < Test
  def description
    return "mkvextract / cue sheets / in(MKV)"
  end

  def run
    merge("#{tmp}-src", "data/simple/v.mp3 --chapters data/text/cuewithtags2.cue")
    sys("../src/mkvextract cuesheet #{tmp}-src --engage no_variable_data > #{tmp} 2>/dev/null")
    hash = hash_tmp(false)
    sys("../src/mkvextract tracks #{tmp}-src --engage no_variable_data --cuesheet 1:#{tmp} > /dev/null 2>/dev/null")
    hash += "-" + hash_tmp("#{tmp}.cue")
    return hash
  end
end

