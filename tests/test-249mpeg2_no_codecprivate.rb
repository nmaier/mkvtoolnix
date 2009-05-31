#!/usr/bin/ruby -w

class T_249mpeg2_no_codecprivate < Test
  def description
    return "mkvmerge / MPEG-1/2 video sequence header extraction for missing CodecPrivate / in(MKV)"
  end

  def run
    merge("data/mpeg12/mpeg2_no_codecprivate.mkv")
    return hash_tmp
  end
end

