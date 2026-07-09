#!sh

### Count the number of bases and the number of ACGT bases.
check () {
    perl -ane '$n += length $F[9]; $acgt += ($F[9] =~ tr/ACGT//);
END{print "More ACGT than N\n" if $acgt > ($n - $acgt);
    print "More N than ACGT\n" if ($n - $acgt) > $acgt;
}' "${@}"
}

pass=$( check "VDB-6398_pass.sam" )
fail=$( check "VDB-6398_fail.sam" )
both=$( check "VDB-6398_pass.sam" "VDB-6398_fail.sam" )

[[ "${pass}" = "More ACGT than N" ]] || { echo "failed! pass file does not have more ACGTs than Ns." ; exit 1 ; }
[[ "${fail}" = "More N than ACGT" ]] || { echo "failed! fail file does not have more Ns than ACGTs." ; exit 1 ; }
[[ "${both}" = "More N than ACGT" ]] || { echo "failed! combined file does not have more Ns than ACGTs." ; exit 1 ; }
