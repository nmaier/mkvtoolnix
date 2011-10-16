#!/usr/bin/ruby -w

class T_317ui_locale_zh_TW < Test
  def description
    "mkvmerge / UI locale: zh_TW"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language zh_TW data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-zh_TW"
    hashes << hash_file("#{tmp}-zh_TW")

    sys "../src/mkvinfo --ui-language zh_TW data/mkv/complex.mkv | head -n 2 > #{tmp}-zh_TW"
    hashes << hash_file("#{tmp}-zh_TW")

    return hashes.join "-"
  end
end
