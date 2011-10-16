#!/usr/bin/ruby -w

class T_315ui_locale_uk_UA < Test
  def description
    "mkvmerge / UI locale: uk_UA"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language uk_UA data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-uk_UA"
    hashes << hash_file("#{tmp}-uk_UA")

    sys "../src/mkvinfo --ui-language uk_UA data/mkv/complex.mkv | head -n 2 > #{tmp}-uk_UA"
    hashes << hash_file("#{tmp}-uk_UA")

    return hashes.join "-"
  end
end
