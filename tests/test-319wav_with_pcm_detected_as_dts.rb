#!/usr/bin/ruby -w

class T_319wav_with_pcm_detected_as_dts < Test
  def description
    "mkvmerge / WAV with PCM detected as DTS"
  end

  def run
    merge "data/wav/wav_with_pcm_detected_as_dts.wav"
    hash_tmp
  end
end

