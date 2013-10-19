#!/usr/bin/ruby -w

# T_360X_chapters_hex_format
describe "mkvextract / extract XML chapters with binary elements (hex format)"

test_merge "data/subtitles/srt/ven.srt --chapters data/text/chapters-hex-format.xml", :keep_tmp => true
test "extraction" do
  extract "#{tmp} > #{tmp}-chapters", :mode => :chapters
  hash_file "#{tmp}-chapters"
end
