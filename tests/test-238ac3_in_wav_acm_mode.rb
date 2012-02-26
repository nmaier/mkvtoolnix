#!/usr/bin/ruby -w

class T_238ac3_in_wav_acm_mode < Test
  def description
    return "mkvmerge / AC3 in WAV in ACM mode / in(WAV)"
  end

  def run
    merge(1, "data/wav/Pirates.Of.The.Caribbean.At.Worlds.End_30secssample.wav")
    return hash_tmp
  end
end

