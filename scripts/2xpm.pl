#!/usr/bin/perl

die "usage $0: BITMAP_FILE" unless @ARGV;

$TRANSCOLOR = "magenta";

foreach ( @ARGV ) {
	$old = $_;
	die "$old doesn't have a .bmp extension!\n" unless s/\.bmp$/.xpm/;
	rename $old, $_;
#Set transparencies
	$cmdline = "convert -transparent $TRANSCOLOR $_ $_";
	system($cmdline);
}
