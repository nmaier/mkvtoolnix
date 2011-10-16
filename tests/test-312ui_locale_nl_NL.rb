#!/usr/bin/ruby -w

class T_312ui_locale_nl_NL < Test
  def description
    "mkvmerge / UI locale: nl_NL"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language nl_NL data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-nl_NL"
    hashes << hash_file("#{tmp}-nl_NL")

    sys "../src/mkvinfo --ui-language nl_NL data/mkv/complex.mkv | head -n 2 > #{tmp}-nl_NL"
    hashes << hash_file("#{tmp}-nl_NL")

    return hashes.join "-"
  end
end
