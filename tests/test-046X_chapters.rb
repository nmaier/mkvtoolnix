#!/usr/bin/ruby -w

class T_046X_chapters < Test
  def description
    return "mkvextract / chapters / out(XML)"
  end

  def run
    sys("../src/mkvextract chapters data/mkv/complex.mkv > #{tmp} 2>/dev/null")
    return hash_tmp
  end
end

