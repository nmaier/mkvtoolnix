#!/usr/bin/ruby -w

class T_207segmentinfo < Test
  def description
    return "mkvmerge / segment info XML file / in(XML)"
  end

  def run
    merge("data/simple/v.mp3 --segmentinfo data/text/seginfo.xml")
    return hash_tmp
  end
end

