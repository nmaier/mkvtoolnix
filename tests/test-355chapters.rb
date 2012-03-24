#!/usr/bin/ruby -w

# T_355chapters
describe "mkvmerge / XML chapters"

source = "data/srt/ven.srt"

# Valid files:
%w{chapters-as-tags.xml chapters-as-attributes.xml chapters-iso-8859-15.xml}.each do |chapters|
  test_merge "#{source} --chapters data/text/#{chapters}"
end

# Invalid files:
%w{chapters-invalid-timecode.xml chapters-invalid-sub-tag.xml chapters-invalid-root-tag.xml chapters-invalid-xml.xml chapters-invalid-range.xml}.each do |chapters|
  test chapters do
    merge "#{source} --chapters data/text/#{chapters}", :exit_code => 2
    :ok
  end
end
