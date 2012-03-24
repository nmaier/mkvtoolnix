#!/usr/bin/ruby -w

# T_356tags
describe "mkvmerge / XML tags"

source = "data/srt/ven.srt"

# Valid files:
%w{tags-as-tags.xml tags-as-attributes.xml tags-iso-8859-15.xml tags-binary.xml}.each do |tags|
  test_merge "--tags 0:data/text/#{tags} #{source}"
end

# Invalid files:
%w{tags-invalid-string-and-binary.xml tags-invalid-sub-tag.xml tags-invalid-root-tag.xml tags-invalid-xml.xml tags-invalid-range.xml
   tags-invalid-binary-format.xml tags-invalid-binary-base64.xml tags-invalid-binary-hex.xml}.each do |tags|
  test tags do
    merge "--tags 0:data/text/#{tags} #{source}", :exit_code => 2
    :ok
  end
end
