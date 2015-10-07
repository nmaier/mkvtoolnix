#!/usr/bin/ruby -w

# T_362xtr_avc
describe "mkvextract / AvC/h.264 extraction"

test_merge "data/h264/interlaced-50i.h264", :keep_tmp => true
test "extraction" do
  extract tmp, 0 => "#{tmp}-x"
  hash_file "#{tmp}-x"
end
