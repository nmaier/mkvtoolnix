#!/usr/bin/ruby -w

class T_270ivf < Test
  def description
    return "mkvmerge / IVF with VP8"
  end

  def run
    checksums = []
    %w{live-stream sample yt3}.each do |file|
      merge "data/webm/#{file}.ivf"
      checksums << hash_tmp
    end

    return checksums.join '-'
  end
end

