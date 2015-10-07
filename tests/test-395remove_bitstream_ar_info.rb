#!/usr/bin/ruby -w

# T_395remove_bitstream_ar_info
describe "mkvmerge / removing bitstream aspect ratio information from MP4/MKV"

file = "data/mp4/rain_800.mp4"
base = tmp

test_merge file, :args => "--engage remove_bitstream_ar_info"
test_merge file, :output => "#{base}-1", :keep_tmp => true
test_merge "#{base}-1", :output => "#{base}-2", :keep_tmp => true
test_merge "#{base}-1", :output => "#{base}-3", :keep_tmp => true, :args => "--engage remove_bitstream_ar_info"
