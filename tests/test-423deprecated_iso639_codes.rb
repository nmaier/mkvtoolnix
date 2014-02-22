#!/usr/bin/ruby -w

codes = {
  "iw"  => "heb",
  "scr" => "hrv",
  "scc" => "srp",
  "mol" => "rum",
}

describe "mkvmerge / deprecated ISO 639-1/2 codes"

codes.each do |deprecated_code, expected_code|
  test_merge "data/subtitles/srt/vde.srt", :args => "--language 0:#{deprecated_code}", :keep_tmp => true
  test "check code #{deprecated_code} => #{expected_code}" do
    output = identify tmp
    unlink_tmp_files
    /language:#{expected_code}/.match(output.join("")) ? "good" : "bad"
  end
end
