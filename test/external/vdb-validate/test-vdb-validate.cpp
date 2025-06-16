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

/**
* Unit tests for class SraInfo
*/

extern "C" {
#include "vdb-validate.c"
}

#include <ktst/unit_test.hpp>

#include <klib/out.h>

using namespace std;
using namespace ncbi::NK;

// to satisfy the linker
bool exhaustive = false;
bool md5_required = false;
bool ref_int_check = false;
bool s_IndexOnly = false;

TEST_SUITE(VdbValidateTestSuite);

TEST_CASE(is_sorted_empty)
{
	REQUIRE(is_sorted(0, NULL));
}

TEST_CASE(is_sorted_sorted)
{
	int64_t const sorted[] = {
		1, 2, 3
	};
	REQUIRE(is_sorted(1, sorted));
	REQUIRE(is_sorted(2, sorted));
	REQUIRE(is_sorted(2, sorted+1));
	REQUIRE(is_sorted(3, sorted));
}

TEST_CASE(is_sorted_unsorted)
{
	int64_t const unsorted[] = {
		3, 2, 1
	};
	REQUIRE(is_sorted(1, unsorted));
	REQUIRE(is_sorted(1, unsorted+1));
	REQUIRE(is_sorted(1, unsorted+2));
	REQUIRE(!is_sorted(2, unsorted));
	REQUIRE(!is_sorted(2, unsorted+1));
	REQUIRE(!is_sorted(3, unsorted));
}

//////////////////////////////////////////// Main
#include <kfg/config.h>

extern "C"
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return VdbValidateTestSuite(argc, argv);
}
