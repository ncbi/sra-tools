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

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>

#include <klib/text.h> /* String */
#include <klib/status.h> /* STSMSG */

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/log-xml.h>

#include <kfg/config.h> /* KConfigSetNgcFile */

#include <kfs/directory.h>

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include "vdb-validate.h"
#include "check-redact.h"

bool exhaustive = false;
bool md5_required = false;
bool ref_int_check = false;
bool s_IndexOnly = false;

/*static char const* const defaultLogLevel =
#if _DEBUGGING
"debug5";
#else
"info";
#endif*/

/******************************************************************************
 * Usage
 ******************************************************************************/
const char UsageDefaultName[] = "vdb-validate";

rc_t CC UsageSummary(const char *prog_name)
{
    return KOutMsg ( "Usage: %s [options] path [ path... ]\n"
                     "\n"
                     , prog_name );
}

/*static char const *help_text[] =
{
    "Check components md5s if present, "
            "fail unless other checks are requested (default: yes)", NULL,
    "Check blobs CRC32 (default: no)", NULL,
    "Check 'skey' index (default: no)", NULL,
    "Continue checking object for all possible errors (default: no)", NULL,
    "Check data referential integrity for databases (default: no)", NULL,
    "Check index-only with blobs CRC32 (default: no)", NULL
};*/

#define ALIAS_md5  "5"
#define OPTION_md5 "md5"
static const char *USAGE_MD5[] = { "Check components md5s if present, "
    "fail unless other checks are requested (default: yes)", NULL };
/*
#define ALIAS_MD5  "M"
#define OPTION_MD5 "MD5"
*/
#define ALIAS_blob_crc  "b"
#define OPTION_blob_crc "blob-crc"
static const char *USAGE_BLOB_CRC[] =
{ "Check blobs CRC32 (default: no)", NULL };

#define ALIAS_BLOB_CRC  "B"
#define OPTION_BLOB_CRC "BLOB-CRC"

#define ALIAS_CNS_CHK  "C"
#define OPTION_CNS_CHK "CONSISTENCY-CHECK"
static const char *USAGE_CNS_CHK[] =
{ "Deeply check data consistency for tables (default: no)", NULL };

#if CHECK_INDEX
#define ALIAS_INDEX  "i"
#define OPTION_INDEX "index"
static const char *USAGE_INDEX[] = { "Check 'skey' index (default: no)", NULL };
#endif

#define ALIAS_EXHAUSTIVE  "x"
#define OPTION_EXHAUSTIVE "exhaustive"
static const char *USAGE_EXHAUSTIVE[] =
{ "Continue checking object for all possible errors (default: false)", NULL };

#define ALIAS_ref_int  "d"
#define OPTION_ref_int "referential-integrity"
static const char *USAGE_REF_INT[] =
{ "Check data referential integrity for databases (default: yes)", NULL };

#define ALIAS_REF_INT  "I"
#define OPTION_REF_INT "REFERENTIAL-INTEGRITY"

#define OPTION_SDC_SEC_ROWS "sdc:rows"
static const char *USAGE_SDC_SEC_ROWS[] =
{ "Specify maximum amount of secondary alignment table rows to look at before saying accession is good, default 100000.",
  "Specifying 0 will iterate the whole table. Can be in percent (e.g. 5%)",
  NULL };

#define OPTION_SDC_SEQ_ROWS "sdc:seq-rows"
static const char *USAGE_SDC_SEQ_ROWS[] =
{ "Specify maximum amount of sequence table rows to look at before saying accession is good, default 100000.",
  "Specifying 0 will iterate the whole table. Can be in percent (e.g. 5%)",
  NULL };

#define OPTION_SDC_PLEN_THOLD "sdc:plen_thold"
static const char *USAGE_SDC_PLEN_THOLD[] =
{ "Specify a threshold for amount of secondary alignment which are shorter (hard-clipped) than corresponding primaries, default 1%.", NULL };

#define OPTION_NGC "ngc"
static const char *USAGE_NGC[] = { "path to ngc file", NULL };

static const char *USAGE_DRI[] =
{ "Do not check data referential integrity for databases", NULL };

static const char *USAGE_IND_ONLY[] =
{ "Check index-only with blobs CRC32 (default: no)", NULL };

#define OPTION_CHECK_REDACT "check-redact"
static const char *USAGE_CHECK_REDACT[] =
{ "check if redaction of bases has been correctly performed (default: false)", NULL };

