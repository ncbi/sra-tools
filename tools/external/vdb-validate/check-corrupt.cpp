/*==============================================================================
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

#include <cmath>

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/writer.h>
#include <kfs/directory.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>

#include <sstream>

#define SA_TABLE_LOOKUP_LIMIT 100000
#define SEQ_TABLE_LOOKUP_LIMIT 100000
#define PA_LONGER_SA_LIMIT 0.01

typedef struct CheckCorruptConfig
{
    double sa_cutoff_percent; // negative when not used
    uint64_t sa_cutoff_number; // only used when sa_cutoff_percent is negative

    double seq_cutoff_percent; // negative when not used
    uint64_t seq_cutoff_number; // only used when seq_cutoff_percent is negative

    double pa_len_threshold_percent; // negative when not used
    uint64_t pa_len_threshold_number; // only used when pa_len_threshold_percent is negative
} CheckCorruptConfig;

struct VDB_ERROR
{
    VDB_ERROR (const char * _msg, rc_t _rc)
        : msg (_msg), rc (_rc)
    {}

    const char * msg;
    rc_t rc;
};

struct VDB_ROW_ERROR
{
    VDB_ROW_ERROR (const char * _msg, int64_t _row_id, rc_t _rc)
        : row_id ( _row_id ), msg (_msg), rc (_rc)
    {}

    int64_t row_id;
    const char * msg;
    rc_t rc;
};

struct DATA_ERROR
{
    DATA_ERROR (const std::string & _msg)
        : msg(_msg)
    {}

    std::string msg;
};

/**
 * returns true if checks are passed
 */
