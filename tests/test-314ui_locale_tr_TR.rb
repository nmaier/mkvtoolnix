#!/usr/bin/ruby -w

class T_314ui_locale_tr_TR < Test
  def description
    "mkvmerge / UI locale: tr_TR"
  end

  def run
    hashes = Array.new

    merge "/dev/null", "--ui-language tr_TR data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-tr_TR"
    hashes << hash_file("#{tmp}-tr_TR")

    sys "../src/mkvinfo --ui-language tr_TR data/mkv/complex.mkv | head -n 2 > #{tmp}-tr_TR"
    hashes << hash_file("#{tmp}-tr_TR")

    return hashes.join "-"
  end
end
