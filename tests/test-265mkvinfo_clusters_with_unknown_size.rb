#!/usr/bin/ruby -w

class T_265mkvinfo_clusters_with_unknown_size < Test
  def description
    return "mkvinfo / Clusters with an unknown size"
  end

  def run
    sys "../src/mkvinfo -v -v -z --ui-language en_US data/webm/live-stream.webm > #{tmp}"
    return hash_tmp
  end
end

