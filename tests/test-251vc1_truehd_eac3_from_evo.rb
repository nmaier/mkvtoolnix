#!/usr/bin/ruby -w

class T_251vc1_truehd_eac3_from_evo < Test
  def description
    return "mkvmerge / VC-1, TrueHD, EAC3 from EVO / in(EVO)"
  end

  def run
    merge("data/vob/vc1_truehd_eac3.evo")
    return hash_tmp
  end
end

