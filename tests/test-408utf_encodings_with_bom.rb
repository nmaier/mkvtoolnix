#!/usr/bin/ruby -w

# T_408utf_encodings_with_bom
describe "mkvmerge / read different UTF encodings with BOM"

types = %w{8 16le 16be 32le 32be}
types.each { |type| test_merge "data/subtitles/srt/bom/vde-utf#{type}.srt" }
