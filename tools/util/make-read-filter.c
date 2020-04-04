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

#include <kapp/main.h>
#include <kapp/args.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>
#include <kdb/meta.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/rc.h>
#include <sra/sradb.h>

#include <assert.h>
#include <inttypes.h>

#include <sysexits.h>

static char const *input;
static char const *table;
static char const *schemaFile;
static char const *includePath;
static char const *outputSchemaType;
static char const *output;

typedef struct CellData {
    union {
        uint32_t const *u32;
        int32_t const *i32;
        uint8_t const *u8;
        void const *raw;
    } u;
    uint32_t count;
    uint32_t elem_bits;
} CellData;

/* Quote:
    Reads that have more than half of quality score values <20 will be flagged
    ‘reject’. Reads that begin or end with a run of more than 10 quality scores
    <20 are also flagged ‘reject’.
 */
/** @brief Apply the rules above to determine if a read should be filtered
 **/
static bool shouldFilter(uint32_t const len, uint8_t const *const qual)
{
    uint32_t under = 0;
    uint32_t first = len;
    uint32_t last = len;
    uint32_t i;
    
    for (i = 0; i < len; ++i) {
        int const qval = qual[i];
        if (qval < 20) {
            under += 1;
            continue;
        }
        last = i;
        if (first == len)
            first = i;
    }
    if (under * 2 > len || (len > 10 && (first > 10 || last < len - 10)))
        return true;
    return false;
}

/** @brief If all biological reads have already been filtered, then just copy it.
 **/
static bool shouldCopy(CellData const *const filterData, CellData const *const typeData)
{
    int i;
    int not_passed = 0;
    int bio_count = 0;
    int nreads = filterData->count;
    uint8_t const *filter = &filterData->u.u8[0];
    uint8_t const *type = &typeData->u.u8[0];
    assert(nreads == typeData->count);
    for (i = 0; i < nreads; ++i) {
        if ((type[i] & SRA_READ_TYPE_BIOLOGICAL) != SRA_READ_TYPE_BIOLOGICAL)
            continue;
        ++bio_count;
        if (filter[i] != SRA_READ_FILTER_PASS)
            ++not_passed;
    }
    return not_passed == bio_count;
}

static void test(void)
{
#if 0
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
#endif
}

static void updateReadFilter(uint8_t *const out_filter
                            , CellData const *const filterData
                            , CellData const *const typeData
                            , CellData const *const startData
                            , CellData const *const lenData
                            , CellData const *const qualData)
{
    int const nreads = filterData->count;
    int i;
    uint8_t const *const filter = &filterData->u.u8[0];
    uint8_t const *const type = &typeData->u.u8[0];
    int32_t const *const start = &startData->u.i32[0];
    uint32_t const *const len = &lenData->u.u32[0];
    uint8_t const *const qual = &qualData->u.u8[0];
    
    assert(nreads == typeData->count);
    assert(nreads == startData->count);
    assert(nreads == lenData->count);
    assert(qualData->count == start[nreads - 1] + len[nreads - 1]);

    memmove(out_filter, filter, nreads * sizeof(out_filter[0]));
    for (i = 0; i < nreads; ++i) {
        if ((type[i] & SRA_READ_TYPE_BIOLOGICAL) != SRA_READ_TYPE_BIOLOGICAL)
            continue;
        if (filter[i] != SRA_READ_FILTER_PASS)
            continue;
        if (shouldFilter(len[i], qual + start[i]))
            out_filter[i] = SRA_READ_FILTER_REJECT;
    }
}

static CellData cellData(VCursor const *const curs, int64_t const row, int const col, char const *const colName)
{
    uint32_t bit_offset = 0;
    CellData result = { {0}, 0, 0 };
    rc_t rc = VCursorCellDataDirect(curs, row, col, &result.elem_bits, &result.u.raw, &bit_offset, &result.count);
    if (rc) {
        pLogErr(klogFatal, rc, "Failed to read $(col) at row ($row)", "col=%s,row=%"PRId64, colName, row);
        exit(EX_DATAERR);
    }
    assert(bit_offset == 0);
    assert((result.elem_bits & 0x7) == 0);
    return result;
}

static uint64_t rowCount(VCursor const *const curs, int64_t *const first)
{
    uint64_t count = 0;
    rc_t const rc = VCursorIdRange(curs, 0, first, &count);
    assert(rc == 0);
    return count;
}

