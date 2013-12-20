#!/usr/bin/perl

($width, $height, $swidth, $sheight) = @ARGV;
$pi = 3.14;

$yfact = 0.5;

$disx = $width / 2;
$disy = $height - $sheight;

foreach $i ( 0..7 ) {
	$_ = $i * 2 * ($pi / 8);
	$x = $swidth * cos;
	$y = $sheight * sin * $yfact;

	$arm1x = int( $x + $disx );
	$arm1y = int( $y + $disy );

	$arm2x = int( - $x + $disx );
	$arm2y = int( - $y + $disy );

	print "$i : $arm1x, $arm1y, $arm2x, $arm2y\n";
}

