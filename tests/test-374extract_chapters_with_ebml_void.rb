#!/usr/bin/ruby -w

# T_374extract_chapters_with_ebml_void
describe "mkvextract / extract chapters with EbmlVoid elements"

test 'extraction' do
  output = extract("data/mkv/chapters-with-ebmlvoid.mkv", :mode => :chapters).join('')
  /ebml.?void/i.match(output) ? 'BAD' : 'ok'
end
