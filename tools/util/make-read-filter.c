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

/* Synopsis: Updates read filter based on quality scores
 * Purpose: In preparation for removing quality scores
 * Example usage:
 *  make-read-filter --include ncbi-vdb/interfaces \
 *                   --schema ncbi-vdb/interfaces/illumina.vschema \
 *                   --output SRR123456.converted
 *                   --input SRR123456
 */

#include "make-read-filter.h" /* contains mostly boilerplate code */

/* NOTE: Needs to be in same order as Options array */
enum OPTIONS {
    OPT_INPUT,      /* input database or table path */
    OPT_TABLE,      /* input table name if input is a database */
    OPT_OUTPUT,     /* output path */
    OPT_INCLUDE,    /* schema include directory */
    OPT_SCHEMA,     /* schema file path */
    OPTIONS_COUNT
};

/* NOTE: Rules for filtering
    Quote:
        Reads that have more than half of quality score values <20 will be
        flagged ‘reject’.
        Reads that begin or end with a run of more than 10 quality scores <20
        are also flagged ‘reject’.
 */
/** @brief Apply the rules to determine if a read should be filtered
 **/
static bool shouldFilter(uint32_t const len, uint8_t const *const qual)
{
    uint32_t under = 0;
    uint32_t last = len;
    uint32_t i;
    
    for (i = 0; i < len; ++i) {
        int const qval = qual[i];
        if (qval < 20) {
            if ((++under) * 2 > len)
                return true;
        }
        else
            last = i;
    }
    if (len <= 10)
        return false;
        
    if (len - last > 10)
        return true;

    for (i = 0; i < len; ++i) {
        int const qval = qual[i];
        if (qval >= 20)
            return false;
        if (i == 10)
            return true;
    }
    assert(!"reachable");
}

static void computeReadFilter(uint8_t *const out_filter
                             , CellData const *const filterData
                             , CellData const *const typeData
                             , CellData const *const startData
                             , CellData const *const lenData
                             , CellData const *const qualData)
{
    int const nreads = filterData->count;
    uint8_t const *const filter = filterData->data;
    uint8_t const *const type = typeData->data;
    int32_t const *const start = startData->data;
    uint32_t const *const len = lenData->data;
    uint8_t const *const qual = qualData->data;
    int i;
    
    assert(nreads == typeData->count);
    assert(nreads == startData->count);
    assert(nreads == lenData->count);
    assert(qualData->count == start[nreads - 1] + len[nreads - 1]);

    for (i = 0; i < nreads; ++i) {
        uint8_t filt = filter[i];
        
        if (   filt == SRA_READ_FILTER_PASS
            && (type[i] & SRA_READ_TYPE_BIOLOGICAL) == SRA_READ_TYPE_BIOLOGICAL
            && shouldFilter(len[i], qual + start[i])
           )
        {
            filt = SRA_READ_FILTER_REJECT;
        }
        out_filter[i] = filt;
    }
}

