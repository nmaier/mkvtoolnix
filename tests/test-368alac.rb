#!/usr/bin/ruby -w

# T_368alac
describe "mkvmerge / ALAC from CAF, MP4 and Matroska"

%w{othertest-itunes.m4a test-ffmpeg.caf}.each do |file_name|
  test_merge "data/alac/#{file_name}"
end

test_merge "data/alac/test-alacconvert.caf", :keep_tmp => true
test "reading from Matroska" do
  merge tmp, :output => "#{tmp}-2"
  hash_file "#{tmp}-2"
end
