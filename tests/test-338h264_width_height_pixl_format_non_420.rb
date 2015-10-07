#!/usr/bin/ruby -w

# T_338h264_width_height_pixl_format_non_420
describe "mkvmerge / h.264 width/height calculations for non-4:2:0 pixel formats"

test_merge "data/h264/profile_high_444_predictive_@l3.1.h264", :keep_tmp => true
test "verify resolution is 640x360" do
  sys "../src/mkvinfo --ui-language en_US -s #{tmp} |  head -n 1 | grep -q 'pixel width: 640, pixel height: 360'"
  :ok
end
