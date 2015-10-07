#!/usr/bin/ruby -w

class T_235wav_fmt_chunk_length < Test
  def description
    return "mkvmerge / WAV files with unusual 'fmt' chunk length / in(WAV)"
  end

  def run
    merge("data/wav/dts-sample.dts.wav")
    return hash_tmp
  end
end

