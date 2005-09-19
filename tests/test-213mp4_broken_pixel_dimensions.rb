#!/usr/bin/ruby -w

class T_213mp4_broken_pixel_dimensions < Test
  def description
    return "mkvmerge / MP4 with wrong pixel dimensions / in(MP4)"
  end

  def run
    merge("data/mp4/P4230041.MP4")
    return hash_tmp
  end
end

