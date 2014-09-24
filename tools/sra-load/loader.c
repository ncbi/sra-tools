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
#include <kapp/main.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <kxml/xml.h>
#include <kfs/md5.h>
#include <kfs/arc.h>
#include <kfs/tar.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kdb/meta.h>
#include <kapp/loader-meta.h>
#include <sra/wsradb.h>

#include <os-native.h>

#include "debug.h"
#include "loader-fmt.h"
#include "run-xml.h"
#include "experiment-xml.h"
#include "loader-file.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>

static KDirectory* s_Directory = NULL;
static const KXMLMgr *s_XmlMgr = NULL;

enum TArgsIndex {
    targs_RunXML = 0,
    targs_InputPath,
    targs_InputUnpacked,
    targs_InputNoCache,
    targs_ExperimentXML,
    targs_Target,
    targs_ForceTarget,
    targs_SpotsNumber,
    targs_BadSpotsNumber,
    targs_BadSpotPercentage,
    targs_ExpectedXML,
    targs_Intensities,
    targs_ArgsQty
};

const char* usage_text[targs_ArgsQty][5] = {
	{ "path to run.xml describing input files", NULL },
	{ "input files location, default '.'", NULL },
	{ "input files are unpacked", NULL },
    { "disable input files threaded caching", NULL },
	{ "path to experiment.xml", NULL },
    { "target location", NULL },
	{ "force target overwrite", NULL },
	{ "process only given number of spots from input", NULL },
	{ "acceptable number of spot creation errors, default is 50", NULL },
	{ "acceptable percentage of spots creation errors, default is 5", NULL },
	{ "path to expected.xml", NULL },
	{ "[on | off] load intensity data, default is off. For ",
      "  Illumina: signal, intensity, noise;",
      "  AB SOLiD: signal(s);",
      "  LS454: signal, position (for SFF files this option is ON by default).", NULL },
};

OptDef TArgsDef[] =
{
    /* if you change order in this array, rearrange enum and usage_text above accordingly! */
    {"run-xml", "r", NULL, usage_text[targs_RunXML], 1, true, true},
    {"input-path", "i", NULL, usage_text[targs_InputPath], 1, true, false},
    {"input-unpacked", "u", NULL, usage_text[targs_InputUnpacked], 1, false, false},
    {"input-no-threads", "t", NULL, usage_text[targs_InputNoCache], 1, false, false},
    {"experiment", "e", NULL, usage_text[targs_ExperimentXML], 1, true, true},
    {"output-path", "o", NULL, usage_text[targs_Target], 1, true, true},
    {"force", "f", NULL, usage_text[targs_ForceTarget], 1, false, false},
    {"spots-number", "n", NULL, usage_text[targs_SpotsNumber], 1, true, false},
    {"bad-spot-number", "bE", NULL, usage_text[targs_BadSpotsNumber], 1, true, false},
    {"bad-spot-percentage", "p", NULL, usage_text[targs_BadSpotPercentage], 1, true, false},
    {"expected", "x", NULL, usage_text[targs_ExpectedXML], 1, true, false},
    {"intensities", "s", NULL, usage_text[targs_Intensities], 1, true, false}
};

rc_t CC UsageSummary (const char * progname)
{
    OUTMSG(( "\nUsage:\n\t%s [options] -r run.xml -e experiment.xml -o output-path\n\n", progname));
    return 0;
}

rc_t Usage( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    int i;

    if(args == NULL) {
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    } else {
        rc = ArgsProgram (args, &fullpath, &progname);
    }
    if(rc != 0 ) {
        progname = fullpath = UsageDefaultName;
    }
    UsageSummary(progname);

    for(i = 0; i < targs_ArgsQty; i++ ) {
        if( TArgsDef[i].required && TArgsDef[i].help[0] != NULL ) {
            HelpOptionLine(TArgsDef[i].aliases, TArgsDef[i].name, NULL, TArgsDef[i].help);
        }
    }
    OUTMSG(("\nOptions:\n"));
    for(i = 0; i < targs_ArgsQty; i++ ) {
        if( !TArgsDef[i].required && TArgsDef[i].help[0] != NULL ) {
            HelpOptionLine(TArgsDef[i].aliases, TArgsDef[i].name, NULL, TArgsDef[i].help);
        }
    }
    XMLLogger_Usage();
    OUTMSG(("\n"));
    HelpOptionsStandard();
    HelpVersion(fullpath, KAppVersion());
    return rc;
}

