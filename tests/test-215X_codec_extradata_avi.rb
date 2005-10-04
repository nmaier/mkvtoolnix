#!/usr/bin/ruby -w

class T_215X_codec_extradata_avi < Test
  def description
    return "mkvextract / codec extradata after BITMAPINFOHEADER"
  end

  def run
    merge("data/avi/extradata.avi")
    hash = hash_file(tmp) + "-"
    xtr_tracks(tmp, "1:#{tmp}-x.avi")
    hash += hash_file("#{tmp}-x.avi")
    unlink_tmp_files
    return hash
  end
end

