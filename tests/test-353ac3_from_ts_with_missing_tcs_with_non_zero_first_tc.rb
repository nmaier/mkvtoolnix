#!/usr/bin/ruby -w

# T_353ac3_from_ts_with_missing_tcs_with_non_zero_first_tc
describe "mkvmerge / MPEG-TS with AC3, TS provides few timecodes, first TC is non-zero"

test_merge "--no-video data/ts/avc-ac3-with-missing-tc-and-non-zero-first-tc.ts"
