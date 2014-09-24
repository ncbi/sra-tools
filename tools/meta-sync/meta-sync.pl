#!/usr/local/bin/perl -w
use strict;
use lib '/export/home/sybase/clients-mssql/current64/perl';
use DBI;
my $db;
my $start = `date`;
my $S =      1;
my $L = 315142; # $S <= R <= $L
my $INI = 'meta-sync.ini';
if (-e $INI) {
    $S = `cat $INI`;
    chomp $S;
}
for (my $i = $S; $i <= $L; ++$i) {
  my $A = sprintf "SRR%06d", ($i);
  if ($i == 305952) {
#   print "$A SKIP\n"; next;
  }
  my $res = system "meta-sync $A"; # -+ APP 
  if ($res) {
    `echo $i > $INI`;
    die "$i $A";
  }
  if ($i == $L) {
    $db = DBI->connect("dbi:Sybase:server=NIHMS2", "anyone", "allowed",
        { syb_err_handler => \&err_handler }) unless ($db);
    die unless ($db);
    my $sth = $db->prepare("select max(acc) from SRA_Main..Run") || die;
    my $rv  = $sth->execute || die;
    my @res = $sth->fetchrow_array || die;
    die if ($#res != 0);
    $res[0] =~ /^(...)(.{7})$/;
    die unless ($1 && $2 && $1 eq 'SRR');
    $L = $2;
  }
}
sub err_handler {
    my($err, $sev, $state, $line, $server, $proc, $msg, $sql, $err_type) = @_;
    print "$msg $proc\n";
    1
}
