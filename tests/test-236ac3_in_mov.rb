#!/usr/bin/ruby -w

class T_236ac3_in_mov < Test
  def description
    return "mkvmerge / AC3 in MOV and constant sample size > 1 audio tracks / in(MOV)"
  end

  def run
    merge("-D data/mp4/swordfish-explosion.mov")
    return hash_tmp
  end
end

