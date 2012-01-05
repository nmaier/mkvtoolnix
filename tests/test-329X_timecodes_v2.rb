#!/usr/bin/ruby -w

class T_329X_timecodes_v2 < Test
  def description
    "mkvextract / timecodes_v2"
  end

  def run
    (1..10).collect do |track_id|
      sys "../src/mkvextract timecodes_v2 data/mkv/complex.mkv #{track_id}:#{tmp}"
      hash_tmp
    end.join('-')
  end
end

