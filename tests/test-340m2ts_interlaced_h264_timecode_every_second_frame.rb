#!/usr/bin/ruby -w

# T_340m2ts_interlaced_h264_timecode_every_second_frame
describe "mkvmerge / M2TS files with interlaced h264, timecodes for every second frame only"

test_merge "data/ts/interlaced_h264.ts"