static
rc_t TArgsParse(TArgs *args, int argc, char *argv[])
{
    rc_t rc = 0;
    uint32_t count;
    char* end = NULL;
    const char* errmsg = NULL;
    const char* spot_qty = NULL, *bad_spots = NULL;
    const char *bad_percent = NULL, *intense = NULL;

    assert(args && argv);

    /* no parameters accepted */
    if( (rc = ArgsParamCount(args->args, &count)) != 0 || count != 0 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcUnexpected);
        ArgsParamValue(args->args, 0, &errmsg);

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_RunXML].name, &count)) != 0 || count != 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
        errmsg = TArgsDef[targs_RunXML].name;
    } else if( (rc = ArgsOptionValue(args->args, TArgsDef[targs_RunXML].name, 0, &args->_runXmlPath)) != 0 ) {
        errmsg = TArgsDef[targs_RunXML].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_ExperimentXML].name, &count)) != 0 || count != 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
        errmsg = TArgsDef[targs_ExperimentXML].name;
    } else if( (rc = ArgsOptionValue(args->args, TArgsDef[targs_ExperimentXML].name, 0, &args->_experimentXmlPath)) != 0 ) {
        errmsg = TArgsDef[targs_ExperimentXML].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_Target].name, &count)) != 0 || count != 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
        errmsg = TArgsDef[targs_Target].name;
    } else if( (rc = ArgsOptionValue(args->args, TArgsDef[targs_Target].name, 0, &args->_target)) != 0 ) {
        errmsg = TArgsDef[targs_Target].name;

    /* optional */
    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_InputPath].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_InputPath].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_InputPath].name, 0, &args->_input_path)) != 0 ) {
        errmsg = TArgsDef[targs_InputPath].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_InputUnpacked].name, &args->_input_unpacked)) != 0 ) {
        errmsg = TArgsDef[targs_InputUnpacked].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_InputNoCache].name, &args->_no_read_ahead)) != 0 ) {
        errmsg = TArgsDef[targs_InputNoCache].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_ForceTarget].name, &args->_force_target)) != 0 ) {
        errmsg = TArgsDef[targs_ForceTarget].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_SpotsNumber].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_SpotsNumber].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_SpotsNumber].name, 0, &spot_qty)) != 0 ) {
        errmsg = TArgsDef[targs_SpotsNumber].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_BadSpotsNumber].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_BadSpotsNumber].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_BadSpotsNumber].name, 0, &bad_spots)) != 0 ) {
        errmsg = TArgsDef[targs_BadSpotsNumber].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_BadSpotPercentage].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_BadSpotPercentage].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_BadSpotPercentage].name, 0, &bad_percent)) != 0 ) {
        errmsg = TArgsDef[targs_BadSpotPercentage].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_ExpectedXML].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_ExpectedXML].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_ExpectedXML].name, 0, &args->_expectedXmlPath)) != 0 ) {
        errmsg = TArgsDef[targs_ExpectedXML].name;

    } else if( (rc = ArgsOptionCount(args->args, TArgsDef[targs_Intensities].name, &count)) != 0 || count > 1 ) {
        rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
        errmsg = TArgsDef[targs_Intensities].name;
    } else if( count > 0 && (rc = ArgsOptionValue(args->args, TArgsDef[targs_Intensities].name, 0, &intense)) != 0 ) {
        errmsg = TArgsDef[targs_Intensities].name;
    }
    while( rc == 0 ) {
        long val = 0;
        if( args->_input_path == NULL ) {
            args->_input_path = "./";
            if( (rc = KDirectoryAddRef(s_Directory)) == 0 ) {
                args->_input_dir = s_Directory;
            } else {
                errmsg = TArgsDef[targs_InputPath].name;
                break;
            }
        } else if( (rc = KDirectoryOpenDirRead(s_Directory, &args->_input_dir, true, args->_input_path)) != 0 ) {
            errmsg = TArgsDef[targs_InputPath].name;
            break;
        }

        if( spot_qty != NULL ) {
            errno = 0;
            val = strtol(spot_qty, &end, 10);
            if( errno != 0 || spot_qty == end || *end != '\0' || val <= 0 ) {
                rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                errmsg = TArgsDef[targs_SpotsNumber].name;
                break;
            }
            args->_spots_to_run = val;
        }
        if( bad_spots != NULL ) {
            errno = 0;
            val = strtol(bad_spots, &end, 10);
            if( errno != 0 || bad_spots == end || *end != '\0' || val < 0 ) {
                rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                errmsg = TArgsDef[targs_BadSpotsNumber].name;
                break;
            }
            args->_spots_bad_allowed = val;
        }
        if( bad_percent != NULL ) {
            errno = 0;
            val = strtol(bad_percent, &end, 10);
            if( errno != 0 || bad_percent == end || *end != '\0' || val < 0 || val > 100 ) {
                rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                errmsg = TArgsDef[targs_BadSpotPercentage].name;
                break;
            }
            args->_spots_bad_allowed_percentage = val;
        }
        if( intense != NULL ) {
            if( strcasecmp(intense, "on") == 0 ) {
                args->_intensities = 1;
            } else if( strcasecmp(intense, "off") == 0 ) {
                args->_intensities = 2;
            } else {
                rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                errmsg = TArgsDef[targs_Intensities].name;
                break;
            }
        }
        break;
    }
    if( rc != 0 ) {
        Usage(args->args);
        LOGERR(klogErr, rc, errmsg ? errmsg : "check command line");
    }
    return rc;
}

static
void TArgsRelease(const TArgs* cself)
{
    if(cself) {
        TArgs* self = (TArgs*)cself;
        RunXML_Whack(self->_run);
        Experiment_Whack(self->_experiment);
        KDirectoryRelease(self->_input_dir);
        ArgsWhack(self->args);
        SRAMgrRelease(self->_sra_mgr);
        XMLLogger_Release(self->_xml_log);
        free(self);
    }
}

