#!/usr/bin/ruby -w

class T_017chapters < Test
  def initialize
    @description = "mkvmerge / chapters from XML and simple OGM style files " +
      "/ in(txt, XML)"
  end

  def run
    merge("data/vde.srt --chapters data/shortchaps.txt")
    hash = hash_tmp
    merge("data/vde.srt --chapters data/vmap.chapters.xml")
    return hash + "-" + hash_tmp
  end
end

