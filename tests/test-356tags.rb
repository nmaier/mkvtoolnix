#!/usr/bin/ruby -w

# T_356tags
describe "mkvmerge / XML tags"

source = "data/srt/ven.srt"

# Valid files:
%w{tags-as-tags.xml tags-as-attributes.xml tags-iso-8859-15.xml tags-binary.xml}.each do |tags|
  test_merge "--tags 0:data/text/#{tags} #{source}"
end

# Invalid files:
Dir["data/text/tags-invalid-*.xml"].sort.each do |tags|
  test tags do
    messages = merge "--tags 0:#{tags} #{source}", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+tag\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
