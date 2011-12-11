#!/usr/bin/ruby -w

class T_027default_track < Test
  def description
    "mkvmerge / default track flag / in(MKV)"
  end

  def run
    merge "--default-track 0 --default-track 4 --default-track 9 data/mkv/complex.mkv"
    hash_tmp
  end
end

