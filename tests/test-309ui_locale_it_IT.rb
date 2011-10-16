#!/usr/bin/ruby -w

class T_309ui_locale_it_IT < Test
  def description
    "mkvmerge / UI locale: it_IT"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language it_IT data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-it_IT"
    hashes << hash_file("#{tmp}-it_IT")

    sys "../src/mkvinfo --ui-language it_IT data/mkv/complex.mkv | head -n 2 > #{tmp}-it_IT"
    hashes << hash_file("#{tmp}-it_IT")

    return hashes.join "-"
  end
end
