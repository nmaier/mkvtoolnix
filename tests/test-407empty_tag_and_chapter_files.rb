#!/usr/bin/ruby -w

# T_407empty_tag_and_chapter_files
describe "mkvmerge / empty tag and chapter files"

file = "data/srt/ven.srt"

test_merge file, :args => "--global-tags data/text/tags-empty.xml"
test_merge file, :args => "--tags 0:data/text/tags-empty.xml"
test_merge file, :args => "--chapters data/text/chapters-empty.xml"
