#!/usr/bin/ruby -w

class T_229rav3_in_rm < Test
  def description
    return "mkvmerge / RealAudio v3 in RealMedia / in(RM)"
  end

  def run
    merge("data/rm/ra3_in_rm_file.rm", 1)
    return hash_tmp
  end
end

