#!/usr/bin/ruby -w

# T_397mpeg_ts_broken_pes_track_detection
describe "mkvmerge / MPEG TS broken PES packets, track detection issues"
test_merge "data/ts/ac3-track-broken-pes-detection.ts"
