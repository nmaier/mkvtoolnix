#!/usr/bin/ruby -w

describe "mkvmerge / WAVPACK4 with correction file"

test_merge "data/wavpack4/v.wv", :keep_tmp => true

test "extract with correction file" do
  extract tmp, 0 => "#{tmp}-0.wv"
  hash_file("#{tmp}-0.wv") + "+" + hash_file("#{tmp}-0.wvc")
end

test "look for SimpleBlock" do
  sys "../src/mkvinfo #{tmp} | grep -q SimpleBlock", :exit_code => 1
  "ok"
end

