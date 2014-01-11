#!/usr/bin/ruby -w

# T_420matroska_attachment_no_fileuid
describe "mkvmerge / Matroska files with attachments missing a FileUID"
test_merge "data/mkv/attachment-without-fileuid.mkv"