static
rc_t TArgsMake(TArgs **args, int argc, char *argv[])
{
    rc_t rc = 0;
    TArgs* r;

    assert(args && argv);

    if( (r = calloc(1, sizeof(*r))) == NULL) {
        rc = RC(rcExe, rcArgv, rcAllocating, rcMemory, rcExhausted);
    } else if( (rc = ArgsMakeAndHandle(&r->args, argc, argv, 2, TArgsDef, targs_ArgsQty, XMLLogger_Args, XMLLogger_ArgsQty)) == 0 &&
               (rc = XMLLogger_Make(&r->_xml_log, s_Directory, r->args)) == 0 ) {
        r->_force_target = false;
        r->_spots_to_run = -1;
        r->_spots_bad_allowed = 50;
        r->_spots_bad_allowed_percentage = 5;
        r->_input_unpacked = false;
        if( argc > 1 ) {
            rc = TArgsParse(r, argc, argv);
        } else {
            MiniUsage(r->args);
            exit(0);
        }
    }
    if( rc == 0 ) {
        *args = r;
    } else {
        TArgsRelease(r);
        *args = NULL;
    }
    return rc;
}

static rc_t read_required_node_data_cstr(const KXMLNode *node,
    const char *attr, char **string)
{
    rc_t rc = attr == NULL ?
            KXMLNodeReadCStr(node, string, NULL) :
            KXMLNodeReadAttrCStr(node, attr, string, NULL);

    if (rc && GetRCState(rc) == rcNotFound) {
        const char *name = NULL;
        if( (rc = KXMLNodeElementName(node, &name)) == 0 ) {
            if (attr == NULL) {
                PLOGERR(klogErr, (klogErr, rc, "XML node $(name)", "name=%s", name));
            }
            else {
                PLOGERR(klogErr, (klogErr, rc, "XML attribute $(name)/@$(attr)",
                                  "name=%s,attr=%s", name, attr));
            }
        }
    }
    return rc;
}

static rc_t read_optional_node_data_cstr(const KXMLNode *node,
    const char *attr, char **string)
{
    rc_t rc = attr == NULL ?
            KXMLNodeReadCStr(node, string, NULL) :
            KXMLNodeReadAttrCStr(node, attr, string, "");

    if (rc) {
        if (GetRCState(rc) == rcNotFound) {
            rc = 0;
        }
        else {
            const char *name = NULL;
            if( (rc = KXMLNodeElementName(node, &name)) == 0 ) {
                PLOGERR(klogErr, (klogErr, rc, "XML attribute $(name)/@$(attr)",
                                  "name=%s,attr=%s", name, attr));
            }
        }
    }
    return rc;
}

static rc_t ResultXML_make(ResultXML **rslt, const KXMLNode *Result)
{
    rc_t rc = 0;
    ResultXML *r = NULL;

    assert(rslt && Result);

    r = calloc(1, sizeof *r);
    if (r == NULL) {
        rc = RC(rcExe, rcXmlDoc, rcAllocating, rcMemory, rcExhausted);
    }

    if (rc == 0) {
        rc = read_required_node_data_cstr(Result, "file", &r->file);
    }

    if (rc == 0) {
        rc = read_required_node_data_cstr(Result, "name", &r->name);
    }

    if (rc == 0) {
        rc = read_required_node_data_cstr(Result, "sector", &r->sector);
    }

    if (rc == 0) {
        rc = read_required_node_data_cstr(Result, "region", &r->region);
    }

    if (rc == 0) {
        rc = read_optional_node_data_cstr(Result, "member_name", &r->member_name);
    }

    if (rc) {
        free(r);
    }
    else {
        *rslt = r;
    }

    return rc;
}

static rc_t ExpectedXMLWhack(const ExpectedXML *r)
{
    return 0;
}

static rc_t ExpectedXMLMake(const ExpectedXML **rslt,
    const KXMLNode *Expected)
{
    rc_t rc = 0;
    const KXMLNodeset *Results = NULL;
    if (rslt == NULL) {
        return RC(rcExe, rcXmlDoc, rcConstructing, rcParam, rcNull);
    }
    *rslt = NULL;
    if (Expected == NULL) {
        return RC(rcExe, rcXmlDoc, rcConstructing, rcParam, rcNull);
    }
    rc = KXMLNodeOpenNodesetRead(Expected, &Results, "Result");
    if (rc == 0) {
        ExpectedXML *r = calloc(1, sizeof *r);
        if (r == NULL) {
            rc = RC(rcExe, rcXmlDoc, rcAllocating, rcMemory, rcExhausted);
        }
        else {
            uint32_t num = 0;
            rc = KXMLNodesetCount(Results, &num);
            if (rc == 0 && num > 0) {
                r->number_of_results = num;
                r->result = calloc(1, num * sizeof r->result[0]);
                if (r->result == NULL) {
                    rc = RC(rcExe, rcXmlDoc, rcAllocating, rcMemory, rcExhausted);
                    free(r);
                }
                else {
                    int i = 0;
                    for (i = 0; rc == 0 && i < num; ++i) {
                        const KXMLNode *Result = NULL;
                        rc = KXMLNodesetGetNodeRead(Results, &Result, i);
                        if (rc == 0) {
                            rc = ResultXML_make(&r->result[i], Result);
                        }
                    }
                    if (rc == 0) {
                        *rslt = r;
                    }
                    else {
                        ExpectedXMLWhack(r);
                    }
                }
            }
        }
    }
    else if (GetRCState(rc) != rcNotFound) {
        LOGERR(klogErr, rc, "Cannot read Result node");
    }
    return rc;
}

