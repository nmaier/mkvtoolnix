#!/usr/bin/ruby -w

# T_363srt_colon_decimal_separator
describe "mkvmerge / SRT subtitles with colons as the decimal separator"
test_merge "data/subtitles/srt/colon_as_decimal_separator.srt"
