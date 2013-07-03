#!/usr/bin/ruby -w

describe "mkvmerge / mux Opus from Ogg in experimental mode"
test_merge "data/opus/v-opus.ogg", :exit_code => :warning
