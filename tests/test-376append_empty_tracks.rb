#!/usr/bin/ruby -w

# T_376append_empty_tracks
describe "mkvmerge / appending empty tracks"

[ %w{unsubbed unsubbed subbed},
  %w{subbed   unsubbed subbed},
  %w{unsubbed subbed   unsubbed subbed},
].each do |test_case|
  test_merge test_case.collect { |e| "data/mkv/#{e}.mkv" }.join(' + ')
end