static rc_t s_KXMLDocMake(const KXMLDoc **doc, const char *path)
{
    rc_t rc = 0;
    const KFile *file = NULL;
    if (doc == NULL || path == NULL) {
        return RC(rcExe, rcXmlDoc, rcConstructing, rcParam, rcNull);
    }
    if (rc == 0) {
        rc = KDirectoryOpenFileRead(s_Directory, &file, "%s", path);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc, "Cannot open file $(file)", "file=%s", path));
        }
    }
    if (rc == 0) {
        rc = KXMLMgrMakeDocRead(s_XmlMgr, doc, file);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc, "Cannot make XML document from $(file)",
                              "file=%s", path));
        }
    }
    KFileRelease(file);
    return rc;
}

static rc_t KXMLNodeMakeFromDoc(const KXMLNode **node, const KXMLDoc *doc,
    const char *root)
{
    rc_t rc = 0;
    const KXMLNodeset *ns = NULL;
    if (node == NULL || doc == NULL || root == NULL) {
        return RC(rcExe, rcXmlDoc, rcConstructing, rcParam, rcNull);
    }
    if (rc == 0) {
        rc = KXMLDocOpenNodesetRead(doc, &ns, root);
    }
    if (rc == 0) {
        uint32_t count = 0;
        rc = KXMLNodesetCount(ns, &count);
        if (rc == 0) {
            if (count == 0) {
                rc = RC(rcExe, rcXmlDoc, rcReading, rcNode, rcNotFound);
                PLOGERR(klogErr, (klogErr, rc, "$(path)", "path=%s", root));
            }
            else if (count > 1) {
                rc = RC(rcExe, rcXmlDoc, rcParsing, rcData, rcExcessive);
                PLOGERR(klogErr, (klogErr, rc,
                                  "too many data $(node) nodes", "node=%s", root));
            }
        }
    }
    if (rc == 0) {
        rc = KXMLNodesetGetNodeRead(ns, node, 0);
    }
    KXMLNodesetRelease(ns);
    return rc;
}

static rc_t KXMLNodeMakeFromFile(const KXMLNode **node, const char *path,
    const char *root)
{
    rc_t rc = 0;
    const KXMLDoc *doc = NULL;
    const char *rootPath = "/";
    if (node == NULL || path == NULL) {
        return RC(rcExe, rcXmlDoc, rcConstructing, rcParam, rcNull);
    }
    rc = s_KXMLDocMake(&doc, path);
    if (rc == 0) {
        if (root != NULL) {
            rootPath = root;
        }
        rc = KXMLNodeMakeFromDoc(node, doc, rootPath);
    }
    KXMLDocRelease(doc);
    return rc;
}

static
rc_t TArgsParseRunXml(TArgs *args)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;
    const KXMLDoc *doc = NULL;
    assert(args);
    if (args->_runXmlPath == NULL) {
        return rc;
    }
    if (rc == 0) {
        rc = s_KXMLDocMake(&doc, args->_runXmlPath);
    }
    if (rc == 0) {
        rc= KXMLNodeMakeFromDoc(&node, doc, "/RUN_SET/RUN | /RUN");
    }
    if (rc == 0) {
        rc = RunXML_Make(&args->_run, node);
    }
    KXMLNodeRelease(node);
    KXMLDocRelease(doc);
    return rc;
}

static rc_t TArgsParseExpectedXml(TArgs *self)
{
    rc_t rc = 0;
    const KXMLNode *node = NULL;
    assert(self);
    if (self->_expectedXmlPath == NULL) {
        return rc;
    }
    if (rc == 0) {
        rc = KXMLNodeMakeFromFile(&node, self->_expectedXmlPath, "//Expected");
    }
    if (rc == 0) {
        rc = ExpectedXMLMake(&self->_expected, node);
    }
    KXMLNodeRelease(node);
    return rc;
}

static rc_t TArgsParseXmls(TArgs *self)
{
    rc_t rc = 0;
    const KXMLDoc *doc = NULL;

    if( (rc = TArgsParseRunXml(self)) == 0 &&
        (rc = s_KXMLDocMake(&doc, self->_experimentXmlPath)) == 0 &&
        (rc = Experiment_Make(&self->_experiment, doc, &self->_run->attributes)) == 0 ) {
        rc = TArgsParseExpectedXml(self);
    }
    KXMLDocRelease(doc);
    return rc;
}

