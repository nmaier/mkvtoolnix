#!/usr/bin/ruby -w

class T_305ui_locale_en_US < Test
  def description
    "mkvmerge / UI locale: en_US"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language en_US data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-en_US"
    hashes << hash_file("#{tmp}-en_US")

    sys "../src/mkvinfo --ui-language en_US data/mkv/complex.mkv | head -n 2 > #{tmp}-en_US"
    hashes << hash_file("#{tmp}-en_US")

    return hashes.join "-"
  end
end
