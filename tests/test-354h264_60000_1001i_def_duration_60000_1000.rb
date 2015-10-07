#!/usr/bin/ruby -w

# T_354h264_60000_1001i_def_duration_60000_1000
describe "mkvmerge / h264 default duration set to 60 FPS while input signals 60000/1001i"

test_merge "data/h264/60000_1001i.h264"
