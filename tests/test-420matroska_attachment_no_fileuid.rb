#!/usr/bin/ruby -w

# T_420matroska_attachment_no_fileuid
describe "mkvmerge & mkvextract / Matroska files with attachments missing a FileUID"

file = "data/mkv/attachment-without-fileuid.mkv"

test_merge file

test "extraction" do
  extract file, :mode => :attachments, 1 => tmp
  hash_tmp
end

test "propedit" do
  sys "cp #{file} #{tmp}"
  propedit "#{tmp} --replace-attachment '=0:data/text/chap1.txt'"
  hash_tmp
end
