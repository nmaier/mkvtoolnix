#!/usr/bin/ruby -w
# -*- coding: utf-8 -*-

# T_426extract_write_bom_only_once
file = "data/mkv/complex.mkv"

describe "mkvextract / write BOMs only once with --redirect-output"
test "extraction via shell redirection" do
  extract(file, :mode => :chapters).
    slice(0..-2).
    join('').
    md5
end

test "extraction via --redirect-output" do
  sys "../src/mkvextract --engage no_variable_data chapters #{file} --redirect-output #{tmp} &> /dev/null"
  hash_tmp
end