#define OPTION_REQUIRE_BLOB_CRC "require-blob-checksums"
static const char *USAGE_REQUIRE_BLOB_CRC[] =
{ "Require blob checksums (default: no)", NULL };

static OptDef options [] =
{                                                    /* needs_value, required */
/*  { OPTION_MD5     , ALIAS_MD5     , NULL, USAGE_MD5     , 1, true , false }*/
    { OPTION_BLOB_CRC, ALIAS_BLOB_CRC, NULL, USAGE_BLOB_CRC, 1, true , false }
#if CHECK_INDEX
  , { OPTION_INDEX   , ALIAS_INDEX   , NULL, USAGE_INDEX   , 1, false, false }
#endif
  , { OPTION_EXHAUSTIVE,
                   ALIAS_EXHAUSTIVE, NULL, USAGE_EXHAUSTIVE, 1, false, false }
  , { OPTION_REF_INT , ALIAS_REF_INT , NULL, USAGE_REF_INT , 1, true , false }
  , { OPTION_CNS_CHK , ALIAS_CNS_CHK , NULL, USAGE_CNS_CHK , 1, true , false }
  , { OPTION_NGC     , NULL          , NULL, USAGE_NGC     , 1, true , false }

    /* secondary alignment table data check options */
  , { OPTION_SDC_SEC_ROWS, NULL      , NULL, USAGE_SDC_SEC_ROWS, 1, true , false }
  , { OPTION_SDC_SEQ_ROWS, NULL      , NULL, USAGE_SDC_SEQ_ROWS, 1, true , false }
  , { OPTION_SDC_PLEN_THOLD, NULL    , NULL, USAGE_SDC_PLEN_THOLD, 1, true , false }

    /* not printed by --help */
  , { "dri"          , NULL          , NULL, USAGE_DRI     , 1, false, false }
  , { "index-only"   ,NULL           , NULL, USAGE_IND_ONLY, 1, false, false }

    /* obsolete options for backward compatibility */
  , { OPTION_md5     , ALIAS_md5     , NULL, USAGE_MD5     , 1, true , false }
  , { OPTION_blob_crc, ALIAS_blob_crc, NULL, USAGE_BLOB_CRC, 1, false, false }
  , { OPTION_ref_int , ALIAS_ref_int , NULL, USAGE_REF_INT , 1, false, false }

  , { OPTION_CHECK_REDACT, NULL      , NULL, USAGE_CHECK_REDACT, 1, false , false }

  , { OPTION_REQUIRE_BLOB_CRC, NULL  , NULL, USAGE_REQUIRE_BLOB_CRC, 1, false , false }
};

/*
#define NUM_SILENT_TRAILING_OPTIONS 5

static const char *option_params [] =
{
    NULL
  , NULL
#if CHECK_INDEX
  , NULL
#endif
  , NULL
  , NULL
  , NULL
  , NULL
};
*/
rc_t CC Usage ( const Args * args )
{
/*  uint32_t i; */
    const char *progname, *fullpath;
    rc_t rc = ArgsProgram ( args, & fullpath, & progname );
    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "  Examine directories, files and VDB objects,\n"
              "  reporting any problems that can be detected.\n"
              "\n"
              "Components md5s are always checked if present.\n"
              "\n"
              "Options:\n"
        );

/*  HelpOptionLine(ALIAS_MD5     , OPTION_MD5     , "yes | no", USAGE_MD5); */
    HelpOptionLine(ALIAS_BLOB_CRC, OPTION_BLOB_CRC, "yes | no", USAGE_BLOB_CRC);
#if CHECK_INDEX
    HelpOptionLine(ALIAS_INDEX   , OPTION_INDEX   , "yes | no", USAGE_INDEX);
