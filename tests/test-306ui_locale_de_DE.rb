#!/usr/bin/ruby -w

class T_306ui_locale_de_DE < Test
  def description
    "mkvmerge / UI locale: de_DE"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language de_DE data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-de_DE"
    hashes << hash_file("#{tmp}-de_DE")

    sys "../src/mkvinfo --ui-language de_DE data/mkv/complex.mkv | head -n 2 > #{tmp}-de_DE"
    hashes << hash_file("#{tmp}-de_DE")

    return hashes.join "-"
  end
end
