#!/usr/bin/perl -w

use strict;

use Data::Dumper;
use Getopt::Long;
use IO::File;
use String::ShellQuote;

sub error {
  my $message = shift;
  $message   .= "\n" if ("\n" ne substr($message, -1));

  print STDERR $message;

  exit 1;
}

sub gather_data {
  my $file_name = shift;
  my $file      = IO::File->new("mkvinfo --ui-language en_US.UTF-8 -s -v " . shell_quote($file_name) . " |");

  error("Could not run mkvinfo on '$file_name'.\n") if (!$file);

  my %type_map = ('I' => 0, 'P' => 1, 'B' => 2);
  my $count    = 0;
  my %data     = ();

  while (my $line = <$file>) {
    # P frame, track 1, timecode 501 (00:00:00.501), size 5724, adler 0xb1b79c2f, position 106331
    next unless ($line =~ m/^(.)\s+frame,\s+track\s+(\d+),\s+timecode\s+(\d+)\s.*?size\s+(\d+),.*?position\s+(\d+)/);

    $data{$2} ||= { map +($_ => []), qw(type timecode size position) };
    push @{ $data{$2}->{type}     }, $type_map{$1};
    push @{ $data{$2}->{timecode} }, $3;
    push @{ $data{$2}->{size}     }, $4;
    push @{ $data{$2}->{position} }, $5;

    $count++;
#     last if ($count > 30);
  }

  $file->close();

  return \%data;
}

$Data::Dumper::Sortkeys = 1;
my $data  = gather_data($ARGV[0]);
my $track = $data->{1};

for (my $i = 0; $i < scalar @{ $track->{timecode} }; $i++) {
  print "$track->{position}->[$i] $track->{timecode}->[$i]\n";
}
