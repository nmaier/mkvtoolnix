#!/usr/bin/ruby -w

# T_357segment_info
describe "mkvmerge / segment info XML"

source = "data/srt/ven.srt"

# Valid files:
%w{segment-info-as-tags.xml segment-info-as-attributes.xml}.each do |segment_info|
  test_merge "#{source} --segmentinfo data/text/#{segment_info}"
end

# Invalid files:
%w{segment-info-invalid-sub-tag.xml segment-info-invalid-root-tag.xml segment-info-invalid-xml.xml}.each do |segment_info|
  test segment_info do
    merge "#{source} --segmentinfo data/text/#{segment_info}", :exit_code => 2
    :ok
  end
end
