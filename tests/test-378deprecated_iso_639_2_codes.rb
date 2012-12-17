#!/usr/bin/ruby -w

the_file = "data/mkv/deprecated-languages.mkv"

# T_378deprecated_iso_639_2_codes
describe "mkvmerge / handling deprecated ISO 639-2 language codes"
test "identification" do
  identify(the_file).select { |e| /language:/.match(e) }.collect { |e| e.chomp.gsub(/.*language:(\w+).*/, '\1') }.compact.sort.join('+')
end

test_merge the_file
