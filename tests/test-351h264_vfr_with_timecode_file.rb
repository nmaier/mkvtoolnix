#!/usr/bin/ruby -w

# T_351h264_vfr_with_timecode_file
describe "mkvmerge / VFR h264 file with and without a timecode file"

test_merge "data/h264/vfr.h264"
test_merge "data/h264/vfr.h264", :args => "--timecodes 0:data/h264/vfr-timecodes.txt"
