#!/usr/bin/ruby -w

class T_222stereo_mode < Test
  def description
    return "mkvmerge / Stereo mode flag for video tracks / in(AVI)"
  end

  def run
    merge(tmp + "-1",
           "-A --stereo-mode 0:mono data/avi/v.avi " +
           "-A --stereo-mode 0:side_by_side_left_first data/avi/v.avi " +
           "-A --stereo-mode 0:top_bottom_right_first data/avi/v.avi " +
           "-A --stereo-mode 0:top_bottom_left_first data/avi/v.avi ")
    if (!FileTest.exist?(tmp + "-1"))
      error("First merge failed")
    end

    hash = hash_file(tmp + "-1")

    merge(tmp + "-2",
           "--stereo-mode 4:mono --stereo-mode 3:side_by_side_left_first " +
           "--stereo-mode 2:top_bottom_right_first --stereo-mode 1:top_bottom_left_first #{tmp}-1")
    File.unlink(tmp + "-1")
    if (!FileTest.exist?(tmp + "-2"))
      error("Second merge failed")
    end

    hash += "-" + hash_file(tmp + "-2")

    merge(tmp + "-3", tmp + "-2")
    File.unlink(tmp + "-2")
    if (!FileTest.exist?(tmp + "-3"))
      error("Third merge failed")
    end

    hash += "-" + hash_file(tmp + "-3")
    File.unlink(tmp + "-3")

    return hash
  end
end

