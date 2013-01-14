#!/usr/bin/ruby -w

# T_385split_parts_frames
describe "mkvmerge / --split parts-frames:..."

test "data/avi/v-h264-aac.avi two output files" do
  merge "--split parts-frames:480-720,-960 data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-01"), hash_file("#{tmp}-02"), File.exist?("#{tmp}-03") ? "bad" : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi one output file" do
  merge "--split parts-frames:480-720,+960-1198 data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi one output file, starting at 0" do
  merge "--split parts-frames:-720,+960-1198 data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi three parts one output file, starting at 0" do
  merge "--split parts-frames:-240,+480-720,+960-1198 data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi three parts one output file, starting at 240" do
  merge "--split parts-frames:240-480,+720-960,+1198-1440 data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end
