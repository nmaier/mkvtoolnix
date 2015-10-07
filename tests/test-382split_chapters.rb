#!/usr/bin/ruby -w

# T_382split_chapters
describe "mkvmerge / splitting by chapters"

avi      = "data/avi/v-h264-aac.avi"
chapters = "data/text/chapters-v-h264-aac.txt"

def hash_results max
  ( (1..max).collect { |i| hash_file(sprintf("%s-%02d", tmp, i)) } + [ File.exists?(sprintf("%s-%02d", tmp, max + 1)) ? 'bad' : 'ok' ]).join '+'
end

test "chapters-in-mkv: numbers 1 & 7" do
  merge "#{avi} --chapters #{chapters}", :output => "#{tmp}-src"
  merge "#{tmp}-src --split chapters:1,7", :output => "#{tmp}-%02d"
  hash_results 2
end

test "chapters-external: numbers 1 & 7" do
  merge "#{avi} --chapters #{chapters} --split chapters:1,7", :output => "#{tmp}-%02d"
  hash_results 2
end

test "chapters-in-mkv: all" do
  merge "#{avi} --chapters #{chapters}", :output => "#{tmp}-src"
  merge "#{tmp}-src --split chapters:all", :output => "#{tmp}-%02d"
  hash_results 17
end

test "chapters-external: all" do
  merge "#{avi} --chapters #{chapters} --split chapters:all", :output => "#{tmp}-%02d"
  hash_results 17
end
