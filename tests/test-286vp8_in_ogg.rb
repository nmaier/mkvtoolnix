#!/usr/bin/ruby -w

class T_286vp8_in_ogg < Test
  def description
    "mkvmerge / VP8 video tracks in Ogg files / in(Ogg)"
  end

  def run
    merge "data/ogg/vp8.ogg"
    hash_tmp
  end
end