void runChecks ( const char * accession, const CheckCorruptConfig * config, const VCursor * pa_cursor, const VCursor * sa_cursor, const VCursor * seq_cursor )
{
    rc_t rc;
    uint32_t pa_has_ref_offset_idx;
    uint32_t sa_has_ref_offset_idx;
    uint32_t sa_seq_spot_id_idx;
    uint32_t sa_seq_read_id_idx;
    uint32_t sa_pa_id_idx;
    uint32_t sa_tmp_mismatch_idx;
    uint32_t seq_pa_id_idx;
    uint32_t seq_read_len_idx;
    uint32_t seq_cmp_read_idx;
    bool has_tmp_mismatch;

    /* add columns to cursor */
#define add_column(tbl_name, cursor, idx, col_spec) \
    rc = VCursorAddColumn( cursor, &idx, col_spec ); \
    if ( rc != 0 ) \
        throw VDB_ERROR("VCursorAddColumn() failed for " tbl_name " table, " col_spec " column", rc);

    add_column( "PRIMARY_ALIGNMENT", pa_cursor, pa_has_ref_offset_idx, "(bool)HAS_REF_OFFSET" );
    add_column( "SECONDARY_ALIGNMENT", sa_cursor, sa_has_ref_offset_idx, "(bool)HAS_REF_OFFSET" );
    add_column( "SECONDARY_ALIGNMENT", sa_cursor, sa_seq_spot_id_idx, "SEQ_SPOT_ID" );
    add_column( "SECONDARY_ALIGNMENT", sa_cursor, sa_seq_read_id_idx, "SEQ_READ_ID" );
    add_column( "SECONDARY_ALIGNMENT", sa_cursor, sa_pa_id_idx, "PRIMARY_ALIGNMENT_ID" );
    add_column( "SEQUENCE", seq_cursor, seq_pa_id_idx, "PRIMARY_ALIGNMENT_ID" );
    add_column( "SEQUENCE", seq_cursor, seq_read_len_idx, "READ_LEN" );
    add_column( "SEQUENCE", seq_cursor, seq_cmp_read_idx, "CMP_READ" );

    // optional columns
    rc = VCursorAddColumn( sa_cursor, &sa_tmp_mismatch_idx, "TMP_MISMATCH" );
    if ( rc == 0 )
        has_tmp_mismatch = true;
    else
    {
        has_tmp_mismatch = false;
        rc = 0;
    }


#undef add_column

    rc = VCursorOpen( pa_cursor );
    if (rc != 0)
        throw VDB_ERROR("VCursorOpen() failed for PRIMARY_ALIGNMENT table", rc);
    rc = VCursorOpen( sa_cursor );
    if (rc != 0)
        throw VDB_ERROR("VCursorOpen() failed for SECONDARY_ALIGNMENT table", rc);
    rc = VCursorOpen( seq_cursor );
    if (rc != 0)
        throw VDB_ERROR("VCursorOpen() failed for SEQUENCE table", rc);

    int64_t sa_id_first;
    uint64_t sa_row_count;

    rc = VCursorIdRange( sa_cursor, sa_pa_id_idx, &sa_id_first, &sa_row_count );
    if (rc != 0)
        throw VDB_ERROR("VCursorIdRange() failed for SECONDARY_ALIGNMENT table, PRIMARY_ALIGNMENT_ID column", rc);

    bool reported_about_no_pa = false;
    uint64_t pa_longer_sa_rows = 0;
    uint64_t pa_longer_sa_limit;
    if (config->pa_len_threshold_percent > 0)
        pa_longer_sa_limit = ceil( config->pa_len_threshold_percent * sa_row_count );
    else if (config->pa_len_threshold_number == 0 || config->pa_len_threshold_number > sa_row_count)
        pa_longer_sa_limit = sa_row_count;
    else
        pa_longer_sa_limit = config->pa_len_threshold_number;

    uint64_t sa_row_limit;
    if (config->sa_cutoff_percent > 0)
        sa_row_limit = ceil( config->sa_cutoff_percent * sa_row_count );
    else if (config->sa_cutoff_number == 0 || config->sa_cutoff_number > sa_row_count)
        sa_row_limit = sa_row_count;
    else
        sa_row_limit = config->sa_cutoff_number;

    for ( uint64_t i = 0; i < sa_row_count && i < sa_row_limit; ++i )
    {
        int64_t sa_row_id = i + sa_id_first;
        const void * data_ptr = NULL;
        uint32_t data_len;
        uint32_t pa_row_len;
        uint32_t sa_row_len;
        uint32_t seq_read_len_len;

        // SA:HAS_REF_OFFSET
        rc = VCursorCellDataDirect ( sa_cursor, sa_row_id, sa_has_ref_offset_idx, NULL, (const void**)&data_ptr, NULL, &sa_row_len );
        if ( rc != 0 )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, HAS_REF_OFFSET column", sa_row_id, rc);

        const int64_t * p_seq_spot_id;
        uint32_t seq_spot_id_len;
        // SA:SEQ_SPOT_ID
        rc = VCursorCellDataDirect ( sa_cursor, sa_row_id, sa_seq_spot_id_idx, NULL, (const void**)&p_seq_spot_id, NULL, &seq_spot_id_len );
        if ( rc != 0 || p_seq_spot_id == NULL || seq_spot_id_len != 1 )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, SEQ_SPOT_ID column", sa_row_id, rc);

        int64_t seq_spot_id = *p_seq_spot_id;
        if (seq_spot_id == 0)
        {
            std::stringstream ss;
            ss << "SECONDARY_ALIGNMENT:" << sa_row_id << " has SEQ_SPOT_ID = " << seq_spot_id;

            throw DATA_ERROR(ss.str());
        }

        if ( has_tmp_mismatch )
        {
            const char * p_sa_tmp_mismatch;
            // SA:TMP_MISMATCH
            rc = VCursorCellDataDirect ( sa_cursor, sa_row_id, sa_tmp_mismatch_idx, NULL, (const void**)&p_sa_tmp_mismatch, NULL, &data_len );
            if ( rc != 0 || p_sa_tmp_mismatch == NULL )
                throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, TMP_MISMATCH column", sa_row_id, rc);

            for ( uint32_t j = 0; j < data_len; ++j )
            {
                if ( p_sa_tmp_mismatch[j] == '=' )
                {
                    std::stringstream ss;
                    ss << "SECONDARY_ALIGNMENT:" << sa_row_id << " TMP_MISMATCH contains '='";

                    throw DATA_ERROR(ss.str());
                }
            }
        }

        const int64_t * p_pa_row_id;
        // SA:PRIMARY_ALIGNMENT_ID
        rc = VCursorCellDataDirect ( sa_cursor, sa_row_id, sa_pa_id_idx, NULL, (const void**)&p_pa_row_id, NULL, &data_len );
        if ( rc != 0 || p_pa_row_id == NULL || data_len != 1 )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, PRIMARY_ALIGNMENT_ID column", sa_row_id, rc);

        int64_t pa_row_id = *p_pa_row_id;
        if (pa_row_id == 0)
        {
            if (!reported_about_no_pa)
            {
                PLOGMSG (klogInfo, (klogInfo, "$(ACC) has secondary alignments without primary", "ACC=%s", accession));
                reported_about_no_pa = true;
            }
            continue;
        }

        // PA:HAS_REF_OFFSET
        rc = VCursorCellDataDirect ( pa_cursor, pa_row_id, pa_has_ref_offset_idx, NULL, &data_ptr, NULL, &pa_row_len );
        if ( rc != 0 )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on PRIMARY_ALIGNMENT table, HAS_REF_OFFSET column", pa_row_id, rc);

        // move on when PA.len equal to SA.len
        if (pa_row_len == sa_row_len)
            continue;

        if (pa_row_len < sa_row_len)
        {
            std::stringstream ss;
            ss << "PRIMARY_ALIGNMENT:" << pa_row_id << " HAS_REF_OFFSET length (" << pa_row_len << ") less than SECONDARY_ALIGNMENT:" << sa_row_id << " HAS_REF_OFFSET length (" << sa_row_len << ")";

            throw DATA_ERROR(ss.str());
        }

        // we already know that pa_row_len > sa_row_len
        ++pa_longer_sa_rows;

        const int32_t * p_seq_read_id;
        // SA:SEQ_READ_ID
        rc = VCursorCellDataDirect ( sa_cursor, sa_row_id, sa_seq_read_id_idx, NULL, (const void**)&p_seq_read_id, NULL, &data_len );
        if ( rc != 0 || p_seq_read_id == NULL || data_len != 1 )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, SEQ_READ_ID column", sa_row_id, rc);

        // one-based read index
        int32_t seq_read_id = *p_seq_read_id;

        const uint32_t * p_seq_read_len;
        // SEQ:READ_LEN
        rc = VCursorCellDataDirect ( seq_cursor, seq_spot_id, seq_read_len_idx, NULL, (const void**)&p_seq_read_len, NULL, &seq_read_len_len );
        if ( rc != 0 || p_seq_read_len == NULL )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SEQUENCE table, READ_LEN column", seq_spot_id, rc);

        if ( seq_read_id < 1 || (uint32_t)seq_read_id > seq_read_len_len )
        {
            std::stringstream ss;
            ss << "SECONDARY:" << sa_row_id << " SEQ_READ_ID value (" << seq_read_id << ") - 1 based, is out of SEQUENCE:" << seq_spot_id << " READ_LEN range (" << seq_read_len_len << ")";

            throw DATA_ERROR(ss.str());
        }

        if (pa_row_len != p_seq_read_len[seq_read_id - 1])
        {
            std::stringstream ss;
            ss << "PRIMARY_ALIGNMENT:" << pa_row_id << " HAS_REF_OFFSET length (" << pa_row_len << ") does not match its SEQUENCE:" << seq_spot_id << " READ_LEN[" << seq_read_id - 1 << "] value (" << p_seq_read_len[seq_read_id - 1] << ")";

            throw DATA_ERROR(ss.str());
        }

        if (pa_longer_sa_rows >= pa_longer_sa_limit)
        {
            std::stringstream ss;
            ss << "Limit violation (pa_longer_sa): there are at least " << pa_longer_sa_rows << " alignments where HAS_REF_OFFSET column is longer in PRIMARY_ALIGNMENT than in SECONDARY_ALIGNMENT";

            throw DATA_ERROR(ss.str());
        }
    }

    int64_t seq_id_first;
    uint64_t seq_row_count;

    rc = VCursorIdRange( seq_cursor, seq_pa_id_idx, &seq_id_first, &seq_row_count );
    if (rc != 0)
        throw VDB_ERROR("VCursorIdRange() failed for SEQUENCE table, PRIMARY_ALIGNMENT_ID column", rc);

    uint64_t seq_row_limit;
    if (config->seq_cutoff_percent > 0)
        seq_row_limit = ceil( config->seq_cutoff_percent * seq_row_count );
    else if (config->seq_cutoff_number == 0 || config->seq_cutoff_number > seq_row_count)
        seq_row_limit = seq_row_count;
    else
        seq_row_limit = config->seq_cutoff_number;

    for ( uint64_t i = 0; i < seq_row_count && i < seq_row_limit; ++i )
    {
        int64_t seq_row_id = i + seq_id_first;
        const void * data_ptr = NULL;
        uint32_t data_len;

        const int64_t * p_seq_pa_id;
        uint32_t seq_pa_id_len;
        // SEQ:PRIMARY_ALIGNMENT_ID
        rc = VCursorCellDataDirect ( seq_cursor, seq_row_id, seq_pa_id_idx, NULL, (const void**)&p_seq_pa_id, NULL, &seq_pa_id_len );
        if ( rc != 0 || p_seq_pa_id == NULL )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SEQUENCE table, PRIMARY_ALIGNMENT_ID column", seq_row_id, rc);

        const uint32_t * p_seq_read_len;
        // SEQ:READ_LEN
        rc = VCursorCellDataDirect ( seq_cursor, seq_row_id, seq_read_len_idx, NULL, (const void**)&p_seq_read_len, NULL, &data_len );
        if ( rc != 0 || p_seq_read_len == NULL )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SEQUENCE table, READ_LEN column", seq_row_id, rc);
        if ( seq_pa_id_len != data_len )
        {
            std::stringstream ss;
            ss << "SEQUENCE:" << seq_row_id << " PRIMARY_ALIGNMENT_ID length (" << seq_pa_id_len << ") does not match SEQUENCE:" << seq_row_id << " READ_LEN length (" << data_len << ")";

            throw DATA_ERROR(ss.str());
        }

        uint64_t sum_unaligned_read_len = 0;
        for ( uint32_t j = 0; j < seq_pa_id_len; ++j )
        {
            if ( p_seq_pa_id[j] == 0 )
            {
                sum_unaligned_read_len += p_seq_read_len[j];
            }
        }

        // SEQ:CMP_READ
        rc = VCursorCellDataDirect ( seq_cursor, seq_row_id, seq_cmp_read_idx, NULL, (const void**)&data_ptr, NULL, &data_len );
        if ( rc != 0 || data_ptr == NULL )
            throw VDB_ROW_ERROR("VCursorCellDataDirect() failed on SEQUENCE table, SEQ:CMP_READ column", seq_row_id, rc);

        if ( sum_unaligned_read_len != data_len )
        {
            std::stringstream ss;
            ss << "SEQUENCE:" << seq_row_id << " CMP_READ length (" << data_len << ") does not match sum of unaligned READ_LEN values (" << sum_unaligned_read_len << ")";

            throw DATA_ERROR(ss.str());
        }
    }

    if (sa_row_limit < sa_row_count || seq_row_limit < seq_row_count)
        PLOGMSG (klogInfo, (klogInfo, "$(ACC) looks good (based on first $(SA_CUTOFF) of SECONDARY_ALIGNMENT and $(SEQ_CUTOFF) SEQUENCE rows)", "ACC=%s,SA_CUTOFF=%lu,SEQ_CUTOFF=%lu", accession, sa_row_limit, seq_row_limit));
    else
        PLOGMSG (klogInfo, (klogInfo, "$(ACC) looks good", "ACC=%s", accession));
}