#endif
    HelpOptionLine(ALIAS_REF_INT , OPTION_REF_INT , "yes | no", USAGE_REF_INT);
    HelpOptionLine(ALIAS_CNS_CHK , OPTION_CNS_CHK , "yes | no", USAGE_CNS_CHK);
    HelpOptionLine(ALIAS_EXHAUSTIVE, OPTION_EXHAUSTIVE, NULL, USAGE_EXHAUSTIVE);
    HelpOptionLine(NULL          , OPTION_SDC_SEC_ROWS, "rows"    , USAGE_SDC_SEC_ROWS);
    HelpOptionLine(NULL          , OPTION_SDC_SEQ_ROWS, "rows"    , USAGE_SDC_SEQ_ROWS);
    HelpOptionLine(NULL          , OPTION_SDC_PLEN_THOLD, "threshold", USAGE_SDC_PLEN_THOLD);
    HelpOptionLine(NULL          , OPTION_NGC           , "path", USAGE_NGC);

    HelpOptionLine(NULL          , OPTION_CHECK_REDACT, NULL, USAGE_CHECK_REDACT);

    HelpOptionLine(NULL          , OPTION_REQUIRE_BLOB_CRC, NULL, USAGE_REQUIRE_BLOB_CRC);
/*
#define NUM_LISTABLE_OPTIONS \
    ( sizeof options / sizeof options [ 0 ] - NUM_SILENT_TRAILING_OPTIONS )

    for ( i = 0; i < NUM_LISTABLE_OPTIONS; ++i )
    {
        HelpOptionLine ( options [ i ] . aliases, options [ i ] . name,
            option_params [ i ], options [ i ] . help );
    }
*/

    KOutMsg("\n");

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion () );

    return 0;
}

static
rc_t parse_args ( vdb_validate_params *pb, Args *args )
{
    const char *dummy = NULL;
    rc_t rc;
    uint32_t cnt;

    pb -> md5_chk = true;
    pb->consist_check = false;
    ref_int_check
        = pb -> md5_chk_explicit = md5_required = true;
    pb -> blob_crc = false;
    pb -> sdc_sec_rows_in_percent = false;
    pb -> sdc_sec_rows.number = 100000;
    pb -> sdc_seq_rows_in_percent = false;
    pb -> sdc_seq_rows.number = 100000;
    pb -> sdc_pa_len_thold_in_percent = true;
    pb -> sdc_pa_len_thold.percent = 0.01;

    pb -> check_redact = false;
  {
    rc = ArgsOptionCount(args, OPTION_CNS_CHK, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_CNS_CHK "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_CNS_CHK, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_CNS_CHK "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'y') {
            pb->consist_check = true;
        }
    }
  }
  {
    rc = ArgsOptionCount ( args, "exhaustive", & cnt );
    if ( rc != 0 )
        return rc;
    exhaustive = cnt != 0;
  }
  {
      rc = ArgsOptionCount ( args, OPTION_CHECK_REDACT, & cnt );
      if ( rc != 0 )
          return rc;
      pb -> check_redact = ( cnt != 0 );
  }
  {
      rc = ArgsOptionCount ( args, OPTION_REQUIRE_BLOB_CRC, & cnt );
      if ( rc != 0 )
          return rc;
      pb -> blob_crc_required = ( cnt != 0 );
  }

  {
    rc = ArgsOptionCount(args, OPTION_REF_INT, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_REF_INT "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_REF_INT, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_REF_INT "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'n') {
            ref_int_check = false;
        }
    }
  }
  {
    rc = ArgsOptionCount ( args, "dri", & cnt );
    if ( rc != 0 )
        return rc;
    if (cnt != 0) {
        ref_int_check = false;
    }
  }
#if CHECK_INDEX
  {
    rc = ArgsOptionCount ( args, "index", & cnt );
    if ( rc != 0 )
        return rc;
    pb -> index_chk = cnt != 0;
  }
