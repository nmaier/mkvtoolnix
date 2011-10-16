#!/usr/bin/ruby -w

class T_308ui_locale_fr_FR < Test
  def description
    "mkvmerge / UI locale: fr_FR"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language fr_FR data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-fr_FR"
    hashes << hash_file("#{tmp}-fr_FR")

    sys "../src/mkvinfo --ui-language fr_FR data/mkv/complex.mkv | head -n 2 > #{tmp}-fr_FR"
    hashes << hash_file("#{tmp}-fr_FR")

    return hashes.join "-"
  end
end
