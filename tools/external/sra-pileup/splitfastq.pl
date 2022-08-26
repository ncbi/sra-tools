#!/usr/bin/perl -w

use IO::File;

my $line;
my $state = 0;
my %filehandles;
my $current_filehandle;
my $base =  $ARGV[ 0 ];

if ( !defined ( $base ) )
{
    $base = "out";
}

while ( defined ( $line = <STDIN> ) )
{
    #remove line-feeds, white space etc.
    chomp( $line );

    if ( $state == 0 )
    {
        # get the first word
        my $word0 = ( split( /\s+/, $line ) )[ 0 ];

        # start with the beginning of the word, until '/' found, continue with numbers to end of string
        if ( $word0 =~ /^[^\/]+\/(\d+)$/ )
        {
            # grep what matched after the '/'
            my $selector = $1;
            if ( !defined ( $filehandles{$selector}) )
            {
                $filehandles{$selector} = new IO::File( "$base.$selector.fastq", "w" );
            }
            $current_filehandle = $filehandles{$selector};
        }
    }

    if ( defined( $current_filehandle ) )
    {
        $current_filehandle -> print( "$line\n" );
    }

    $state ++;
    $state &= 0x3;
}