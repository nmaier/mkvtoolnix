#!/usr/bin/ruby -w

class T_297mpeg_transport_streams < Test
  def description
    "MPEG transport streams"
  end

  def run
    hashes = Array.new
    Dir.glob('data/ts/*.{m2ts,ts}').sort.each do |file|
      merge file, 1
      hashes << hash_tmp
    end

    hashes.join '-'
  end
end

