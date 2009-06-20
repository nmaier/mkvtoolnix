#!/usr/bin/ruby -w

class T_252native_mpeg4 < Test
  def description
    return "mkvmerge / Native MPEG4 part 2 / in(AVI)"
  end

  def run
    merge("--engage native_mpeg4 data/avi/vielfrass_xvid_test.avi")
    return hash_tmp
  end
end

