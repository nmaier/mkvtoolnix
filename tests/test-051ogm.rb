#!/usr/bin/ruby -w

class T_051ogm < Test
  def description
    return "mkvmerge / OGM audio & video / in(OGM)"
  end

  def run
    merge "--subtitle-charset -1:ISO-8859-15 data/ogg/v.ogm"
    return hash_tmp
  end
end

