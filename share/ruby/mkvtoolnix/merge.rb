#!/usr/bin/env ruby

require "pp"

module MKVToolNix
  module Merge
    EXIT_STATUS_MAP = {
      0 => :success,
      1 => :warnings,
      2 => :errors,
    }

    def self.identify file_name, opt = {}
      opt[:executable] ||= 'mkvmerge'

      p = IO.popen([ opt[:executable], "--identify-for-mmg", "--output-charset", "UTF-8", file_name ])
      output = p.readlines
      p.close

      result = {
        :status       => EXIT_STATUS_MAP[ $?.exitstatus ] || :unknown,
        :exitstatus   => $?.exitstatus,
        :container    => { },
        :tracks       => [ ],
        :attachments  => [ ],
        :num_chapters => 0,
        :num_tags     => 0,
      }

      return result if result[:status] != :success

      tracks_by_id   = { }
      num_tags_by_id = { }

      output.each do |line|
        line.chomp!

        if /^File.+:.+container: +([^ ]+)/.match line
          result[:container]  = self.extract_properties(line).merge(:type => $1)

        elsif /^Track ID (\d): +([a-z\-]+) +\((.+?)\)/.match line
          track = self.extract_properties(line).merge(
            :mkvmerge_id => $1.to_i,
            :type        => $2.to_sym,
            :codec       => $3,
            :num_tags    => 0,
          )

          result[:tracks] << track
          tracks_by_id[ track[:mkvmerge_id] ] = track

        elsif /^Attachment +ID +(\d+): +type +["'](.+)["'], +size +(\d+) +bytes?, +file +name +["'](.+)["']$/.match line
         # Attachment ID 1: type 'text/plain', size 1202 bytes, file name 'ven.srt'
          result[:attachments] << {
            :mkvmerge_id => $1.to_i,
            :type        => $2,
            :size        => $3.to_i,
            :file_name   => $4,
          }

        elsif /^Global +tags: +(\d+)/.match line
          # Global tags: 4 entries
          result[:num_tags] = $1.to_i

        elsif /^Chapters: +(\d+)/.match line
          # Chapters: 5 entries
          result[:num_chapters] = $1.to_i

        elsif /^Tags +for +track +ID +(\d+): +(\d+)/.match line
          # Tags for track ID 1: 2 entries
          num_tags_by_id[$1.to_i]  = $2.to_i

        end
      end

      num_tags_by_id.each do |id, num|
        tracks_by_id[id][:num_tags] = num if tracks_by_id.has_key? id
      end

      return result
    end

    protected

    def self.extract_properties line
      return { } unless /.+\[ ( .+ ) \]/x.match line
      return Hash[ *
        $1
          .chomp
          .split(/ +/)
          .collect { |part| sub_parts = part.split(/:/).collect { |sub_part| MKVToolNix::Common.unescape sub_part } ; [ sub_parts[0].to_sym, sub_parts[1] ] }
          .flatten
      ]
    end
  end
end
