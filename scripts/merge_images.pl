#!/usr/bin/perl

#Author: David Lawrence - philaw@ozemail.com.au
#Merges the images together to make a composite image for the game XTux.
#Usage: "merge_images.pl ENTITY DIRECTIONS"

die "Usage $0: ENTITY_NAME DIRECTIONS" unless @ARGV == 2;

$name = @ARGV[0];
$num = @ARGV[1]; 

@dir = ( "n", "ne", "e", "se", "s", "sw", "w", "nw" );

sub do_dir {
	my $dir = @_[0];
	my @cmdline;

	for( $i=0 ; $i < $num ; $i++ ) {
		push @cmdline, sprintf "%s_%s_%d.xpm", $name, $dir, $i;
	}

	print "\"$dir\": [ @cmdline ]\n";
	system("convert +append @cmdline $dir");

}

#Make files for each direction of all the frames stacked horizontally.
foreach $d ( @dir ) {
	do_dir( $d );
}

#Join them all together into one big file.
print "Appending @dir\n";
system("convert -append @dir $name.xpm");

