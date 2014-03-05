#!/usr/bin/ruby -w

# T_425mpeg_ts_timestamp_outlier
describe "mkvmerge / MPEG TS ignore timestamp outliers"
test_merge "data/ts/pts_outlier.ts"
