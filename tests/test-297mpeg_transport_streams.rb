#!/usr/bin/ruby -w

class T_297mpeg_transport_streams < Test
  def description
    "MPEG transport streams"
  end

  def run
    %w{hd_distributor_regency.m2ts hd_other_sony_the_incredible_game.m2ts}.
      map { |file| merge "data/ts/#{file}", 1 ; hash_tmp }.
      join('-')
  end
end

