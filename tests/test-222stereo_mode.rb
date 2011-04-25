#!/usr/bin/ruby -w

class T_222stereo_mode < Test
  def description
    "mkvmerge / Stereo mode flag for video tracks / in(AVI)"
  end

  def run
    merge tmp + "-1",
           "-A --stereo-mode 0:mono data/avi/v.avi " +
           "-A --stereo-mode 0:side_by_side_left_first data/avi/v.avi " +
           "-A --stereo-mode 0:top_bottom_right_first data/avi/v.avi " +
           "-A --stereo-mode 0:top_bottom_left_first data/avi/v.avi "
    error "First merge failed" if !FileTest.exist? tmp + "-1"

    hash = [ hash_file(tmp + "-1") ]

    merge tmp + "-2",
           "--stereo-mode 4:mono --stereo-mode 3:side_by_side_left_first " +
           "--stereo-mode 2:top_bottom_right_first --stereo-mode 1:top_bottom_left_first #{tmp}-1"
    error "Second merge failed" if !FileTest.exist? tmp + "-2"

    hash << hash_file(tmp + "-2")

    merge tmp + "-3", tmp + "-2"
    error "Third merge failed" if !FileTest.exist? tmp + "-3"

    hash << hash_file(tmp + "-3")

    merge tmp + "-4", "-A --stereo-mode 0:mono data/avi/v.avi"
    error "Fourth merge failed" if !FileTest.exist? tmp + "-4"

    hash << hash_file(tmp + "-4")

    hash.join"-"
  end
end

