#!/usr/bin/ruby -w

class T_225dts_in_wav < Test
  def description
    return "mkvmerge / DTS audio from WAV / in(WAV)"
  end

  def run
    merge("'data/dts/Norrlanda.wav'")
    h = hash_tmp
    merge("'data/dts/SURROUNDTEST_011212.wav'")
    return h + "-" + hash_tmp
  end
end

