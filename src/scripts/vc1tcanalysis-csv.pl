#!/usr/bin/perl -w

use Text::CSV_XS;

use strict 'vars';

if (scalar @ARGV < 2) {
  print "Need two file names\n";
  exit 1;
}

my @frames;

open(IN, $ARGV[0]) || die "$@";
while (<IN>) {
  next unless (m/ frame, track /);
  my @f = split m/\s+/, $_;
  push @frames, { 'frame_num' => scalar(@frames) + 1, 'eac3_tc' => $f[5] };
}
close IN;

open(IN, $ARGV[1]) || die "$@";
my $idx      = -1;
my $in_frame = 0;
while (<IN>) {
  if (/^Frame/) {
    $idx++;
    $in_frame = 1;

  } elsif (/^[A-Z]/) {
    $in_frame = 0;

  } elsif ($in_frame) {
    if (m/fcm:\s+\d+\s+\((.*)\)/) {
      $frames[$idx]->{fcm} = $1;

    } elsif (m/frame_type:\s+([A-Z].*)/) {
      $frames[$idx]->{frame_type} = $1;

    } elsif (m/repeat_frame:\s+(\d+)/) {
      $frames[$idx]->{rf} = $1;

    } elsif (m/top_field_first_flag:\s+(\d+)/) {
      $frames[$idx]->{tff} = $1;

    } elsif (m/repeat_first_field_flag:\s+(\d+)/) {
      $frames[$idx]->{rff} = $1;

    }
  }
}
close IN;

my $csv = Text::CSV_XS->new();
my @columns = qw(frame_num eac3_tc frame_type fcm rff rf tff);
$csv->combine(@columns);
print $csv->string(), "\n";
foreach my $frame (@frames) {
  $csv->combine(map { $frame->{$_} } @columns);
  print $csv->string(), "\n";
}
