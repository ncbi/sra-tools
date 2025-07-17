#include <kapp/main.h>
#include <kapp/args.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>
#include <klib/text.h>

#include <assert.h>
#include <os-native.h>


/*

  default file is "ncbi/seq.vschema"
  default table-spec "NCBI:tbl:base_space#2"

*/
#define SCHEMASPEC "ncbi/seq.vschema"
#define TYPESPEC "NCBI:tbl:base_space#2"

const char UsageDefaultName[] = "sortreadtest";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [OPTIONS] file-path table-path [schema-path]\n"
                    "\n"
                    "Summary:\n"
                    "  Write lines from file as rows to table using schema\n"
                    "\n", progname);
}


rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    OUTMSG (("Options:\n"));

    HelpOptionsStandard ();

    return rc;
}

typedef struct param_block
{
    const char * file_path;
    const char * table_path;
    const char * schema_path;

    KDirectory * pwd;
    const KFile * file;
    const KMMap * mmap;
    VDBManager * mgr;
    VSchema * schema;
    VTable * table;
    VCursor * cursor;

    const char * seq;
    const char * eof;
    size_t seq_size;


    uint32_t    idx; /* column idx in the cursor for READ (should be 0 */

}  param_block;

rc_t get_a_sequence (param_block * pb)
{
    char * eol;
    rc_t rc = 0;
    if (pb->seq == NULL)        /* first call */
    {
        const void * annoying;
        uint64_t file_pos;
        size_t map_size;
        uint64_t file_size;

        rc = KFileSize (pb->file, &file_size);
        if (rc)
            return rc;

        rc = KMMapAddrRead (pb->mmap, &annoying);
        if (rc)
            return rc;

        pb->seq = annoying;

        if (pb->seq == NULL)
            return 0;

        rc = KMMapPosition (pb->mmap, &file_pos);
        if (rc)
            return rc;

        if (file_pos != 0)
        {
            rc = RC (rcExe, rcMemMap, rcAccessing, rcOffset, rcInvalid);
            return rc;
        }

        rc = KMMapSize (pb->mmap, &map_size);
        if (rc)
            return rc;

        if (map_size != file_size)
        {
            rc = RC (rcExe, rcMemMap, rcAccessing, rcFile, rcInvalid);
            return rc;
        }
        pb->eof = pb->seq + map_size;
    }
    else
    {
        pb->seq += pb->seq_size + 1;

        if (pb->seq >= pb->eof)
        {
            pb->seq = NULL;
            return 0;
        }
    }

    eol = string_chr (pb->seq, pb->eof - pb->seq, '\n');
    if (eol == NULL)
        pb->seq_size = pb->eof - pb->seq;
    else
        pb->seq_size = eol - pb->seq;

    return rc;
}


rc_t write_rows (param_block * pb)
{
    rc_t rc = 0;

    pb->seq = NULL;

    do
    {
        rc = get_a_sequence (pb);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failed to read a sequence");
        }
        else
        {
            if (pb->seq == NULL)
                break;
            rc = VCursorOpenRow (pb->cursor);
            if (rc)
            {
                LOGERR (klogFatal, rc, "Failed to open row");
                break;
            }
            else
            {
                rc_t rc2;
                rc = VCursorWrite (pb->cursor, pb->idx, 8, pb->seq, 0, pb->seq_size);
                if (rc)
                    LOGERR (klogFatal, rc, "Failed to write row");
                else
                    rc = VCursorCommitRow (pb->cursor);
                rc2 = VCursorCloseRow (pb->cursor);
                if (rc == 0)
                    rc = rc2;
            }
        }
    } while (rc == 0);

    return rc;
}