static uint32_t addColumn(VCursor const *const curs
                         , char const *const type
                         , char const *const name)
{
    uint32_t cid = 0;
    rc_t const rc = VCursorAddColumn(curs, &cid, "(%s)%s", type, name);
    if (rc == 0)
        return cid;

    pLogErr(klogFatal, rc, "Failed to open $(name) column", "name=%s", name);
    exit(EX_NOINPUT);
}

static void processCursors(VCursor *const out, VCursor const *const in)
{
    size_t out_filter_size = 1024;
    uint8_t *out_filter = malloc(out_filter_size); // sized by nreads

    uint32_t cid_read_filter = 0;
    uint32_t cid_read_type = 0;
    uint32_t cid_readstart = 0;
    uint32_t cid_readlen = 0;
    uint32_t cid_qual = 0;
    uint32_t cid_rd_filter = 0;
    int64_t first = 0;
    uint64_t count = 0;
    uint64_t r = 0;
    
    if (out_filter == NULL)
        goto OUT_OF_MEMORY;
        
    cid_readstart   = addColumn(in, "I32", "READ_START");
    cid_readlen     = addColumn(in, "U32", "READ_LEN");
    cid_qual        = addColumn(in, "U8", "QUALITY");
    cid_read_filter = addColumn(in, "U8", "READ_FILTER");
    cid_read_type   = addColumn(in, "U8", "READ_TYPE");
    {
        rc_t const rc = VCursorOpen(in);
        if (rc) {
            LogErr(klogFatal, rc, "Failed to open input cursor");
            exit(EX_NOINPUT);
        }
    }
    
    cid_rd_filter = addColumn(out, "U8", "RD_FILTER");
    {
        rc_t const rc = VCursorOpen(out);
        if (rc) {
            LogErr(klogFatal, rc, "Failed to open output cursor");
            exit(EX_CANTCREAT);
        }
    }
    count = rowCount(in, &first);
    assert(first == 1);
    for (r = 0; r < count; ++r) {
        assert(r < INT64_MAX);
        {
            rc_t const rc = VCursorOpenRow(out);
            if (rc) {
                pLogErr(klogFatal, rc, "Failed to open a new row $(row)", "row=%"PRIu64, r + 1);
                exit(EX_IOERR);
            }
        }
        {
            int64_t const row = first + r;
            CellData const readfilter = cellData(in, row, cid_read_filter, "READ_FILTER");
            CellData const readtype   = cellData(in, row, cid_read_type  , "READ_TYPE");
            if (shouldCopy(&readfilter, &readtype)) {
                rc_t const rc = VCursorWrite(out, cid_rd_filter, readfilter.elem_bits, readfilter.u.raw, 0, readfilter.count);
                if (rc) {
                    pLogErr(klogFatal, rc, "Failed to write row $(row)", "row=%"PRIu64, r + 1);
                    exit(EX_IOERR);
                }
            }
            else {
                CellData const readlen   = cellData(in, row, cid_readlen  , "READ_LEN");
                CellData const quality   = cellData(in, row, cid_qual     , "QUALITY");
                CellData const readstart = cellData(in, row, cid_readstart, "READ_START");
                
                if (out_filter_size < readfilter.count * sizeof(out_filter[0])) {
                    free(out_filter);
                    while (out_filter_size < readfilter.count * sizeof(out_filter[0]))
                        out_filter_size *= 2;
                    out_filter = malloc(out_filter_size);
                    if (out_filter == NULL)
                        goto OUT_OF_MEMORY;
                }
                updateReadFilter(out_filter, &readfilter, &readtype, &readstart, &readlen, &quality);
                {
                    rc_t const rc = VCursorWrite(out, cid_rd_filter, readfilter.elem_bits, out_filter, 0, readfilter.count);
                    if (rc) {
                        pLogErr(klogFatal, rc, "Failed to write row $(row)", "row=%"PRIu64, r + 1);
                        exit(EX_IOERR);
                    }
                }
            }
        }
        {
            rc_t const rc = VCursorCommitRow(out);
            if (rc) {
                pLogErr(klogFatal, rc, "Failed to commit row $(row)", "row=%"PRIu64, r + 1);
                exit(EX_IOERR);
            }
        }
        {
            rc_t const rc = VCursorCloseRow(out);
            if (rc) {
                pLogErr(klogFatal, rc, "Failed to close row $(row)", "row=%"PRIu64, r + 1);
                exit(EX_IOERR);
            }
        }
    }
    {
        rc_t const rc = VCursorCommit(out);
        if (rc) {
            LogErr(klogFatal, rc, "Failed to commit cursor");
            exit(EX_IOERR);
        }
    }
    free(out_filter);
    VCursorRelease(out);
    VCursorRelease(in);
    return;
    
OUT_OF_MEMORY:
    LogErr(klogFatal, RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "OUT OF MEMORY!!!");
    exit(EX_TEMPFAIL);
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
    {
        rc_t const rc = VTableCreateCursorRead(input, &in);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create input cursor!");
            exit(EX_NOINPUT);
        }
    }
    processCursors(out, in);
    VTableRelease(output);
    VTableRelease(input);
}