/**
 * returns true if accession is good
 */
bool checkAccession ( const char * accession, const CheckCorruptConfig * config )
{
    rc_t rc;
    KDirectory * cur_dir;
    const VDBManager * manager;
    const VDatabase * database;
    const VTable * pa_table;
    const VTable * sa_table;
    const VTable * seq_table;

    const VCursor * pa_cursor;
    const VCursor * sa_cursor;
    const VCursor * seq_cursor;

    rc = KDirectoryNativeDir( &cur_dir );
    if ( rc != 0 )
        PLOGERR( klogInt, (klogInt, rc, "$(ACC) KDirectoryNativeDir() failed", "ACC=%s", accession));
    else
    {
        rc = VDBManagerMakeRead ( &manager, cur_dir );
        if ( rc != 0 )
            PLOGERR( klogInt, (klogInt, rc, "$(ACC) VDBManagerMakeRead() failed", "ACC=%s", accession));
        else
        {
            int type = VDBManagerPathType ( manager, "%s", accession );
            if ( ( type & ~ kptAlias ) != kptDatabase )
                PLOGMSG (klogInfo, (klogInfo, "$(ACC) SKIPPING - can't be opened as a database", "ACC=%s", accession));
            else
            {
                rc = VDBManagerOpenDBRead( manager, &database, NULL, "%s", accession );
                if (rc != 0)
                    PLOGERR( klogInt, (klogInt, rc, "$(ACC) VDBManagerOpenDBRead() failed", "ACC=%s", accession));
                else
                {
                    rc = VDatabaseOpenTableRead( database, &pa_table, "%s", "PRIMARY_ALIGNMENT" );
                    if ( rc != 0 )
                    {
                        PLOGMSG (klogInfo, (klogInfo, "$(ACC) SKIPPING - failed to open PRIMARY_ALIGNMENT table", "ACC=%s", accession));
                        rc = 0;
                    }
                    else
                    {
                        rc = VTableCreateCursorRead( pa_table, &pa_cursor );
                        if ( rc != 0 )
                            PLOGERR( klogInt, (klogInt, rc, "$(ACC) VTableCreateCursorRead() failed for PRIMARY_ALIGNMENT cursor", "ACC=%s", accession));
                        else
                        {
                            rc = VDatabaseOpenTableRead( database, &sa_table, "%s", "SECONDARY_ALIGNMENT" );
                            if ( rc != 0 )
                            {
                                PLOGMSG (klogInfo, (klogInfo, "$(ACC) SKIPPING - failed to open SECONDARY_ALIGNMENT table", "ACC=%s", accession));
                                rc = 0;
                            }
                            else
                            {
                                rc = VTableCreateCursorRead( sa_table, &sa_cursor );
                                if ( rc != 0 )
                                    PLOGERR( klogInt, (klogInt, rc, "$(ACC) VTableCreateCursorRead() failed for SECONDARY_ALIGNMENT cursor", "ACC=%s", accession));
                                else
                                {
                                    rc = VDatabaseOpenTableRead( database, &seq_table, "%s", "SEQUENCE" );
                                    if ( rc != 0 )
                                    {
                                        PLOGMSG (klogInfo, (klogInfo, "$(ACC) SKIPPING - failed to open SEQUENCE table", "ACC=%s", accession));
                                        rc = 0;
                                    }
                                    else
                                    {
                                        rc = VTableCreateCursorRead( seq_table, &seq_cursor );
                                        if ( rc != 0 )
                                            PLOGERR( klogInt, (klogInt, rc, "VTableCreateCursorRead() failed for SEQUENCE cursor", "ACC=%s", accession));
                                        else
                                        {
                                            try {
                                                runChecks( accession, config, pa_cursor, sa_cursor, seq_cursor );
                                            } catch ( VDB_ERROR & x ) {
                                                PLOGERR (klogErr, (klogInfo, x.rc, "$(ACC) VDB error: $(MSG)", "ACC=%s,MSG=%s", accession, x.msg));
                                                rc = 1;
                                            } catch ( VDB_ROW_ERROR & x ) {
                                                PLOGERR (klogErr, (klogInfo, x.rc, "$(ACC) VDB error: $(MSG) row_id: $(ROW_ID)", "ACC=%s,MSG=%s,ROW_ID=%ld", accession, x.msg, x.row_id));
                                                rc = 1;
                                            } catch ( DATA_ERROR & x ) {
                                                KOutMsg("%s\n", accession);
                                                PLOGMSG (klogInfo, (klogInfo, "$(ACC) Invalid data: $(MSG) ", "ACC=%s,MSG=%s", accession, x.msg.c_str()));
                                                rc = 1;
                                            }
                                            VCursorRelease( seq_cursor );
                                        }
                                        VTableRelease( seq_table );
                                    }
                                    VCursorRelease( sa_cursor );
                                }
                                VTableRelease( sa_table );
                            }
                            VCursorRelease( pa_cursor );
                        }
                        VTableRelease( pa_table );
                    }
                    VDatabaseRelease( database );
                }
            }
            VDBManagerRelease( manager );
        }
        KDirectoryRelease( cur_dir );
    }
    return rc == 0;
}

