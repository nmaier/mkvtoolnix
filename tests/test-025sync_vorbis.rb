#!/usr/bin/ruby -w

class T_025sync_vorbis < Test
  def description
    return "mkvmerge / Vorbis sync / in(Ogg)"
  end

  def run
    merge("--sync -1:500 data/ogg/v.ogg")
    hash = hash_tmp
    merge("--sync -1:-500 data/ogg/v.ogg")
    return hash + "-" + hash_tmp
  end
end

