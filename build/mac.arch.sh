#!/bin/bash
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

# PPC or Intel?
ARCH="$(uname -m)"
if [ "$ARCH" = "x86_64" ]
then
    echo $ARCH
    exit 0
fi

# the Mac likes to keep its architecture hidden because for all practical
# purposes, it is both 32 and 64 bit, and makes use of emulation as needed.

# does the hardware support 64-bit mode, even in emulation?
SYSCTL=/usr/sbin/sysctl
[ -x /sbin/sysctl ] && SYSCTL=/sbin/sysctl
CAP64=$($SYSCTL -n hw.cpu64bit_capable)

# real 64-bit hardware has > 32 bits of address space
PADDR_BITS=$(/usr/sbin/sysctl -n machdep.cpu.address_bits.physical)
VADDR_BITS=$(/usr/sbin/sysctl -n machdep.cpu.address_bits.virtual)

# to be 64-bit, it's probably sufficient to test the address space
# but we have the emulation information as well.
if [ $CAP64 -ne 0 ]
then
    if [ $PADDR_BITS -gt 32 ] && [ $VADDR_BITS -gt 32 ]
    then
        # call it 64-bit
        if [ "$ARCH" = "i386" ] || [ "$ARCH" = "x86_64" ]
        then
            echo "x86_64"
            exit 0
        fi

        if [ "$ARCH" = "Power Macintosh" ]
        then
            echo "ppc64"
            exit 0
        fi

        # unrecognized
        echo "unrecognized"
        exit 5
    fi
fi

# call it 32-bit
if [ "$ARCH" = "i386" ]
then
    echo "i386"
    exit 0
fi

if [ "$ARCH" = "Power Macintosh" ]
then
    echo "ppc32"
    exit 0
fi

# unrecognized
echo "unrecognized"
exit 5

