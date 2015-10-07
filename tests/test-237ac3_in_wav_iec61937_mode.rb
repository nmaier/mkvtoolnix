#!/usr/bin/ruby -w

class T_237ac3_in_wav_iec61937_mode < Test
  def description
    return "mkvmerge / AC3 in WAV in IEC 61937 mode / in(WAV)"
  end

  def run
    merge("data/ac3/shortsample.ac3.wav")
    return hash_tmp
  end
end