rc_t open (param_block * pb)
{
    rc_t rc;

    rc = KDirectoryNativeDir (&pb->pwd);
    if (rc)
        LOGERR (klogFatal, rc, "Failed to open file system");
    else
    {
        rc = KDirectoryOpenFileRead (pb->pwd, &pb->file, "%s", pb->file_path);
        if (rc)
            LOGERR (klogFatal, rc, "Failed to open input file");
        else
        {
            rc = KMMapMakeRead (&pb->mmap, pb->file);
            if (rc)
                LOGERR (klogFatal, rc, "unable to map file");
            else
            {
                rc = VDBManagerMakeUpdate (&pb->mgr, pb->pwd);
                if (rc)
                    LOGERR (klogFatal, rc, "Failed to open DB Manager");
                else
                {
                    rc = VDBManagerMakeSchema (pb->mgr, &pb->schema);
                    if (rc)
                        LOGERR (klogFatal, rc, "Failed to create a schema object");
                    else
                    {
                        VSchemaAddIncludePath (pb->schema, "interfaces");

                        rc = VSchemaParseFile (pb->schema, "%s", pb->schema_path);
                        if (rc)
                            LOGERR (klogFatal, rc, "Failed to parse schema");
                        else
                        {
                            rc = VDBManagerCreateTable (pb->mgr, &pb->table, pb->schema,
                                                        TYPESPEC, kcmCreate, "%s", pb->table_path);
                            if (rc)
                                LOGERR (klogFatal, rc, "Failed to create table");
                            else
                            {
                                rc = VTableCreateCursorWrite (pb->table, &pb->cursor, kcmCreate);
                                if (rc)
                                    LOGERR (klogFatal, rc, "Failed to create cursor");
                                else
                                {
                                    rc = VCursorAddColumn (pb->cursor, &pb->idx, "READ");
                                    if (rc)
                                        LOGERR (klogFatal, rc, "Failed to add READ to cursor");
                                    else
                                    {
                                        rc = VCursorOpen (pb->cursor);
                                        if (rc)
                                            LOGERR (klogFatal, rc, "Failed to open cursor");
                                        else
                                        {
                                            rc = write_rows (pb);
                                            if (rc == 0)
                                                VCursorCommit (pb->cursor);
                                        }
                                    }
                                    VCursorRelease (pb->cursor);
                                }
                                VTableRelease (pb->table);
                            }
                        }
                        VSchemaRelease (pb->schema);
                    }
                    VDBManagerRelease (pb->mgr);
                }
                KMMapRelease (pb->mmap);
            }
            KFileRelease (pb->file);
        }
        KDirectoryRelease (pb->pwd);
    }
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;
    rc_t rc = 0;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle (&args, argc, argv, 0 /*1, Options, sizeof Options / sizeof (&Options[1])*/);
    if (rc)
    {
        LOGERR (klogInt, rc, "Failed to parse parameters");
        return rc;
    }
    else
    {
        do
        {
            param_block pb;
            uint32_t pcount;

            rc = ArgsParamCount (args, &pcount);
            if (rc)
            {
                LOGERR (klogInt, rc, "Failed to get paramater count");
                break;
            }
            switch (pcount)
            {
            default:
                rc = MiniUsage (args);
                goto bailout;

            case 2:
                pb.schema_path = SCHEMASPEC;
                break;

            case 3:
                pb.schema_path = NULL;
                break;
            }

            rc = ArgsParamValue (args, 0, (const void **)&pb.file_path);
            if (rc)
            {
                LOGERR (klogInt,rc, "Failed to get file path");
                break;
            }

            rc = ArgsParamValue (args, 1, (const void **)&pb.table_path);
            if (rc)
            {
                LOGERR (klogInt,rc, "Failed to get table path");
                break;
            }
            if (pb.schema_path == NULL)
            {
                rc = ArgsParamValue (args, 2, (const void **)&pb.schema_path);
                if (rc)
                {
                    LOGERR (klogInt,rc, "Failed to get schema path");
                    break;
                }
            }
            rc = open (&pb);

        } while (0);
    bailout:
        ArgsWhack (args);
    }
    if (rc)
        LOGERR (klogFatal, rc, "Failed!");
    else
        KStsMsg("Exit success %R");

    return VDB_TERMINATE( rc );
}
