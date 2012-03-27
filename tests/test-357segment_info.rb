#!/usr/bin/ruby -w

# T_357segment_info
describe "mkvmerge / segment info XML"

source  = "data/srt/ven.srt"
invalid = Dir["data/text/segment-info-invalid-*.xml"]

# Valid files:
(Dir["data/text/segment-info-*.xml"] - invalid).sort.each do |segment_info|
  test_merge "#{source} --segmentinfo #{segment_info}"
end

# Invalid files:
invalid.sort.each do |segment_info|
  test segment_info do
    merge "#{source} --segmentinfo #{segment_info}", :exit_code => 2
    :ok
  end
end
