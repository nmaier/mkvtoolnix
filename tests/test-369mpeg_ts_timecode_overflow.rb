#!/usr/bin/ruby -w

# T_369mpeg_ts_timecode_overflow
describe "mkvmerge / MPEG TS with overflowing timecodes"

test_merge "data/ts/timecode-overflow.m2ts"
