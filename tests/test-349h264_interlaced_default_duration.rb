#!/usr/bin/ruby -w

# T_349h264_interlaced_default_duration
describe "mkvmerge / interlaced h264 & handling of default duration"

test_merge "data/h264/interlaced-50i.h264"
test_merge "--default-duration 0:50i data/h264/interlaced-50i.h264"
test_merge "--default-duration 0:25p data/h264/interlaced-50i.h264"
test_merge "--default-duration 0:30fps data/h264/interlaced-50i.h264"