#endif
  {
    rc = ArgsOptionCount ( args, "index-only", & cnt );
    if ( rc != 0 )
        return rc;
    if ( cnt != 0 )
        s_IndexOnly = pb -> blob_crc = true;
  }
  {
    rc = ArgsOptionCount( args, OPTION_BLOB_CRC, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_BLOB_CRC "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_BLOB_CRC, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_BLOB_CRC "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'y') {
            pb -> blob_crc = true;
        }
    }
  }
  {
    rc = ArgsOptionCount(args, OPTION_md5, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_md5 "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_md5, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_md5 "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'n') {
            pb -> md5_chk = pb -> md5_chk_explicit = md5_required = false;
        }
    }
  }
  {
      rc = ArgsOptionCount ( args, OPTION_SDC_SEC_ROWS, &cnt );
      if (rc)
      {
          LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SDC_SEC_ROWS);
          return rc;
      }

      if (cnt > 0)
      {
          uint64_t value;
          size_t value_size;
          rc = ArgsOptionValue ( args, OPTION_SDC_SEC_ROWS, 0, (const void **) &dummy );
          if (rc)
          {
              LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SDC_SEC_ROWS);
              return rc;
          }

          pb->sdc_enabled = true;

          value_size = string_size ( dummy );
          if ( value_size >= 1 && dummy[value_size - 1] == '%' )
          {
              value = string_to_U64 ( dummy, value_size - 1, &rc );
              if (rc)
              {
                  LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_SEC_ROWS);
                  return rc;
              }
              else if (value == 0 || value > 100)
              {
                  rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                  LOGERR (klogInt, rc, OPTION_SDC_SEC_ROWS " has illegal percentage value (has to be 1-100%)" );
                  return rc;
              }

              pb->sdc_sec_rows_in_percent = true;
              pb->sdc_sec_rows.percent = (double)value / 100;
          }
          else
          {
              value = string_to_U64 ( dummy, value_size, &rc );
              if (rc)
              {
                  LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_SEC_ROWS);
                  return rc;
              }
              pb->sdc_sec_rows_in_percent = false;
              pb->sdc_sec_rows.number = value;
          }
      }
  }
  {
    rc = ArgsOptionCount ( args, OPTION_SDC_SEQ_ROWS, &cnt );
    if (rc)
    {
        LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SDC_SEQ_ROWS);
        return rc;
    }

    if (cnt > 0)
    {
        uint64_t value;
        size_t value_size;
        rc = ArgsOptionValue ( args, OPTION_SDC_SEQ_ROWS, 0, (const void **) &dummy );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SDC_SEQ_ROWS);
            return rc;
        }

        pb->sdc_enabled = true;

        value_size = string_size ( dummy );
        if ( value_size >= 1 && dummy[value_size - 1] == '%' )
        {
            value = string_to_U64 ( dummy, value_size - 1, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_SEQ_ROWS);
                return rc;
            }
            else if (value == 0 || value > 100)
            {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                LOGERR (klogInt, rc, OPTION_SDC_SEQ_ROWS " has illegal percentage value (has to be 1-100%)" );
                return rc;
            }

            pb->sdc_seq_rows_in_percent = true;
            pb->sdc_seq_rows.percent = (double)value / 100;
        }
        else
        {
            value = string_to_U64 ( dummy, value_size, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_SEQ_ROWS);
                return rc;
            }
            pb->sdc_seq_rows_in_percent = false;
            pb->sdc_seq_rows.number = value;
        }
    }
  }
    {
        rc = ArgsOptionCount ( args, OPTION_SDC_PLEN_THOLD, &cnt );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SDC_PLEN_THOLD);
            return rc;
        }

        if (cnt > 0)
        {
            uint64_t value;
            size_t value_size;
            rc = ArgsOptionValue ( args, OPTION_SDC_PLEN_THOLD, 0, (const void **) &dummy );
            if (rc)
            {
                LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SDC_PLEN_THOLD);
                return rc;
            }

            pb->sdc_enabled = true;

            value_size = string_size ( dummy );
            if ( value_size >= 1 && dummy[value_size - 1] == '%' )
            {
                value = string_to_U64 ( dummy, value_size - 1, &rc );
                if (rc)
                {
                    LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_PLEN_THOLD);
                    return rc;
                }
                else if (value == 0 || value > 100)
                {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR (klogInt, rc, OPTION_SDC_PLEN_THOLD " has illegal percentage value (has to be 1-100%)" );
                    return rc;
                }

                pb->sdc_pa_len_thold_in_percent = true;
                pb->sdc_pa_len_thold.percent = (double)value / 100;
            }
            else
            {
                value = string_to_U64 ( dummy, value_size, &rc );
                if (rc)
                {
                    LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_PLEN_THOLD);
                    return rc;
                }
                pb->sdc_pa_len_thold_in_percent = false;
                pb->sdc_pa_len_thold.number = value;
            }
        }
    }

/* OPTION_NGC */
    {
        rc = ArgsOptionCount(args, OPTION_NGC, &cnt);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_NGC "' argument");
            return rc;
        }
        if (cnt != 0) {
            rc = ArgsOptionValue(args, OPTION_NGC, 0, (const void **)&dummy);
            if (rc != 0) {
                LOGERR(klogErr, rc,
                    "Failure to get '" OPTION_NGC "' argument");
                return rc;
            }
            KConfigSetNgcFile(dummy);
        }
    }

    if ( pb -> blob_crc || pb -> index_chk )
        pb -> md5_chk = pb -> md5_chk_explicit;

    pb->exhaustive = exhaustive;

    return 0;
}

