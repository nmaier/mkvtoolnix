#!/usr/bin/ruby -w

class T_307ui_locale_es_ES < Test
  def description
    "mkvmerge / UI locale: es_ES"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language es_ES data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-es_ES"
    hashes << hash_file("#{tmp}-es_ES")

    sys "../src/mkvinfo --ui-language es_ES data/mkv/complex.mkv | head -n 2 > #{tmp}-es_ES"
    hashes << hash_file("#{tmp}-es_ES")

    return hashes.join "-"
  end
end