static void processTable(VDBManager *const mgr
                        , VTable const *const input)
{
    VSchema *schema = NULL;
    {
        rc_t const rc = VDBManagerMakeSchema(mgr, &schema);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to make a schema object!");
            exit(EX_TEMPFAIL);
        }
    }
    {
        rc_t const rc = VSchemaAddIncludePath(schema, "%s", includePath);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to add include path");
            exit(EX_NOINPUT);
        }
    }
    {
        rc_t const rc = VSchemaParseFile(schema, "%s", schemaFile);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to add load schema file");
            exit(EX_NOINPUT);
        }
    }
    {
        VDatabase *db = NULL;
        rc_t const rc = VDBManagerCreateDB(mgr, &db, schema, outputSchemaType
                                          , kcmInit + kcmMD5, "%s", output);
        VSchemaRelease(schema);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output database");
            exit(EX_CANTCREAT);
        }
        {
            VTable *output = NULL;
            rc_t const rc = VDatabaseCreateTable(db, &output, outputSchemaType, kcmCreate | kcmMD5, "%s", "SEQUENCE");
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output table");
                exit(EX_CANTCREAT);
            }
            {
                rc_t const rc = VTableColumnCreateParams(output, kcmCreate, kcsCRC32, 0);
                if (rc != 0) {
                    LogErr(klogFatal, rc, "VTableColumnCreateParams failed!?");
                    exit(EX_SOFTWARE);
                }
            }
            processTables(output, input);
        }
        VDatabaseRelease(db);
    }
    VDBManagerRelease(mgr);
    VTableRelease(input);
}

/* Open table contained in a database
 */
static void processDatabaseInput(VDBManager *const mgr)
{
    VDatabase const *db = NULL;
    VTable const *in = NULL;
    rc_t rc = VDBManagerOpenDBRead(mgr, &db, NULL, "%s", input);
    if (rc != 0) {
        pLogErr(klogFatal, rc, "Can't open database $(input)", "input=%s", input);
        exit(EX_DATAERR);
    }
    rc = VDatabaseOpenTableRead(db, &in, "%s", table);
    VDatabaseRelease(db);
    if (rc != 0) {
        pLogErr(klogFatal, rc, "Can't open table $(table) of database $(input)", "input=%s,table=%s", input, table);
        exit(EX_DATAERR);
    }
    processTable(mgr, in);
}

/* Open un-contained table
 */
static void processTableInput(VDBManager *const mgr)
{
    VTable const *in = NULL;
    rc_t const rc = VDBManagerOpenTableRead(mgr, &in, NULL, "%s", input);
    if (rc != 0) {
        pLogErr(klogFatal, rc, "Can't open table $(input)", "input=%s", input);
        exit(EX_DATAERR);
    }
    processTable(mgr, in);
}

static VDBManager *manager()
{
    VDBManager *mgr = NULL;
    rc_t const rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc == 0) return mgr;
    LogErr(klogFatal, rc, "VDBManagerMake failed!");
    exit(EX_TEMPFAIL);
}

