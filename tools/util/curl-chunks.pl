#!/usr/local/bin/perl -w
die if $#ARGV < 0;
for ( $from = 0; $from < 97523902; $from += 524288 ) {
    $to = $from + 524287;
    `curl -sr $from-$to $ARGV[0]  > /dev/null`;
}
