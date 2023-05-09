#!/usr/bin/perl -w

use strict;
use warnings;
use JSON;
use IO::Handle;

sub allTagsMatching($$$) {
    my $url = "https://hub.docker.com/v2/namespaces/$_[0]/repositories/$_[1]/tags";
    while ($url) {
        my $kid = open(my $fh, '-|') // die "can't fork: $!";
        if ($kid == 0) {
            exec 'curl', '--silent', $url
        }
        
        my $json = decode_json do {local $/; binmode($fh); <$fh>};
        close $fh;

        my $results = $json->{'results'};
        $url = $json->{'next'};

        for my $tag (@$results) {
            my $name = $tag->{'name'};
            printf("%s/%s:%s\n", $_[0], $_[1], $name) if $name =~ $_[2];
        }
    }
}

my $version = shift or die "need version";
allTagsMatching 'ncbi', 'sra-tools', qr/.-$version$/;
