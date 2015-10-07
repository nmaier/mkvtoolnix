#!/usr/bin/ruby -w

class T_019attachments2 < Test
  def description
    return "mkvmerge / attachments and splitting / in(*)"
  end

  def run
    merge(tmp + "-%03d", "data/avi/v.avi --split-max-files 2 --split 4m " +
           "--attachment-description dummy --attachment-mime-type " +
           "text/plain --attach-file data/subtitles/srt/vde.srt")
    hash = hash_file(tmp + "-001") + "-" + hash_file(tmp + "-002")
    File.unlink(tmp + "-001")
    File.unlink(tmp + "-002")
    merge(tmp + "-%03d", "data/avi/v.avi --split-max-files 2 --split 4m " +
           "--attachment-description dummy --attachment-mime-type " +
           "text/plain --attach-file-once data/subtitles/srt/vde.srt")
    hash += "-" + hash_file(tmp + "-001") + "-" + hash_file(tmp + "-002")
    File.unlink(tmp + "-001")
    File.unlink(tmp + "-002")

    return hash
  end
end