static void processCursors(VCursor *const out, VCursor const *const in)
{
    /* MARK: input columns */
    uint32_t const cid_read_filter = addColumn("READ_FILTER", "U8" , in);
    uint32_t const cid_readstart   = addColumn("READ_START" , "I32", in);
    uint32_t const cid_read_type   = addColumn("READ_TYPE"  , "U8" , in);
    uint32_t const cid_readlen     = addColumn("READ_LEN"   , "U32", in);
    uint32_t const cid_qual        = addColumn("QUALITY"    , "U8" , in);

    /* MARK: output column */
    uint32_t const cid_rd_filter = addColumn("READ_FILTER", "U8", out);

    int64_t first = 0;
    uint64_t count = 0;

    uint64_t r = 0;
    size_t out_filter_count = 0;
    uint8_t *out_filter = NULL;

    openCursor(in, "input");    
    openCursor(out, "output");
    
    count = rowCount(in, &first);
    assert(first == 1);
    /* MARK: Main loop over the input */
    for (r = 0; r < count; ++r) {
        int64_t const row = 1 + r;
        CellData const readfilter = cellData("READ_FILTER", cid_read_filter, row, in);
        CellData const readstart  = cellData("READ_START" , cid_readstart  , row, in);
        CellData const readtype   = cellData("READ_TYPE"  , cid_read_type  , row, in);
        CellData const readlen    = cellData("READ_LEN"   , cid_readlen    , row, in);
        CellData const quality    = cellData("QUALITY"    , cid_qual       , row, in);

        out_filter = growFilterBuffer(out_filter, &out_filter_count, readfilter.count);
        computeReadFilter(out_filter, &readfilter, &readtype, &readstart, &readlen, &quality);
        openRow(row, out);
        writeRow(row, readfilter.count, out_filter, cid_rd_filter, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    commitCursor(out);
    free(out_filter);
    VCursorRelease(out);
    VCursorRelease(in);
}

/* MARK: the main action starts here */
void main_1(int argc, char *argv[])
{
    Args *const args = getArgs(argc, argv);
    VDBManager *const mgr = manager();
    bool noDb = false;
    char const *schemaType = NULL;
    VTable const *const in = openInput(args, mgr, &noDb, &schemaType);
    VTable *const out = createOutput(args, mgr, noDb, schemaType);
    
    VDBManagerRelease(mgr);
    ArgsWhack(args);
    
    processTables(out, in);
}

static void processTables(VTable *const output, VTable const *const input)
{
    VCursor *out = NULL;
    VCursor const *in = NULL;
    {
        rc_t const rc = VTableCreateCursorWrite(output, &out, kcmInsert);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output cursor!");
            exit(EX_CANTCREAT);
        }
    }
    VTableRelease(output);
    {
        rc_t const rc = VTableCreateCursorRead(input, &in);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create input cursor!");
            exit(EX_NOINPUT);
        }
    }
    VTableRelease(input);
    processCursors(out, in);
}

static VTable *createOutput(  Args *const args
                            , VDBManager *const mgr
                            , bool noDb
                            , char const *schemaType)
{
    VTable *out = NULL;
    VSchema *schema = NULL;
    VDatabase *db = NULL;
    {
        rc_t const rc = VDBManagerMakeSchema(mgr, &schema);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to make a schema object!");
            exit(EX_TEMPFAIL);
        }
    }
    {
        rc_t const rc = VSchemaAddIncludePath(schema, "%s", getArgValue(OPT_INCLUDE, args));
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to add include path");
            exit(EX_NOINPUT);
        }
    }
    {
        rc_t const rc = VSchemaParseFile(schema, "%s", getArgValue(OPT_SCHEMA, args));
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to add load schema file");
            exit(EX_NOINPUT);
        }
    }
    if (noDb) {
        rc_t const rc = VDBManagerCreateTable(mgr, &out, schema
                                              , schemaType
                                              , kcmInit + kcmMD5, "%s"
                                              , getArgValue(OPT_OUTPUT, args));
        VSchemaRelease(schema);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output table");
            exit(EX_CANTCREAT);
        }
    }
    else {
        {
            rc_t const rc = VDBManagerCreateDB(mgr, &db, schema
                                              , schemaType
                                              , kcmInit + kcmMD5, "%s"
                                              , getArgValue(OPT_OUTPUT, args));
            VSchemaRelease(schema);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output database");
                exit(EX_CANTCREAT);
            }
        }
        {
            rc_t const rc = VDatabaseCreateTable(db, &out, "SEQUENCE", kcmCreate | kcmMD5, "SEQUENCE");
            VDatabaseRelease(db);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output table");
                exit(EX_CANTCREAT);
            }
        }
    }
    return out;
}

