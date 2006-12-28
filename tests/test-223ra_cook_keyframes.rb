#!/usr/bin/ruby -w

class T_223ra_cook_keyframes < Test
  def description
    return "mkvmerge / RealAudio COOK with invalid key frame flags / in(RA)"
  end

  def run
    merge("data/rm/R2_petshopboys.ra")
    return hash_tmp
  end
end

