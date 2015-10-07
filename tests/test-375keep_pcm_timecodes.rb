#!/usr/bin/ruby -w

describe "mkvmerge / PCM timecodes from Matroska not starting at 0"
test_merge "data/mkv/pcm-starting-at-1000ms.mkv"
