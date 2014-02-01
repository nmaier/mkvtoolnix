#!/usr/bin/env ruby

require 'digest/sha1'

class FormatStringVerifier
  @@errors_to_ignore = {
    "d2820b11" => true,
    "f3230d7f" => true,
  }

  def read_entries file_name
    entries = Array.new
    flags   = {}

    IO.readlines(file_name).each_with_index do |line, line_number|
      line.gsub! /[\r\n]/, ''

      if /^msgid /.match line
        entries << { :id => [ line ], :line_number => line_number + 1, :flags => flags }
        flags = {}

      elsif /^msgid_plural /.match line
        entries[-1][:id] << line

      elsif /^msgstr/.match line
        entries[-1][:str] ||= Array.new
        entries[-1][:str]  << line

      elsif /^"/.match line
        entry      = entries[-1]
        str        = entry[:str] ? entry[:str][-1] : entry[:id][-1]
        str.gsub! /"$/, ''
        str << line[1 .. line.length - 1]

      elsif /^#,/.match line
        flags = Hash[ *line.gsub(/^#,\s*/, '').split(/,\s*/).map { |flag| [ flag.to_sym, true ] }.flatten ]
      end
    end

    entries
  end

  def verify file_name
    matcher = /%(?:
                 %
               | \|[0-9$a-zA-Z\.\-]+\|
               | -?\.?\s?[0-9]*l?l?[a-zA-Z]
               | [0-9]+%?
               )/ix

    errors = read_entries(file_name).select { |e| !e[:flags][:fuzzy] && !e[:id].nil? && (e[:id][0] != 'msgid ""') && !e[:str].nil? && e[:str].detect { |e| e != 'msgstr ""' } }.collect do |entry|
      non_id  = entry[:id][ 1 .. entry[:id].size - 1 ] + entry[:str]
      formats = {
        :id     => entry[:id][0].scan(matcher).uniq.sort,
        :non_id => non_id.collect { |e| e.scan(matcher) }.flatten.uniq.sort
      }

      missing = formats[:id]     - formats[:non_id]
      added   = formats[:non_id] - formats[:id]

      next nil if missing.empty? && added.empty?

      digest = Digest::SHA1.new.update(([ file_name.gsub(/.*\//, ''), entry[:id][-1] ] + added + missing).join(':')).hexdigest[0..7]
      next nil if @@errors_to_ignore[digest]

      { :line_number => entry[:line_number],
        :added       => added,
        :missing     => missing,
        :digest      => digest
      }
    end.compact

    errors.each do |error|
      messages = []
      messages << ("- " + error[:missing].join(' ')) if !error[:missing].empty?
      messages << ("+ " + error[:added  ].join(' ')) if !error[:added  ].empty?
      puts "#{file_name}:#{error[:line_number]}: error: #{messages.join('; ')} (id: #{error[:digest]})"
    end

    return errors.empty?
  end
end
