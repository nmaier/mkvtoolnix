#!/usr/bin/ruby -w

describe "mkvmerge / mux Opus from MKA in experimental mode"
test_merge "data/opus/v-opus-experimental.mka", :exit_code => :warning
