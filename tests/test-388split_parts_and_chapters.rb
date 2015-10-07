#!/usr/bin/ruby -w

args = "data/avi/v-h264-aac.avi --chapters data/text/chapters-v-h264-aac-i-frames.txt"

# T_388split_parts_and_chapters
describe "mkvmerge / splitting by parts and chapters"
test_merge "#{args} --split parts-frames:1-140"
test_merge "#{args} --split parts-frames:141-209,+440-658,+1362-"
test_merge "#{args} --split parts-frames:1362-"
