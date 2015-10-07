#!/usr/bin/ruby -w

class T_326mpeg_ps_mpeg_audio_layer4 < Test
  def description
    "mkvmerge / MPEG PS with MPEG audio layer 4"
  end

  def run
    merge "data/mpeg12/mpeg_audio_layer4.mpg", 1
    hash_tmp
  end
end

