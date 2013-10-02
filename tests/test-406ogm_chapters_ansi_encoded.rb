#!/usr/bin/ruby -w

file = "data/ogg/with_chapters_ansi_encoded.ogm"

describe "mkvmerge / identify and merge OGMs with comments encoded in ANSI"

test_identify file
test_merge file, :exit_code => :warning
test_merge file, :args => "--chapter-charset MS-ANSI"
