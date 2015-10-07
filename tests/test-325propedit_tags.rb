#!/usr/bin/ruby -w

class T_325propedit_tags < Test
  def description
    "mkvpropedit / tags"
  end

  def _hash_chapters erase = false
    sys "../src/mkvextract --engage no_variable_data tags #{tmp} > #{tmp}-x"
    hash_x = hash_file("#{tmp}-x")
    [ hash_tmp(erase), hash_x ]
  end

  def run
    hashes = []
    sys "cp data/mkv/complex.mkv #{tmp}"
    hashes += _hash_chapters

    %w{tags.xml vmap.tags.xml}.each do |chapters|
      %w{all global track:1}.each do |selector|
        propedit "--tags #{selector}:data/text/#{chapters}"
        hashes += _hash_chapters
      end
    end

    propedit "--tags all:"
    hashes += _hash_chapters(true)

    hashes.join '-'
  end
end