static rc_t TArgsLogResultTotal(const TArgs *self, rc_t status, uint64_t spotCount, uint64_t baseCount, int64_t bad_spot_count)
{
    rc_t rc = 0;
    int i = 0;
    const char* accession, *state;
    KLogLevel tmp_lvl;

    if (self == NULL) {
        return rc;
    }

    accession = strrchr(self->_target, '/');
    if( accession == NULL ) {
        accession = self->_target;
    } else {
        accession++;
    }
    state = status == 0 ? "success" : "failure";
    /* raise log level in order to print stats */
    tmp_lvl = KLogLevelGet();
    KLogLevelSet(klogInfo);
    for(i = 0; self->_expected != NULL && i < self->_expected->number_of_results && rc == 0; i++) {
        ResultXML *f = self->_expected->result[i];
        PLOGMSG(klogInfo, (klogInfo, "loaded",
                "severity=result,status=%s,accession=%s,spot_count=%lu,base_count=%lu,"
                "file=%s,name=%s,region=%s,sector=%s,member_name=%s",
                state, accession, spotCount, baseCount,
                f->file, f->name, f->region, f->sector, f->member_name ? f->member_name : ""));
    }
    if(status == 0) {
        PLOGMSG(klogInfo, (klogInfo, "loaded",
                "severity=total,status=%s,accession=%s,spot_count=%lu,base_count=%lu,bad_spots=%ld",
                state, accession, spotCount, baseCount, bad_spot_count));
    } else {
        PLOGERR(klogErr, (klogErr, status, "load failed: $(reason_short)",
                "severity=total,status=%s,accession=%s",
                state, accession));
    }
    KLogLevelSet(tmp_lvl);
    return rc;
}

static
void SInput_Release(const SInput* cself)
{
    if(cself) {
        SInput* self = (SInput*)cself;
        uint32_t i;
        for(i = 0; i < self->count; i++) {
            /* actual files must be released elsewhere */
            free(self->blocks[i].files);
        }
        free(self->blocks);
        free(self);
    }
}

typedef struct SInputOpen_TarVisitData_struct {
    uint32_t alloc;
    char** files;
    uint32_t count;
    
} SInputOpen_TarVisitData;

static
rc_t SInputOpen_TarVisit(const KDirectory *dir, uint32_t type, const char *name, void *data )
{
    rc_t rc = 0;

    if( type == kptFile ) {
        SInputOpen_TarVisitData* d = (SInputOpen_TarVisitData*)data;

        if( d->alloc <= d->count ) {
            void* p;
            d->alloc = d->files == NULL ? 1024 : (d->alloc * 2);
            if( (p = realloc(d->files, d->alloc * sizeof(*d->files))) == NULL ) {
                rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            } else {
                d->files = p;
            }
        }
        if( rc == 0 ) {
            char buf[1024];
            if( (rc = KDirectoryResolvePath(dir, true, buf, sizeof(buf), "%s", name)) == 0 ) { 
                d->files[d->count++] = strdup(buf);
                if( d->files[d->count - 1] == NULL ) {
                    rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                }
            }
        }
    }
    return rc;
}

