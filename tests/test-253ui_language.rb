#!/usr/bin/ruby -w

class T_253ui_language < Test
  def description
    return "all / user interface languages"
  end

  def run
    hashes = Array.new

    %w{en_US de_DE es_ES fr_FR ja_JP nl_NL ru_RU tr_TR uk_UA zh_CN zh_TW}.each do |language|
      merge "/dev/null", "--ui-language #{language} data/avi/v.avi | head -n 2 | tail -n 1 > #{tmp}-#{language}"
      hashes << hash_file("#{tmp}-#{language}")

      sys "../src/mkvinfo --ui-language #{language} data/mkv/complex.mkv | head -n 2 > #{tmp}-#{language}"
      hashes << hash_file("#{tmp}-#{language}")
    end

    sys "../src/mkvmerge --ui-language gnufudel", 2

    return hashes.join "-"
  end
end

