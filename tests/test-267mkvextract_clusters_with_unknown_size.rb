#!/usr/bin/ruby -w

class T_267mkvextract_clusters_with_unknown_size < Test
  def description
    return "mkvextract / Clusters with an unknown size"
  end

  def run
    xtr_tracks "data/webm/live-stream.webm", "1:#{tmp}"
    return hash_tmp
  end
end

