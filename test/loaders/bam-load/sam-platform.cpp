/*===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#include <iostream>
#include <vector>


#include <ctype.h>      /* toupper */
#include <insdc/sra.h>  /* SRA platform ID values */

extern "C" {
#include "../../../tools/loaders/bam-loader/sam-header-platform.c"
}
static const char *platform_symbolic_names[] = { INSDC_SRA_PLATFORM_SYMBOLS };

int main(int argc, char *argv[]) {
	auto const testValues = std::vector<char const *> {
		"CAPILLARY", "DNBSEQ", "ELEMENT",
		"HELICOS", "ILLUMINA", "IONTORRENT",
		"LS454", "ONT", "PACBIO", "SINGULAR",
		"SOLID", "ULTIMA",
		"SANGER", "DNSSEC", "ELEFENT",
		"HELICOPTER", "ILLUMINATE", "NONTORRENT",
		"LA454", "0NT", "PACBI0", "CINGULAR",
		"SALAD", "ULTIMATE",
	};
	auto const expected = std::vector<int> {
		SRA_PLATFORM_CAPILLARY,
		SRA_PLATFORM_DNBSEQ,
		SRA_PLATFORM_ELEMENT_BIO,
		SRA_PLATFORM_HELICOS,
		SRA_PLATFORM_ILLUMINA,
		SRA_PLATFORM_ION_TORRENT,
		SRA_PLATFORM_454,
		SRA_PLATFORM_OXFORD_NANOPORE,
		SRA_PLATFORM_PACBIO_SMRT,
		SRA_PLATFORM_SINGULAR_GENOMICS,
		SRA_PLATFORM_ABSOLID,
		SRA_PLATFORM_ULTIMA,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED,
		SRA_PLATFORM_UNDEFINED
	};
	
	for (auto && testValue : testValues) {
		auto const i = &testValue - &testValues[0];
		auto const expect = expected[i];
		auto const got = (int)get_platform_id(testValue);
		if (got == expect)
			continue;
		
		std::cerr << "failure: " << testValue;
		if (got < 0 || got > int(sizeof(platform_symbolic_names)/sizeof(platform_symbolic_names[0]))) {
			std::cerr << " resulted in an unexpected value " << got;
		}
		else {
			std::cerr << " should have translated to "
					  << platform_symbolic_names[expect] << " not "
					  << platform_symbolic_names[  got ];
		}
		std::cerr << std::endl;
		return 1;
	}
	return 0;
}
