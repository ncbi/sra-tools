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
* Unit tests for copycat/kff files
*/

#include <ktst/unit_test.hpp>

#include <kff/ffext.h>
#include <kff/ffmagic.h>

#define class clss
#include <kff/fileformat.h>
#undef class

using namespace std;

TEST_SUITE(KffTestSuite);

TEST_CASE(ExtFileFormat)
{
    struct KFileFormat* pft;
    const char format[] = {
        "ext1\tTestFormat1\n"
        "ext2\tTestFormat2\n"
    };
    const char typeAndClass[] = {
        "TestFormat1\tTestClass1\n"
        "TestFormat2\tTestClass2\n"
    };
    REQUIRE_RC(KExtFileFormatMake(&pft, format, sizeof(format) - 1, typeAndClass, sizeof(typeAndClass) - 1));

    KFileFormatType type;
    KFileFormatClass clss;
    char descr[1024];
    size_t length;
    REQUIRE_RC(KFileFormatGetTypePath(pft,
                                      NULL, // ignored
                                      "qq.ext2",
                                      &type,
                                      &clss,
                                      descr,
                                      sizeof(descr),
                                      &length));
    REQUIRE_EQ(type, 2);
    REQUIRE_EQ(clss, 2);
    REQUIRE_EQ(string(descr, length), string("TestFormat2"));

    REQUIRE_RC(KFileFormatGetClassDescr(pft, clss, descr, sizeof (descr)));
    REQUIRE_EQ(string(descr), string("TestClass2"));
    REQUIRE_EQ(length, string("TestClass2").length()+1);
    REQUIRE_RC(KFileFormatRelease(pft));
}

TEST_CASE(MagicFileFormat)
{
    struct KFileFormat* pft;
    const char magic[] =
    {
        "Generic Format for Sequence Data (SRF)\tSequenceReadFormat\n"
        "GNU tar archive\tTapeArchive\n"
    };
    const char typeAndClass[] = {
        "SequenceReadFormat\tRead\n"
        "TapeArchive\tArchive\n"
    };
    REQUIRE_RC(KMagicFileFormatMake (&pft, magic, sizeof(magic) - 1, typeAndClass, sizeof(typeAndClass) - 1));
    REQUIRE_RC(KFileFormatRelease(pft));
}


//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return KffTestSuite(argc, argv);
}

}
