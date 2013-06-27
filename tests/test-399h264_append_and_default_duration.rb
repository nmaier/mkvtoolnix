#!/usr/bin/ruby -w

# T_399h264_append_and_default_duration
describe "mkvmerge / appending AVC/h.264 raw and setting the default duration"
test_merge "--default-duration 0:24000/1001fps data/h264/append-test/1.264 + data/h264/append-test/2.264"
