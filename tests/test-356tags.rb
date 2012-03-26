#!/usr/bin/ruby -w

# T_356tags
describe "mkvmerge / XML tags"

source = "data/srt/ven.srt"

# Valid files:
%w{tags-as-tags.xml tags-as-attributes.xml tags-iso-8859-15.xml tags-binary.xml}.each do |tags|
  test_merge "--tags 0:data/text/#{tags} #{source}"
end

# Invalid files:
%w{string-and-binary sub-tag root-tag xml range binary-format binary-base64 binary-hex binary-and-string no-binary-no-string no-name no-simple string-and-binary}.each do |tags|
  test tags do
    messages = merge "--tags 0:data/text/tags-invalid-#{tags}.xml #{source}", :exit_code => 2
    messages.detect { |line| /The\s+XML\s+tag\s+file.*contains\s+an\s+error/i.match line } ? :ok : :bad
  end
end
