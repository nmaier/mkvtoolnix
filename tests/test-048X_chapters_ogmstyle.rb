#!/usr/bin/ruby -w

class T_048X_chapters_ogmstyle < Test
  def description
    return "mkvextract / chapters (OGM style) / out(TXT)"
  end

  def run
    sys("../src/mkvextract chapters data/mkv/complex.mkv -s &> #{tmp}")
    return hash_tmp
  end
end

