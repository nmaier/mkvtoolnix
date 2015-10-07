#!/usr/bin/ruby -w

class T_289wav_unsupported_formattag < Test
  def description
    "mkvmerge / unsupported format tags in WAV files"
  end

  def run
    sys "../src/mkvmerge --identify data/wav/wmav2.wav", 3
    ($? >> 8).to_s
  end
end

