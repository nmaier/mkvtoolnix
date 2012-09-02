#!/usr/bin/ruby -w

# T_370propedit_attachments
describe "mkvpropedit / attachments"

sources = [
  { :file => "chap1.txt",      :name => "Dummy File.txt",           :mime_type => "text/plain",      :description => "Some funky description" },
  { :file => "chap2.txt",      :name => "Magic: The Gathering.ttf", :mime_type => "font/ttf",        :description => "This is a Font, mon!"   },
  { :file => "shortchaps.txt", :name => "Dummy File.txt",           :mime_type => "some:thing:else", :description => "Muh"                    },
  { :file => "tags.xml",       :name => "Hitme.xml",                :mime_type => "text/plain",      :description => "Gonzo"                  },
]

commands = [
  "--delete-attachment 2",
  "--delete-attachment =2",
  "--delete-attachment 'name:Magic: The Gathering.ttf'",
  "--delete-attachment mime-TYPE:text/plain",

  "--add-attachment data/text/tags-binary.xml",
  "--attachment-name 'Hallo! Die Welt....doc' --add-attachment data/text/tags-binary.xml",
  "--attachment-mime-type fun/go --add-attachment data/text/tags-binary.xml",
  "--attachment-description 'Alice and Bob see Charlie' --add-attachment data/text/tags-binary.xml",
  "--attachment-name 'Hallo! Die Welt....doc' --attachment-mime-type fun/go --attachment-description 'Alice and Bob see Charlie' --add-attachment data/text/tags-binary.xml",

  "--replace-attachment 3:data/text/tags-binary.xml",
  "--replace-attachment =3:data/text/tags-binary.xml",
  "--replace-attachment 'name:Magic\\c The Gathering.ttf:data/text/tags-binary.xml'",
  "--replace-attachment mime-type:text/plain:data/text/tags-binary.xml",

  "--attachment-name 'Hallo! Die Welt....doc' --attachment-mime-type fun/go --attachment-description 'Alice and Bob see Charlie' --replace-attachment 3:data/text/tags-binary.xml",
  "--attachment-name 'Hallo! Die Welt....doc' --attachment-mime-type fun/go --attachment-description 'Alice and Bob see Charlie' --replace-attachment =3:data/text/tags-binary.xml",
  "--attachment-name 'Hallo! Die Welt....doc' --attachment-mime-type fun/go --attachment-description 'Alice and Bob see Charlie' --replace-attachment 'name:Magic\\c The Gathering.ttf:data/text/tags-binary.xml'",
  "--attachment-name 'Hallo! Die Welt....doc' --attachment-mime-type fun/go --attachment-description 'Alice and Bob see Charlie' --replace-attachment mime-type:text/plain:data/text/tags-binary.xml",

  "--attachment-description 'Alice and Bob see Charlie' --replace-attachment mime-type:text/plain:data/text/tags-binary.xml",
]

test "several" do
  hashes  = []
  src     = "#{tmp}-src"
  work    = "#{tmp}-work"
  command = "data/srt/vde.srt " + sources.collect { |s| "--attachment-name '#{s[:name]}' --attachment-mime-type '#{s[:mime_type]}' --attachment-description '#{s[:description]}' --attach-file 'data/text/#{s[:file]}'" }.join(' ')

  merge command, :keep_tmp => true, :output => src
  hashes << hash_file(src)

  commands.each do |command|
    sys "cp #{src} #{work}"
    hashes << hash_file(work)
    propedit "#{work} #{command}"
    hashes << hash_file(work)
  end

  hashes.join '-'
end
