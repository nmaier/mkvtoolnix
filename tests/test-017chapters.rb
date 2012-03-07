#!/usr/bin/ruby -w

class T_017chapters < Test
  def description
    return "mkvmerge / chapters from XML and simple OGM style files " +
      "/ in(txt, XML)"
  end

  def run
    merge("--sub-charset 0:ISO-8859-1 data/srt/vde.srt " +
           "--chapter-charset ISO-8859-1 --chapters data/text/shortchaps.txt")
    hash = hash_tmp
    merge("--sub-charset 0:ISO-8859-1 data/srt/vde.srt " +
           "--chapters data/text/vmap.chapters.xml")
    return hash + "-" + hash_tmp
  end
end