#define PATH_TYPE_ISA_DATABASE(TYPE) ((TYPE | kptAlias) == (kptDatabase | kptAlias))
#define PATH_TYPE_ISA_TABLE(TYPE) ((TYPE | kptAlias) == (kptTable | kptAlias))
static int pathType(VDBManager const *mgr, char const *path)
{
    KDBManager const *kmgr = 0;
    rc_t rc = VDBManagerOpenKDBManagerRead(mgr, &kmgr);
    if (rc == 0) {
        int const type = KDBManagerPathType(kmgr, "%s", path);
        KDBManagerRelease(kmgr);
        return type;
    }
    LogErr(klogFatal, rc, "VDBManagerOpenKDBManager failed!");
    exit(EX_TEMPFAIL);
}

static void process()
{
    pLogMsg(klogInfo, "processing from table $(table) of $(input) to table $(table) of $(output)", "input=%s,output=%s,table=%s", input, output, table);
    {
        VDBManager *const mgr = manager();
        int const inputType = pathType(mgr, input);
        if (PATH_TYPE_ISA_DATABASE(inputType)) {
            if (table) {
                processDatabaseInput(mgr);
            }
            else {
                VDBManagerRelease(mgr);
                pLogMsg(klogFatal, "input $(input) is a database, input table name is required", "input=%s", input);
                exit(EX_USAGE);
            }
        }
        else if (PATH_TYPE_ISA_TABLE(inputType)) {
            processTableInput(mgr);
        }
        else {
            VDBManagerRelease(mgr);
            pLogMsg(klogFatal, "input $(input) is not a table or database", "input=%s", input);
            exit(EX_NOINPUT);
        }
    }
}

#define OPT_DEF_VALUE(N, A, H) { N, A, NULL, H, 1, true, false }
#define OPT_DEF_REQUIRED_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }

static char const *input_help[]  = { "input to process (!= output)", NULL };
static char const *table_help[]  = { "table to process, e.g. 'SEQUENCE'", NULL };
static char const *output_help[] = { "output directory", NULL };
static char const *tschema_help[] = { "output table schema type", NULL };
static char const *ischema_help[] = { "schema include path", NULL };
static char const *schema_help[] = { "schema file name", NULL };
static OptDef Options [] = {
    OPT_DEF_REQUIRED_VALUE("input" , "i", input_help),
    OPT_DEF_VALUE         ("table" , "t", table_help),
    OPT_DEF_REQUIRED_VALUE("output", "o", output_help),
    OPT_DEF_REQUIRED_VALUE("outtype", "T", tschema_help),
    OPT_DEF_REQUIRED_VALUE("include", "I", ischema_help),
    OPT_DEF_REQUIRED_VALUE("schema", "s", schema_help),
};
#define OPT_INPUT 0
#define OPT_TABLE 1
#define OPT_OUTPUT 2
#define OPT_TYPE 3
#define OPT_INCLUDE 4
#define OPT_SCHEMA 5

#define OPT_NAME(n) Options[n].name

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

static rc_t getArgValue(char **result, Args *const args, int opt, int index)
{
    void const *value;
    rc_t const rc = ArgsOptionValue(args, Options[opt].name, index, &value);
    if (rc) return rc;
    free((void *)*result);
    *result = strdup(value);
    assert(*result);
    return 0;
}

rc_t CC KMain(int argc, char *argv[])
{
    char *s_input;
    char *s_table;
    char *s_schemaFile;
    char *s_includePath;
    char *s_outputSchemaType;
    char *s_output;

    test();
    {
        Args *args = NULL;
        rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, 3);
        if ( rc != 0 )
            LogErr ( klogErr, rc, "failed to parse arguments" );
        else
        {
            rc = getArgValue(&s_input, args, OPT_INPUT, 0);
            assert(rc == 0);
        
            rc = getArgValue(&s_table, args, OPT_TABLE, 0);
            assert(rc == 0);

            rc = getArgValue(&s_output, args, OPT_OUTPUT, 0);
            assert(rc == 0);
            
            rc = getArgValue(&s_outputSchemaType, args, OPT_TYPE, 0);
            assert(rc == 0);
            
            rc = getArgValue(&s_schemaFile, args, OPT_SCHEMA, 0);
            assert(rc == 0);
            
            rc = getArgValue(&s_includePath, args, OPT_INCLUDE, 0);
            assert(rc == 0);
            
            input = s_input;
            table = s_table;
            schemaFile = s_schemaFile;
            includePath = s_includePath;
            outputSchemaType = s_outputSchemaType;
            output = s_output;
            
            process();
            ArgsWhack(args);
        }
        return rc;
    }
}
