#!/usr/bin/ruby -w

class T_051ogm < Test
  def description
    return "mkvmerge / OGM audio & video / in(OGM)"
  end

  def run
    merge("data/ogg/v.ogm")
    return hash_tmp
  end
end

