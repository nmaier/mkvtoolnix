#!/usr/bin/ruby -w

class T_310ui_locale_ja_JP < Test
  def description
    "mkvmerge / UI locale: ja_JP"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language ja_JP data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-ja_JP"
    hashes << hash_file("#{tmp}-ja_JP")

    sys "../src/mkvinfo --ui-language ja_JP data/mkv/complex.mkv | head -n 2 > #{tmp}-ja_JP"
    hashes << hash_file("#{tmp}-ja_JP")

    return hashes.join "-"
  end
end
