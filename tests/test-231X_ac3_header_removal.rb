#!/usr/bin/ruby -w

class T_231X_ac3_header_removal < Test
  def description
    return "mkvextract / raw AC3 with header removal / out(AC3)"
  end

  def run
    xtr_tracks("data/simple/ac3_header_removal.mka", "1:#{tmp}")
    return hash_tmp
  end
end

