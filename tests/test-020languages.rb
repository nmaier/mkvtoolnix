#!/usr/bin/ruby -w

class T_020languages < Test
  def description
    "mkvmerge / track languages / in(*)"
  end

  def run
    merge "--language 1:eng --language 2:ger --language 3:fre --language 4:nl --language 5:tr --language 6:tlh --language 7:ro --language 8:fi --language 9:be data/mkv/complex.mkv"
    hash_tmp
  end
end

