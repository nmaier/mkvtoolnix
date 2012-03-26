#!/usr/bin/ruby -w

# T_355chapters
describe "mkvmerge / XML chapters"

source = "data/srt/ven.srt"

# Valid files:
%w{chapters-as-tags.xml chapters-as-attributes.xml chapters-iso-8859-15.xml}.each do |chapters|
  test_merge "#{source} --chapters data/text/#{chapters}"
end

# Invalid files:
%w{timecode sub-tag root-tag xml range chaptercountry chapterlanguage chapter-track-no-chaptertracknumber no-atom no-chapterstring no-chaptertimestart}.each do |chapters|
  test chapters do
    messages = merge "#{source} --chapters data/text/chapters-invalid-#{chapters}.xml", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+chapter\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
