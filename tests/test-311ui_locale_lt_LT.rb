#!/usr/bin/ruby -w

class T_311ui_locale_lt_LT < Test
  def description
    "mkvmerge / UI locale: lt_LT"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language lt_LT data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-lt_LT"
    hashes << hash_file("#{tmp}-lt_LT")

    sys "../src/mkvinfo --ui-language lt_LT data/mkv/complex.mkv | head -n 2 > #{tmp}-lt_LT"
    hashes << hash_file("#{tmp}-lt_LT")

    return hashes.join "-"
  end
end
