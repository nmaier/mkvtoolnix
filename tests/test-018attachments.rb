#!/usr/bin/ruby -w

class T_018attachments < Test
  def description
    return "mkvmerge / attachments / in(*)"
  end

  def run
    merge("data/vde.srt --attachment-description 'Dummy description' " +
           "--attachment-mime-type text/plain --attach-file data/vde.srt")
    hash = hash_tmp
    merge("data/vde.srt --attachment-description 'automatic MIME type test' " +
           "--attach-file data/cuewithtags2.cue")
    return hash + "-" + hash_tmp
  end
end