//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>
#include <kapp/main.h>
#include <kapp/log-xml.h>

const char UsageDefaultName[] = "test-general-loader";

#define ALIAS_SA_CUTOFF    NULL
#define OPTION_SA_CUTOFF   "sa-cutoff"

static const char * sa_cutoff_usage[] = { "specify maximum amount of secondary alignment rows to look at before saying accession is good, default 100000.",
        "Specifying '0' will iterate the whole table. Can be in percent (e.g. 5%)",
        NULL };

#define ALIAS_SEQ_CUTOFF    NULL
#define OPTION_SEQ_CUTOFF   "seq-cutoff"

static const char * seq_cutoff_usage[] = { "specify maximum amount of sequence table rows to look at before saying accession is good, default 100000.",
        "Specifying '0' will iterate the whole table. Can be in percent (e.g. 5%)",
        NULL };

#define ALIAS_SA_SHORT_THRESHOLD NULL
#define OPTION_SA_SHORT_THRESHOLD "sa-short-threshold"
static const char * sa_short_threshold_usage[] = { "specify amount of secondary alignment which are shorter (hard-clipped) than corresponding primaries, default 1%.",
        NULL };

OptDef Options[] = {
      { OPTION_SA_CUTOFF          , ALIAS_SA_CUTOFF          , NULL, sa_cutoff_usage            , 1, true , false },
      { OPTION_SEQ_CUTOFF         , ALIAS_SEQ_CUTOFF         , NULL, seq_cutoff_usage           , 1, true , false },
      { OPTION_SA_SHORT_THRESHOLD , ALIAS_SA_SHORT_THRESHOLD , NULL, sa_short_threshold_usage   , 1, true , false }
};

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] path [path ...]\n"
        "\n"
        "Summary:\n"
        "  Validate a list of runs for corrupted data\n"
        "\n", progname);
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ("Options:\n");

    HelpOptionLine(ALIAS_SA_CUTOFF          , OPTION_SA_CUTOFF           , "cutoff"    , sa_cutoff_usage);
    HelpOptionLine(ALIAS_SEQ_CUTOFF         , OPTION_SEQ_CUTOFF          , "cutoff"    , seq_cutoff_usage);
    HelpOptionLine(ALIAS_SA_SHORT_THRESHOLD , OPTION_SA_SHORT_THRESHOLD  , "threshold" , sa_short_threshold_usage);
    XMLLogger_Usage();

    KOutMsg ("\n");

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

