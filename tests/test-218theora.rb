#!/usr/bin/ruby -w

class T_218theora < Test
  def description
    return "mkvmerge / Theora from Ogg / in(OGG)"
  end

  def run
    merge("data/ogg/qt4dance_medium.ogg")
    return hash_tmp
  end
end

