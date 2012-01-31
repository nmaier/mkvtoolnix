#!/usr/bin/ruby -w

describe "mkvmerge / h264 ES mis-detected as MP3"

test_identify "data/h264/bug574.h264"

