#!/usr/bin/ruby -w

class T_017chapters < Test
  def description
    return "mkvmerge / chapters from XML and simple OGM style files " +
      "/ in(txt, XML)"
  end

  def run
    merge("data/textsubs/vde.srt --chapters data/text/shortchaps.txt")
    hash = hash_tmp
    merge("data/textsubs/vde.srt --chapters data/text/vmap.chapters.xml")
    return hash + "-" + hash_tmp
  end
end

