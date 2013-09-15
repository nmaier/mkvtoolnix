#!/usr/bin/ruby -w

describe "mkvmerge / extract Opus from MKA in final mode"
test "extraction" do
  extract "data/opus/v-opus.mka", 0 => tmp
  hash_tmp
end
