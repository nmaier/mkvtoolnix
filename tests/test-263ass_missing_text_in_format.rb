#!/usr/bin/ruby -w

class T_263ass_missing_text_in_format < Test
  def description
    return "mkvextract / ASS: missing column 'text' in format line"
  end

  def run
    xtr_tracks "data/mkv/ass_missing_text_in_format.mkv", "2:#{tmp}2 3:#{tmp}3"
    result = hash_file(tmp + "2") + "-" + hash_file(tmp + "3")
    File.unlink(tmp + "2")
    File.unlink(tmp + "3")

    return result
  end
end

