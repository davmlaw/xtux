#!/usr/bin/perl

#Author: David Lawrence - philaw@ozemail.com.au
#A script to flip images in a horizontal direction in the process of creating
#the composite images in XTux.

foreach $file ( @ARGV ) {

	$_ = $file;
	if( s/_(.*?)e_/_$1w_/ ) {
		print "$file ==> $_\n";
		system("convert -flop $file $_");
	}

}
#print "string = \"$_\"\n";
