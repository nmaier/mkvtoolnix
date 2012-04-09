#!/usr/bin/ruby -w

# T_359split_parts
describe "mkvmerge / --split parts:..."

test "data/avi/v-h264-aac.avi two output files" do
  merge "--split parts:20s-30s,-40s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-01"), hash_file("#{tmp}-02"), File.exist?("#{tmp}-03") ? "bad" : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi one output file" do
  merge "--split parts:20s-30s,+40s-50s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi one output file, starting at 00:00:00" do
  merge "--split parts:-30s,+40s-50s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "split timecodes + appending vs split parts" do
  result = []
  source = "data/mp4/10-DanseMacabreOp.40.m4a"

  merge "--disable-lacing --split timecodes:01:21,01:52,03:07,03:51 #{source}", :output => "#{tmp}-%02d"
  result += (1..5).collect { |idx| hash_file "#{tmp}-0#{idx}" }

  merge "--disable-lacing #{tmp}-02 + #{tmp}-04", :output => "#{tmp}-appended"
  result << hash_file("#{tmp}-appended")

  output_appended = info "-s #{tmp}-appended", :output => :return

  merge "--disable-lacing --split parts:01:21-01:52,+03:07-03:51 #{source}", :output => "#{tmp}-parts"
  result << hash_file("#{tmp}-parts")

  output_parts = info "-s #{tmp}-parts", :output => :return

  fail "summary is different" if output_appended != output_parts

  result << "ok"

  unlink_tmp_files

  result.join '-'
end

test "data/avi/v-h264-aac.avi three parts one output file, starting at 00:00:00" do
  merge "--split parts:-10s,+20s-30s,+40s-50s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end

test "data/avi/v-h264-aac.avi three parts one output file, starting at 00:00:10" do
  merge "--split parts:10s-20s,+30s-40s,+50s-60s data/avi/v-h264-aac.avi", :output => "#{tmp}-%02d"
  result = [ hash_file("#{tmp}-%02d"), File.exist?("#{tmp}-02") ? fail("second file should not exist") : "ok" ].join('+')
  unlink_tmp_files
  result
end