static
void vdb_validate_params_whack ( vdb_validate_params *pb )
{
    VDBManagerRelease ( pb -> vmgr );
    KDBManagerRelease ( pb -> kmgr );
    KDirectoryRelease ( pb -> wd );
    memset ( pb, 0, sizeof * pb );
}

static
rc_t vdb_validate_params_init ( vdb_validate_params *pb )
{
    rc_t rc;
    KDirectory *wd;

    memset ( pb, 0, sizeof * pb );

    rc = KDirectoryNativeDir ( & wd );
    if ( rc == 0 )
    {
        pb -> wd = wd;
        rc = VDBManagerMakeRead ( & pb -> vmgr, wd );
        if ( rc == 0 )
        {
            rc = VDBManagerOpenKDBManagerRead ( pb -> vmgr, & pb -> kmgr );
            if ( rc == 0 )
                return 0;
        }
    }

    vdb_validate_params_whack ( pb );
    return rc;
}

static rc_t main_with_args(Args *const args)
{
    XMLLogger const *xlogger = NULL;
    rc_t rc = XMLLogger_Make(&xlogger, NULL, args);

    if (rc) {
        LOGERR(klogErr, rc, "Failed to make XML logger");
    }
    else {
        uint32_t pcount;
        rc = ArgsParamCount ( args, & pcount );
        if ( rc != 0 )
            LOGERR ( klogErr, rc, "Failed to count command line parameters" );
        else if ( pcount == 0 )
        {
            rc = RC ( rcExe, rcPath, rcValidating, rcParam, rcInsufficient );
            LOGERR ( klogErr, rc, "No paths to validate" );
            MiniUsage ( args );
        }
        else
        {
            vdb_validate_params pb;
            rc = vdb_validate_params_init ( & pb );
            if ( rc != 0 )
                LOGERR ( klogErr, rc, "Failed to initialize internal managers" );
            else
            {
                rc = parse_args ( & pb, args );
                if ( rc != 0 )
                    LOGERR ( klogErr, rc, "Failed to extract command line options" );
                else
                {
                    rc = KLogLevelSet ( klogInfo );
                    if ( rc != 0 )
                        LOGERR ( klogErr, rc, "Failed to set log level" );
                    else
                    {
                        uint32_t i;

                        md5_required = false;

                        STSMSG(2, ("exhaustive = %d", exhaustive));
                        STSMSG(2, ("ref_int_check = %d", ref_int_check));
                        STSMSG(2, ("md5_required = %d", md5_required));
                        STSMSG(2, ("P {"));
                        STSMSG(2, ("\tmd5_chk = %d", pb.md5_chk));
                        STSMSG(2, ("\tmd5_chk_explicit = %d",
                            pb.md5_chk_explicit));
                        STSMSG(2, ("\tblob_crc = %d", pb.blob_crc));
                        STSMSG(2, ("\tconsist_check = %d", pb.consist_check));
                        STSMSG(2, ("}"));
                        for ( i = 0; i < pcount; ++ i )
                        {
                            rc_t rc2;
                            const char *path;
                            rc = ArgsParamValue ( args, i, (const void **)& path );
                            if ( rc != 0 )
                            {
                                LOGERR ( klogErr, rc, "Failed to extract command line options" );
                                break;
                            }

                            rc2 = vdb_validate ( & pb, path );
                            if ( rc == 0 ) {
                                rc = rc2;
                            }
                            if ( pb . check_redact ) {
                                rc2 = check_redact( path );
                                if ( rc == 0 ) {
                                    rc = rc2;
                                }
                            }
                        }
                    }
                }

                vdb_validate_params_whack ( & pb );
            }
        }
        XMLLogger_Release(xlogger);
    }
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    Args *args = NULL;
    rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 2,
                                options, sizeof(options)/sizeof(options[0]),
                                XMLLogger_Args, XMLLogger_ArgsQty);

    if ( rc != 0 )
        LOGERR ( klogErr, rc, "Failed to parse command line" );
    else
    {
        rc = main_with_args(args);
        ArgsWhack ( args );
    }

    return VDB_TERMINATE( rc );
}
