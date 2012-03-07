#!/usr/bin/ruby -w

# T_350h264_progressive_default_duration
describe "mkvmerge / progressive h264 & handling of default duration"

test_merge "data/h264/progressive-23.976p.h264"
test_merge "--default-duration 0:23.976p data/h264/progressive-23.976p.h264"
test_merge "--default-duration 0:30fps data/h264/progressive-23.976p.h264"
