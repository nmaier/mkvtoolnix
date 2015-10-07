#!/usr/bin/ruby -w

# T_348srt_negative_timecodes2
describe "mkvmerge / SRT subitles with negative timecodes"

test_merge "data/subtitles/srt/negative_timecodes2.srt", :exit_code => 1
