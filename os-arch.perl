use strict;

use FindBin qw($Bin);
require "$Bin/os-arch.pm";

my ($OS, $ARCH, $OSTYPE, $MARCH, @ARCHITECTURES) = OsArch();

my $res = "$OS.$ARCH";

if (@ARCHITECTURES) {
    my $name = "$Bin/user.status";
    if (-e $name) {
        while (1) {
            open F, $name or last;
            foreach (<F>) {
                chomp;
                @_ = split /=/;
                if ($#_ == 1) {
                    if ($_[0] eq 'arch') {
                        foreach (@ARCHITECTURES) {
                            if ($_ eq $_[1]) {
                                $res = "$OS.$_[1]";
                                last;
                            }
                        }
                        last;
                    }
                }
            }
            last;
        }
    }
}

print "$res\n";