static uint8_t *growFilterBuffer(uint8_t *out_filter, size_t *const out_filter_count, size_t needed)
{
    if (*out_filter_count < needed) {
        free(out_filter);
        *out_filter_count = needed;
        pLogMsg(klogInfo, "increasing max read count to $(max)", "max=%zu", needed);
        out_filter = malloc(needed * sizeof(out_filter[0]));
        if (out_filter == NULL)
            OUT_OF_MEMORY();
    }
    return out_filter;
}

static void test(void)
{
    uint8_t qual[30];
    int i;
    
    assert(shouldFilter(0, NULL) == false);
    
    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    assert(shouldFilter(30, qual) == false);

    for (i = 0; i < 30; ++i)
        qual[i] = (i & 1) ? 30 : 3;
    qual[19] = 3;
    assert(shouldFilter(30, qual) == true);
    
    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    for (i = 0; i < 11; ++i)
        qual[i] = 19;
    assert(shouldFilter(30, qual) == true);

    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    for (i = 19; i < 30; ++i)
        qual[i] = 19;
    assert(shouldFilter(30, qual) == true);
}

#define OPT_DEF_VALUE(N, A, H) { N, A, NULL, H, 1, true, false }
#define OPT_DEF_REQUIRED_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }
#define OPT_NAME(n) Options[n].name

static char const *input_help[]  = { "input to process (!= output)", NULL };
static char const *table_help[]  = { "table to process, e.g. 'SEQUENCE'", NULL };
static char const *output_help[] = { "output directory", NULL };
static char const *ischema_help[] = { "schema include path", NULL };
static char const *schema_help[] = { "schema file name", NULL };

/* MARK: Options array */
static OptDef Options [] = {
    OPT_DEF_REQUIRED_VALUE("input" , "i", input_help),
    OPT_DEF_VALUE         ("table" , "t", table_help),
    OPT_DEF_REQUIRED_VALUE("output", "o", output_help),
    OPT_DEF_REQUIRED_VALUE("include", "I", ischema_help),
    OPT_DEF_REQUIRED_VALUE("schema", "s", schema_help),
};

/* MARK: Mostly boilerplate from here */

rc_t CC KMain(int argc, char *argv[])
{
#if _DEBUGGING
    test();
#endif
    main_1(argc, argv);
    return 0;
}

const char UsageDefaultName[] = "make-read-filter";

rc_t CC UsageSummary ( const char *progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s [options]\n"
                     "\n"
                     "Summary:\n"
                     "  Make RD_FILTER from QUALITY.\n"
                     , progname
        );
}

rc_t CC Usage ( const Args *args )
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

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion () );

    return rc;
}

static Args *getArgs(int argc, char *argv[])
{
    Args *args = NULL;
    rc_t const rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, OPTIONS_COUNT);
    if (rc == 0)
        return args;
    
    LogErr(klogErr, rc, "failed to parse arguments");
    exit(EX_USAGE);
}

VTable const *openInput(Args *const args, VDBManager const *const mgr, bool *const noDb, char const **const schemaType)
{
    char const *const input = getArgValue(OPT_INPUT, args);
    int const inputType = pathType(mgr, input);

    if (PATH_TYPE_ISA_DATABASE(inputType)) {
        *noDb = false;
        return dbOpenTable(input, getArgValue(OPT_TABLE, args), mgr, schemaType);
    }
    else if (PATH_TYPE_ISA_TABLE(inputType)) {
        *noDb = true;
        return openTable(input, mgr, schemaType);
    }
    else {
        LogMsg(klogFatal, "input is not a table or database");
        exit(EX_NOINPUT);
    }
}

static char const *getArgValue(int opt, Args *const args)
{
    void const *value = NULL;
    rc_t const rc = ArgsOptionValue(args, Options[opt].name, 0, &value);
    if (rc == 0)
        return value;
        
    pLogErr(klogFatal, rc, "Failed to get $(arg) value", "arg=%s", Options[opt].name);
    exit(EX_USAGE);
}