rc_t parseArgs ( Args * args, CheckCorruptConfig * config )
{
    rc_t rc;
    uint32_t opt_count;
    rc = ArgsOptionCount ( args, OPTION_SA_CUTOFF, &opt_count );
    if (rc)
    {
        LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SA_CUTOFF);
        return rc;
    }

    if (opt_count > 0)
    {
        const char * value;
        size_t value_size;
        rc = ArgsOptionValue ( args, OPTION_SA_CUTOFF, 0, (const void **) &value );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SA_CUTOFF);
            return rc;
        }

        value_size = string_size ( value );
        if ( value_size >= 1 && value[value_size - 1] == '%' )
        {
            config->sa_cutoff_percent = string_to_U64 ( value, value_size - 1, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SA_CUTOFF);
                return rc;
            }
            else if (config->sa_cutoff_percent == 0 || config->sa_cutoff_percent > 100)
            {
                LOGERR (klogInt, rc, OPTION_SA_CUTOFF " has illegal percentage value (has to be 1-100%)" );
                return 1;
            }
            config->sa_cutoff_percent /= 100;
        }
        else
        {
            config->sa_cutoff_percent = -1;
            config->sa_cutoff_number = string_to_U64 ( value, value_size, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SA_CUTOFF);
                return rc;
            }
        }
    }

    rc = ArgsOptionCount ( args, OPTION_SEQ_CUTOFF, &opt_count );
    if (rc)
    {
        LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SEQ_CUTOFF);
        return rc;
    }

    if (opt_count > 0)
    {
        const char * value;
        size_t value_size;
        rc = ArgsOptionValue ( args, OPTION_SEQ_CUTOFF, 0, (const void **) &value );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SEQ_CUTOFF);
            return rc;
        }

        value_size = string_size ( value );
        if ( value_size >= 1 && value[value_size - 1] == '%' )
        {
            config->seq_cutoff_percent = string_to_U64 ( value, value_size - 1, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SEQ_CUTOFF);
                return rc;
            }
            else if (config->seq_cutoff_percent == 0 || config->seq_cutoff_percent > 100)
            {
                LOGERR (klogInt, rc, OPTION_SEQ_CUTOFF " has illegal percentage value (has to be 1-100%)" );
                return 1;
            }
            config->seq_cutoff_percent /= 100;
        }
        else
        {
            config->seq_cutoff_percent = -1;
            config->seq_cutoff_number = string_to_U64 ( value, value_size, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SEQ_CUTOFF);
                return rc;
            }
        }
    }

    rc = ArgsOptionCount ( args, OPTION_SA_SHORT_THRESHOLD, &opt_count );
    if (rc)
    {
        LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SA_SHORT_THRESHOLD);
        return rc;
    }

    if (opt_count > 0)
    {
        const char * value;
        size_t value_size;
        rc = ArgsOptionValue ( args, OPTION_SA_SHORT_THRESHOLD, 0, (const void **) &value );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SA_SHORT_THRESHOLD);
            return rc;
        }

        value_size = string_size ( value );
        if ( value_size >= 1 && value[value_size - 1] == '%' )
        {
            config->pa_len_threshold_percent = string_to_U64 ( value, value_size - 1, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SA_SHORT_THRESHOLD);
                return rc;
            }
            else if (config->pa_len_threshold_percent == 0 || config->pa_len_threshold_percent > 100)
            {
                LOGERR (klogInt, rc, OPTION_SA_SHORT_THRESHOLD " has illegal percentage value (has to be 1-100%)" );
                return 1;
            }
            config->pa_len_threshold_percent /= 100;
        }
        else
        {
            config->pa_len_threshold_percent = -1;
            config->pa_len_threshold_number = string_to_U64 ( value, value_size, &rc );
            if (rc)
            {
                LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SA_SHORT_THRESHOLD);
                return rc;
            }
        }
    }

    return 0;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    XMLLogger const *xlogger = NULL;
    Args * args;
    rc_t rc;
    bool any_failed = false;
    CheckCorruptConfig config = { -1.0, SA_TABLE_LOOKUP_LIMIT, -1.0, SEQ_TABLE_LOOKUP_LIMIT, PA_LONGER_SA_LIMIT, 0 };

    KLogLevelSet(klogInfo);

    rc = ArgsMakeAndHandle (&args, argc, app.getArgV(), 2, Options,
                            sizeof (Options) / sizeof (Options[0]),
                            XMLLogger_Args, XMLLogger_ArgsQty);
    if (rc)
        LOGERR (klogInt, rc, "failed to parse command line parameters");
    else
    {
        rc = XMLLogger_Make(&xlogger, NULL, args);
        if (rc)
            LOGERR (klogInt, rc, "failed to make xml logger");
        else
        {
            rc = parseArgs ( args, &config );
            if (rc == 0)
            {
                uint32_t pcount;
                rc = ArgsParamCount ( args, &pcount );
                if (rc)
                    LOGERR (klogInt, rc, "ArgsParamCount() failed");
                else
                {
                    if ( pcount == 0 )
                        LOGMSG (klogErr, "no accessions were passed in");
                    else
                    {
                        for ( uint32_t i = 0; i < pcount; ++i )
                        {
                            const char * accession;
                            rc = ArgsParamValue ( args, i, (const void **)&accession );
                            if (rc)
                            {
                                PLOGERR (klogInt, (klogInt, rc, "failed to get $(PARAM_I) accession from command line", "PARAM_I=%d", i));
                                any_failed = true;
                            }
                            else
                            {
                                if (!checkAccession ( accession, &config ))
                                    any_failed = true;
                            }
                        }

                        if (!any_failed)
                            LOGMSG (klogInfo, "All accessions are good!");
                    }
                }
            }
            XMLLogger_Release(xlogger);
        }
        ArgsWhack ( args );
    }
    app.setRc( rc );
    return app.getExitCode();
}

}
