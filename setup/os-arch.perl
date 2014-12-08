use strict;

use FindBin qw($Bin);
require "$Bin/os-arch.prl";

my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();

my $res = "$OS.$ARCH";

if (@ARCHITECTURES) {
    my $name = "$Bin/../Makefile.config.$OS.arch";
    if (-e $name) {
        while (1) {
            open F, $name or last;
            $res = "$OS." . <F>;
            chomp $res;
            last;
        }
    }
}

print "$res\n";
