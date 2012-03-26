#!/usr/bin/ruby -w

# T_355chapters
describe "mkvmerge / XML chapters"

source = "data/srt/ven.srt"

# Valid files:
%w{chapters-as-tags.xml chapters-as-attributes.xml chapters-iso-8859-15.xml}.each do |chapters|
  test_merge "#{source} --chapters data/text/#{chapters}"
end

# Invalid files:
Dir["data/text/chapters-invalid-*.xml"].sort.each do |chapters|
  test chapters do
    messages = merge "#{source} --chapters #{chapters}", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+chapter\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
