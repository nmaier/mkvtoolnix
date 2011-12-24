#!/usr/bin/ruby -w

class T_050X_ass < Test
  def description
    return "mkvextract / ASS subtitles / out(ASS)"
  end

  def run
    my_hash = hash_file("data/textsubs/11.Magyar.ass")
    my_tmp = tmp
    merge("--sub-charset 0:ISO-8859-1 data/textsubs/11.Magyar.ass")
    @tmp = nil
    xtr_tracks(my_tmp, "-c ISO-8859-1 0:#{tmp}")
    File.unlink(my_tmp)
    return my_hash + "-" + hash_tmp
  end
end

