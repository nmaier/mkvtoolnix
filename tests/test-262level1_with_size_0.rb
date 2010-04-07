#!/usr/bin/ruby -w

class T_262level1_with_size_0 < Test
  def description
    return "mkvmerge / level 1 elements with size 0"
  end

  def run
    merge "data/mkv/pcm_u8-audio-stored-as-wavpack.mkv"
    return hash_tmp
  end
end

