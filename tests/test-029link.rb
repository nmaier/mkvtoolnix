#!/usr/bin/ruby -w

class T_029link < Test
  def description
    return "mkvmerge / link to previous & next / in(AVI)"
  end

  def run
    merge("--link --link-to-previous 000102030405060708090A0B0C0D0E0F " +
           "--link-to-next 0f0e0d0c0b0a09080706050403020100 data/avi/v.avi", 1)
    return hash_tmp
  end
end

