#!/usr/bin/ruby -w

# T_417mkvextract_tracks_at_end_of_file
describe "mkvextract / track headers located at the end of the file"
test "extraction" do
  extract "data/mkv/tracks-at-end.mka", 0 => tmp
  hash_tmp
end
