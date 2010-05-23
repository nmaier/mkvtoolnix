#!/usr/bin/ruby -w

class T_266mkvmerge_clusters_with_unknown_size < Test
  def description
    return "mkvmerge / Clusters with an unknown size"
  end

  def run
    merge "data/webm/live-stream.webm"
    return hash_tmp
  end
end

