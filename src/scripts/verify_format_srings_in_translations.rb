#!/usr/bin/env ruby

require 'pp'

def read_entries file_name
  entries = Array.new

  IO.readlines(file_name).each_with_index do |line, line_number|
    line.gsub! /[\r\n]/, ''

    if /^msgid/.match line
      entries << { :id => line, :line_number => line_number + 1 }

    elsif /^msgstr/.match line
      entries[-1][:str] = line

    elsif /^"/.match line
      entry      = entries[-1]
      idx        = entry[:str] ? :str : :id
      entry[idx] = entry[idx][0 .. entry[idx].length - 2] + line[1 .. line.length - 1]
    end
  end

  entries
end

def process_file file_name
  matcher = /%(?:
                 %
               | \|[0-9$a-zA-Z\.\-]+\|
               | -?\.?\s?[0-9]*l?l?[a-zA-Z]
               | [0-9]+%?
               )/ix

  errors = read_entries(file_name).select { |entry| !entry[:id].nil? && !entry[:str].nil? && (entry[:str] != 'msgstr ""') }.collect do |entry|
    formats = Hash[ *[:id, :str].collect { |idx| [idx, entry[idx].scan(matcher).uniq.sort ] }.flatten(1) ]

    missing = formats[:id]  - formats[:str]
    added   = formats[:str] - formats[:id]

    next nil if missing.empty? && added.empty?

    { :line_number => entry[:line_number],
      :added       => added,
      :missing     => missing }
  end.compact

  errors.each do |error|
    messages = []
    messages << ("- " + error[:missing].join(' ')) if !error[:missing].empty?
    messages << ("+ " + error[:added  ].join(' ')) if !error[:added  ].empty?
    puts "#{file_name}:#{error[:line_number]}: error: #{messages.join('; ')}"
  end
end

ARGV.each { |file| process_file file }
