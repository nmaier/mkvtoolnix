#!/usr/bin/ruby -w

class T_022display_dimensions < Test
  def description
    return "mkvmerge / display dimensions / in(AVI)"
  end

  def run
    merge("-A --display-dimensions 0:640x480 data/avi/v.avi")
    return hash_tmp
  end
end

