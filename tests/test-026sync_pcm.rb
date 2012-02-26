#!/usr/bin/ruby -w

class T_026sync_pcm < Test
  def description
    return "mkvmerge / PCM sync / in(WAV)"
  end

  def run
    merge("--sync 0:500 data/wav/v.wav")
    hash = hash_tmp
    merge("--sync 0:-500 data/wav/v.wav")
    return hash + "-" + hash_tmp
  end
end

