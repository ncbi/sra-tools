#!sh

### Count the number of bases and the number of ACGT bases.
check_bases () {
    perl -ane '$n += length $F[9]; $acgt += ($F[9] =~ tr/ACGT//);
END{print "More ACGT than N\n" if $acgt > ($n - $acgt);
    print "More N than ACGT\n" if ($n - $acgt) > $acgt;
}' "${@}"
}

### Count the number of aligned and unaligned reads.
check_aligned () {
    perl -ane'++$aligned[((0+$F[1]) & 0x004) == 0 ? 0 : 1];
END{print "Some aligned and some unaligned\n" if $aligned[0] > 0 && $aligned[1] > 0}' "${@}"
}

pass=$( check_bases "VDB-6398_pass.sam" )
fail=$( check_bases "VDB-6398_fail.sam" )
both=$( check_bases "VDB-6398_pass.sam" "VDB-6398_fail.sam" )

[[ "${pass}" = "More ACGT than N" ]] || { echo "failed! pass file does not have more ACGTs than Ns." ; exit 1 ; }
[[ "${fail}" = "More N than ACGT" ]] || { echo "failed! fail file does not have more Ns than ACGTs." ; exit 1 ; }
[[ "${both}" = "More N than ACGT" ]] || { echo "failed! combined file does not have more Ns than ACGTs." ; exit 1 ; }

pass=$( check_aligned "VDB-6398_pass.sam" )
fail=$( check_aligned "VDB-6398_fail.sam" )

[[ "${pass}" = "Some aligned and some unaligned" ]] || { echo "failed! pass file does not have both aligned and unaligned reads." ; exit 1 ; }
[[ "${fail}" = "Some aligned and some unaligned" ]] || { echo "failed! fail file does not have both aligned and unaligned reads." ; exit 1 ; }
