#!/usr/bin/ruby -w

# T_418ac3_frame_size_0
describe "mkvmerge / AC3 parsing getting stuck in endless loops due to frame size == 0"
test_merge "data/ts/garbage-ac3-frame-size-0.ts", :exit_code => :warning
