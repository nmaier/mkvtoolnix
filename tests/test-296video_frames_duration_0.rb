#!/usr/bin/ruby -w

class T_296video_frames_duration_0 < Test
  def description
    "mkvmerge / Matroska files, block group with explicit duration 0"
  end

  def run
    merge "data/webm/out_video.webm"
    hash_tmp
  end
end