static
rc_t SInputOpen(const SInput **cself, TArgs *args)
{
    rc_t rc = 0;
    SInput *self = calloc(1, sizeof(*self));
    uint32_t file_qty = 0;

    if(self == NULL) {
        rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    } else {
        self->blocks = calloc(args->_run->datablock_count, sizeof(*(self->blocks)));
        if(self->blocks == NULL) {
            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        } else {
            uint32_t i, ccdirs_qty = 0;
            struct {
                const char* uid;
                const KDirectory* xdir;
            } ccdirs[1024];

            memset(ccdirs, 0, sizeof(ccdirs));
            self->count = args->_run->datablock_count;
            for(i = 0; rc == 0 && i < self->count; i++) {
                const DataBlock* b = &args->_run->datablocks[i];
                uint32_t f, j;
                char fname[10240];

                self->blocks[i].files = calloc(b->files_count, sizeof(*(self->blocks->files)));
                if(self->blocks == NULL) {
                    rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                    continue;
                }
                self->blocks[i].count = b->files_count;
                for(f = 0, j = 0; rc == 0 && j < b->files_count; j++ ) {
                    int fnlen = strlen(b->files[j].filename);
                    if( b->files[j].cc_xml != NULL ) {
                        const KDirectory* fdir = args->_input_dir;
                        if( !args->_input_unpacked ) {
                            uint32_t cd;
                            for(cd = 0; cd < ccdirs_qty; cd++) {
                                if( strcmp(ccdirs[cd].uid,  b->files[j].cc_xml) == 0 ) {
                                    break;
                                }
                            }
                            if( cd >= ccdirs_qty ) {
                                const KFile* kf = NULL;
                                if( ccdirs_qty + 1 > sizeof(ccdirs) / sizeof(ccdirs[0]) ) {
                                    rc = RC(rcExe, rcStorage, rcAllocating, rcNamelist, rcTooLong);
                                } else if( (rc = KDirectoryOpenFileRead(args->_input_dir, &kf, "%s.xml", b->files[j].cc_xml)) == 0) {
                                    char base_path[4096];
                                    int r = snprintf(base_path, sizeof base_path, "%s/", b->files[j].cc_xml);
                                    if (r >= sizeof base_path) {
                                        rc = RC(rcExe, rcStorage, rcAllocating, rcFile, rcTooLong);
                                    } else if ((rc = KDirectoryOpenXTocDirRead(args->_input_dir, &ccdirs[ccdirs_qty].xdir, false, kf, "%s", base_path)) == 0 ) {
                                        cd = ccdirs_qty++;
                                        ccdirs[cd].uid = b->files[j].cc_xml;
                                        KFileRelease(kf);
                                    }
                                }
                            }
                            fdir = ccdirs[cd].xdir;
                        }
                        if( rc == 0 ) {
                            char* fn = strdup(b->files[j].cc_path);
                            if( fn == NULL ) {
                                rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                            } else {
                                rc = SRALoaderFile_Make(&self->blocks[i].files[f++], fdir, fn, b, &b->files[j].attr,
                                                        NULL, !args->_no_read_ahead);
                                file_qty++;
                                free(fn);
                            }
                        }
                        continue;
                    }
                    if( fnlen >= sizeof(fname) ) {
                        rc = RC(rcExe, rcStorage, rcAllocating, rcFile, rcTooLong);
                        PLOGERR(klogErr, (klogErr, rc, "$(file)", PLOG_S(file), b->files[j].filename));
                        continue;
                    }
                    strcpy(fname, b->files[j].filename);
                    /* compressed archives supposed to be decompressed beforehand */
                    if( fnlen > 7 && strcmp(&fname[fnlen - 7], ".tar.gz") == 0 ) {
                        fnlen -= 3;
                        fname[fnlen] = '\0';
                    } else if( fnlen > 4 && strcmp(&fname[fnlen - 4], ".tgz") == 0 ) {
                        fname[fnlen - 2] = 'a';
                        fname[fnlen - 1] = 'r';
                    } else if( fnlen > 7 && strcmp(&fname[fnlen - 7], ".tar.bz2") == 0 ) {
                        fnlen -= 4;
                        fname[fnlen] = '\0';
                    }
                    if( strcmp(&fname[fnlen - 4], ".tar") == 0 ) {
                        const KDirectory* tar;
                        if( (rc = KDirectoryOpenArcDirRead(args->_input_dir, &tar, true, fname,
                                                           tocKFile, KArcParseTAR, NULL, NULL)) == 0 ) {
                            uint32_t k;
                            void* p;
                            SInputOpen_TarVisitData data = {0, NULL, 0};

                            if( (rc = KDirectoryVisit(tar, true, SInputOpen_TarVisit, &data, ".")) != 0 ) {
                                continue;
                            }
                            if( data.count == 0 ) {
                                rc = RC(rcExe, rcStorage, rcAllocating, rcFile, rcEmpty);
                                PLOGERR(klogErr,(klogErr, rc, "Empty tar $(file)", PLOG_S(file), fname));
                                continue;
                            }
                            self->blocks[i].count += data.count - 1; /* minus tar file */
                            p = realloc(self->blocks[i].files, self->blocks[i].count * sizeof(*(self->blocks->files)));
                            if( p == NULL) {
                                rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                                continue;
                            }
                            self->blocks[i].files = p;
                            for(k = 0; k < data.count; k++) {
                                rc = SRALoaderFile_Make(&self->blocks[i].files[f++], tar, data.files[k], b, &b->files[j].attr,
                                                        NULL, !args->_no_read_ahead);
                                file_qty++;
                            }
                            free(data.files);
                            KDirectoryRelease(tar);
                        }
                    } else {
                        char* fn = strdup(fname);
                        if( fn == NULL ) {
                            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                        } else {
                            rc = SRALoaderFile_Make(&self->blocks[i].files[f++], args->_input_dir, fn, b, &b->files[j].attr,
                                                    b->files[j].md5_digest, !args->_no_read_ahead);
                            file_qty++;
                            free(fn);
                        }
                    }
                }
            }
            for(i = 0; i < ccdirs_qty; i++) {
                KDirectoryRelease(ccdirs[i].xdir);
            }
        }
    }
    if( rc == 0 && file_qty < 1 ) {
        rc = RC(rcExe, rcStorage, rcAllocating, rcData, rcEmpty);
        LOGERR(klogErr, rc, "no files to load");
    }
    if( rc == 0 ) {
        *cself = self;
    } else {
        SInput_Release(self);
    }
    return rc;
}

static
rc_t WriteSoftwareVersionsMeta(SRATable* table, const char *argv0, uint32_t fe_version, const char* fe_name)
{
    rc_t rc = 0;
    KMDataNode *node = NULL;

    assert(table);

    if( (rc = SRATableOpenMDataNodeUpdate(table, &node, "/")) == 0 ) {
        rc = KLoaderMeta_Write(node, argv0, __DATE__, fe_name, fe_version);
    }
    if( rc != 0 ) {
        LOGERR(klogErr, rc, "Cannot update loader meta");
    }
    KMDataNodeRelease(node);
    return rc;
}

