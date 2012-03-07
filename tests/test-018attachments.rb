#!/usr/bin/ruby -w

class T_018attachments < Test
  def description
    return "mkvmerge / attachments / in(*)"
  end

  def run
    merge("--sub-charset 0:ISO-8859-1 data/srt/vde.srt " +
           "--attachment-description 'Dummy " +
           "description' --attachment-mime-type text/plain --attach-file " +
           "data/srt/vde.srt")
    hash = hash_tmp
    merge("--sub-charset 0:ISO-8859-1 data/srt/vde.srt " +
           "--attachment-description 'automatic MIME " +
           "type test' --attach-file data/text/cuewithtags2.cue")
    return hash + "-" + hash_tmp
  end
end

