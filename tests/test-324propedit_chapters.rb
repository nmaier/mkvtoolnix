#!/usr/bin/ruby -w

class T_324propedit_chapters < Test
  def description
    "mkvpropedit / chapters"
  end

  def _hash_chapters erase = false
    sys "../src/mkvextract --engage no_variable_data chapters #{tmp} > #{tmp}-x"
    hash_x = hash_file("#{tmp}-x")
    [ hash_tmp(erase), hash_x ]
  end

  def run
    hashes = []
    sys "cp data/mkv/complex.mkv #{tmp}"
    hashes += _hash_chapters

    %w{chap1.txt chap2.txt vmap.chapters.xml}.each do |chapters|
      propedit "--chapters data/text/#{chapters}"
      hashes += _hash_chapters
    end

    propedit "--chapters ''"
    hashes += _hash_chapters(true)

    hashes.join '-'
  end
end

