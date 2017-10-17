foreach (<STDIN>){
  chomp;
  if ( $_ ne "" ) {
    die "*** bad version $_ (expected $ARGV[0])\n" unless ( /$ARGV[0]/ );
  }
}
