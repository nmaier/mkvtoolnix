#!/usr/bin/ruby -w

# underflow.mkv is incomplete. libmatroska leaves part of the very
# last block buffer uninitialized as it cannot read the whole
# buffer. This caused frequent failures in this test -- uninitialized
# memory is undefined behaviour. Therefore only use mkvinfo's summary
# output up to but excluding the very last line.

describe "mkvmerge / read buffer integer underflow on incomplete files"
test "data/mkv/underflow.mkv" do
  merge "data/mkv/underflow.mkv"
  info "-s #{tmp}", :output => "| sed -e '$d' > #{tmp}-1"
  hash_file "#{tmp}-1"
end