rc_t KMain(int argc, char *argv[])
{
    rc_t rc = 0, lastRc = 0;

    TArgs *args = NULL;
    const SInput *input = NULL;
    SRATable* table = NULL;
    SRALoaderFmt* fe = NULL;

    SRALoaderConfig feInput;
    memset(&feInput, 0, sizeof feInput);

    /* Initialize statics */
    if (rc == 0) {
        rc = KDirectoryNativeDir(&s_Directory);
    }
    /* Parse command line arguments */
    if (rc == 0) {
        rc = TArgsMake(&args, argc, argv);
    }
    if( rc == 0 &&
        (rc = SRAMgrMakeUpdate(&args->_sra_mgr, NULL)) == 0 &&
        (rc = SRAMgrSetMD5Mode(args->_sra_mgr, true)) == 0 ) {
        rc = KXMLMgrMakeRead(&s_XmlMgr);
    }
    if( rc == 0 && (rc = TArgsParseXmls(args)) == 0) {
        if( args->_intensities == 2 ) {
            feInput.columnFilter = efltrINTENSITY | efltrNOISE | efltrSIGNAL;
        } else if( args->_intensities == 1 ) {
            feInput.columnFilter = 0;
        } else {
            feInput.columnFilter = efltrDEFAULT;
        }
        feInput.spots_to_run = args->_spots_to_run;
        feInput.spots_bad_allowed = args->_spots_bad_allowed;
        feInput.experiment = args->_experiment;
        feInput.sra_mgr = args->_sra_mgr;
        feInput.table_path = args->_target;
        feInput.force_table_overwrite = args->_force_target;

        rc = SRALoaderFmtMake(&fe, &feInput);
    }
    if (rc == 0) {
        rc = SInputOpen(&input, args);
    }
    if(rc == 0) {
        if( fe->vt->v1.exec_prep ) {
            const char* efile = NULL;
            const char* eargs[64];
            uint32_t i = 1, j, qty = 0, x = 0;
            char extra[1024];

            eargs[i++] = "-L";
            eargs[i++] = KLogGetParamStrings()[KLogLevelGet()];
            qty = KStsLevelGet();
            while(qty-- > 0 ) {
                eargs[i++] = "-v";
            }
#if _DEBUGGING
            if( ArgsOptionCount(args->args, "+", &qty) == 0 ) {
                uint32_t j;
                for(j = 0; j < qty; j++) {
                    const char* v;
                    if( ArgsOptionValue(args->args, "+", j, &v) == 0 ) {
                        eargs[i++] = "-+";
                        eargs[i++] = v;
                    }
                }
            }
            if( ArgsOptionCount(args->args, "debug", &qty) == 0) {
                uint32_t j;
                for(j = 0; j < qty; j++) {
                    const char* v;
                    if( ArgsOptionValue(args->args, "debug", j, &v) == 0 ) {
                        eargs[i++] = "-+";
                        eargs[i++] = v;
                    }
                }
            }
#endif
            for(j = 0; j < XMLLogger_ArgsQty; j++) {
                const char* v;
                if( XMLLogger_Args[j].aliases != NULL &&
                    ArgsOptionCount(args->args, XMLLogger_Args[j].aliases, &qty) == 0 &&
                    qty > 0 &&
                    ArgsOptionValue(args->args, XMLLogger_Args[j].aliases, 0, &v) == 0 ) {
                        eargs[i++] = &extra[x];
                        extra[x++] = '-';
                        extra[x++] = '-';
                        strcpy(&extra[x], XMLLogger_Args[j].name);
                        x += strlen(XMLLogger_Args[j].name);
                        eargs[i++] = v;
                } else if( XMLLogger_Args[j].name != NULL &&
                           ArgsOptionCount(args->args, XMLLogger_Args[j].name, &qty) == 0 &&
                           qty > 0 &&
                           ArgsOptionValue(args->args, XMLLogger_Args[j].name, 0, &v) == 0 ) {
                        eargs[i++] = &extra[x];
                        extra[x++] = '-';
                        extra[x++] = '-';
                        strcpy(&extra[x], XMLLogger_Args[j].name);
                        x += strlen(XMLLogger_Args[j].name);
                        eargs[i++] = v;
                }

            }
            eargs[i] = NULL;

            if( (rc = SRALoaderFmtExecPrep(fe, args, input, &efile, eargs + i, sizeof(eargs) / sizeof(eargs[0]) - i)) == 0 ) {
                eargs[0] = efile;
#if _DEBUGGING
                DEBUG_MSG(3, ("execv: '%s", efile));
                for(i = 0; eargs[i] != NULL; i++) {
                    DEBUG_MSG(3, (" %s", eargs[i]));
                }
                DEBUG_MSG(3, ("'\n"));
#endif
                execv(efile, (char* const*)eargs);
                rc = RC(rcExe, rcFormatter, rcExecuting, rcInterface, rcFailed);
                PLOGERR(klogErr, (klogErr, rc, "front-end exec: $(errno)", "errno=%s", strerror(errno)));
            }
        } else {
            rc_t rc2 = 0;
            uint64_t spotCount = 0;
            uint64_t numBases = 0;
            int64_t bad_spot_count = 0;
            uint32_t fe_vers = 0;
            const char* fe_name;

            rc = SRALoaderFmtVersion(fe, &fe_vers, &fe_name);
            if(rc == 0) {
                uint32_t i, j;
                for(i = 0; rc == 0 && i < input->count; i++) {
                    rc = SRALoaderFmtWriteData(fe, input->blocks[i].count, input->blocks[i].files, &bad_spot_count);
                    for(j = 0; j < input->blocks[i].count; j++) {
                        bool eof = true;
                        if( rc == 0 && SRALoaderFile_IsEof(input->blocks[i].files[j], &eof) == 0 ) {
                            if( args->_spots_to_run < 0 && eof != true ) {
                                const char* fn;
                                SRALoaderFileName(input->blocks[i].files[j], &fn);
                                rc2 = RC(rcExe, rcFormatter, rcReading, rcFile, rcExcessive);
                                PLOGERR(klogErr, (klogErr, rc2, "File $(file) not completely parsed", PLOG_S(file), fn));
                            }
                        }
                        SRALoaderFile_Release(input->blocks[i].files[j]);
                    }
                    if( GetRCObject(rc) == rcTransfer && GetRCState(rc) == rcDone ) {
                        /* interrupted by requested spot count */
                        rc = 0;
                        break;
                    }
                    rc = rc ? rc : rc2;
                }
            }
            /* release object to make sure it free's it's data and commits */
            rc2 = SRALoaderFmtRelease(fe, &table);
            rc = rc ? rc : rc2;
            fe = NULL;

            if( rc == 0 && table != NULL ) {
                /* Write versions */
                if (rc == 0) {
                    rc = WriteSoftwareVersionsMeta(table, argv[0], fe_vers, fe_name);
                }
                if (rc == 0) {
                    rc = SRATableCommit(table);
                    if( GetRCObject(rc) == (enum RCObject)rcCursor && GetRCState(rc) == rcNotOpen ) {
                        rc = 0;
                    }
                }
                if (rc == 0) {
                    rc = SRATableSpotCount(table, &spotCount);
                    if (rc == 0) {
                        rc = SRATableBaseCount(table, &numBases);
                    }
                    if(rc == 0) {
                        if(spotCount > 0) {
                            double percentage = ((double)bad_spot_count) / spotCount;
                            double allowed = args->_spots_bad_allowed_percentage / 100.0;
                            if(percentage > allowed ||
                               (feInput.spots_bad_allowed >= 0 && bad_spot_count > feInput.spots_bad_allowed)) {
                                rc = RC(rcExe, rcTable, rcClosing, rcData, rcInvalid);
                                PLOGERR(klogErr, 
                                        (klogErr, rc,
                                         "Too many bad spots: "
                                         "spots: $(spots), bad spots: $(bad_spots), "
                                         "bad spots percentage: $(percentage), "
                                         "allowed percentage: $(allowed), "
                                         "allowed count: $(bad_allowed)",
                                         "spots=%d,bad_spots=%li,percentage=%.2f,allowed=%.2f,bad_allowed=%li",
                                         spotCount, bad_spot_count,
                                         percentage, allowed, feInput.spots_bad_allowed));
                            }
                        }
                    }
                }
                if( rc == 0) {
                    rc = SRATableRelease(table);
                    table = NULL;
                }
            }
            if(rc == 0) {
                if(spotCount == 0) {
                    rc = RC(rcExe, rcFormatter, rcClosing, rcTable, rcEmpty);
                    LOGERR(klogErr, rc, "0 spots were loaded after successful front-end run");
                }
            }
            rc2 = TArgsLogResultTotal(args, rc, spotCount, numBases, bad_spot_count);
            rc = rc ? rc : rc2;
        }

        if( rc != 0 ) {
            if( table ) {
                rc_t status = SRATableRelease(table);
                table = NULL;
                if(status == 0) {
                    /* should go away as soon as cursor transactions are in place */
                    status = SRAMgrDropTable(args->_sra_mgr, true, args->_target);
                }
                if(status != 0) {
                    lastRc = KLogLastErrorCode();
                    LOGERR(klogInt, status, "cannot DropRun after load failure");
                }
            }
        }
    }

    /* make sure the error was reported */
    if( rc != 0 &&
        rc != SILENT_RC(rcExe, rcProcess, rcExecuting, rcProcess, rcCanceled) &&
        rc != SILENT_RC(rcExe, rcProcess, rcExecuting, rcTransfer, rcDone)) {
        if (lastRc == 0) {
            lastRc = KLogLastErrorCode();
        }
        if (rc != lastRc) {
            LOGERR(klogInt, rc, "no message text available");
        }
    }
    SInput_Release(input);
    SRALoaderFmtRelease(fe, NULL);
    TArgsRelease(args);
    KDirectoryRelease(s_Directory);
    KXMLMgrRelease(s_XmlMgr);

    return rc;
}
