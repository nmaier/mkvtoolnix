#!/usr/bin/ruby -w

class T_316ui_locale_zh_CN < Test
  def description
    "mkvmerge / UI locale: zh_CN"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language zh_CN data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-zh_CN"
    hashes << hash_file("#{tmp}-zh_CN")

    sys "../src/mkvinfo --ui-language zh_CN data/mkv/complex.mkv | head -n 2 > #{tmp}-zh_CN"
    hashes << hash_file("#{tmp}-zh_CN")

    return hashes.join "-"
  end
end
