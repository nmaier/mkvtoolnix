#!/usr/bin/ruby -w

class T_313ui_locale_ru_RU < Test
  def description
    "mkvmerge / UI locale: ru_RU"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language ru_RU data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-ru_RU"
    hashes << hash_file("#{tmp}-ru_RU")

    sys "../src/mkvinfo --ui-language ru_RU data/mkv/complex.mkv | head -n 2 > #{tmp}-ru_RU"
    hashes << hash_file("#{tmp}-ru_RU")

    return hashes.join "-"
  end
end
