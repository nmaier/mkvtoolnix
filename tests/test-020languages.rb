#!/usr/bin/ruby -w

class T_020languages < Test
  def description
    return "mkvmerge / track languages / in(*)"
  end

  def run
    merge("--language 2:eng --language 3:ger --language 4:fre  " +
           "--language 5:nl --language 6:tr --language 7:tlh " +
           "--language 8:ro --language 9:fi --language 10:be " +
           "data/mkv/complex.mkv")
    return hash_tmp
  end
end

