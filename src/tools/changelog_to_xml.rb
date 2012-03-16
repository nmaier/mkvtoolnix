#!/usr/bin/env ruby

require 'rubygems'
require 'pp'
gem 'builder', '~> 3.0.0'
require 'builder'

class Array
  def stable_sort_by &block
    index = 0
    sort_by { |e| index += 1; [ block.yield(e), index ] }
  end
end

fail "Missing input file name" if ARGV.empty?

changelog    = []
current_line = []
info         = nil
version      = 'HEAD'

IO.readlines(ARGV[0]).each do |line|
  line = line.chomp.gsub(/^\s+/, '').gsub(/\s+/, ' ')

  if line.empty?
    if !current_line.empty?
      version = $1 if / released? \s+ v? ( [\d\.]+ )/ix.match(current_line[0])

      changelog << info.dup.merge({ :content => current_line.join(' '), :version => version })
      current_line = []
    end

  elsif / ^ (\d+)-(\d+)-(\d+) \s+ (.+) /x.match(line)
    info   = { :date => sprintf("%04d-%02d-%02d", $1.to_i, $2.to_i, $3.to_i) }
    author = $4.gsub(/\s+/, ' ')
    if /(.+?) \s+ < (.+) >$/x.match(author)
      info[:author_name]  = $1
      info[:author_email] = $2
    else
      info[:author_name]  = author
    end

  else
    current_line << line.gsub(/^\*\s*/, '')

  end
end

changelog =
  changelog.
  stable_sort_by { |e| e[:date] }.
  reverse.
  chunk   { |e| e[:version] }.
  collect { |e| e }.
  stable_sort_by do |e|
    next [ 9, 9, 9, 9 ] if e[0] == 'HEAD'
    v = e[0].split(/\./).collect(&:to_i)
    v << 0 while v.length < 4
    v
  end.
  reverse

xml = Builder::XmlMarkup.new( :indent => 2 )
xml.instruct! :xml, :version => "1.0", :encoding => "UTF-8"

xml.releases do |releases|
  changelog.each do |e|
    releases.release(:version => e[0]) do |release|
      e[1].each do |entry|
        release.change(entry.reject { |key| key == :version })
      end
    end
  end
end

puts xml.to_s
