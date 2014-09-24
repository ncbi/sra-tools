/*******************************************************************************
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
 * =============================================================================
 */

#include "meta-sync.vers.h" /* META_SYNC_VERS */
#include <kapp/main.h>
#include <sra/srapath.h>
#include <sra/sraschema.h> /* VDBManagerMakeSRASchema */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/database.h> /* VDatabase */
#include <vdb/table.h> /* VTable */
#include <vdb/schema.h> /* VSchemaRelease */
#include <vdb/cursor.h> /* VCursor */
#include <rdbms/sybase.h> /* SybaseInit */
#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */
#include <kxml/xml.h> /* KXMLMgr */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h> /* RC */
#include <insdc/sra.h> /* SRA_READ_TYPE_... */
#include <assert.h>
#include <stdio.h> /* vspnrintf */
#include <stdlib.h> /* malloc */
#include <string.h> /* memset */
#include <os-native.h> /* strncasecmp on Windows */

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt,rc, "$(msg): $(name)","msg=%s,name=%s",msg,name)))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

enum ECol {
      eFIXED_SPOT_LEN
    , eLABEL
    , eLABEL_LEN
    , eLABEL_START
    , eLINKER_SEQUENCE
/*  , eNREADS */
    , ePLATFORM
    , eREAD_LEN
    , eREAD_START
    , eREAD_TYPE
    , eSPOT_GROUP
    , eSPOT_LEN
};
/*enum EType {
      eUndetected
    , e454_2
    , e454_2_AA
    , e454_4_FollowingLinker
    , e454_4_ForwardForward
    , e454_4_RelativeOrder
    , e454_4_FFLF
    , eIllumina_1
};*/
enum EColType {
      eAscii
    , eReadType
    , eUint8
    , eUint32
};
typedef struct Col {
    enum ECol type;
    const char* name;
    enum EColType datatype;
    bool isArray;
    uint32_t idx;
    char buffer[70];
    uint32_t row_len;
    bool exists;
} Col;
enum ELabel {
      eUnsetL
    , eInvalidF7L
    , eAdapterL
    , eBarcodeL
    , eFL
    , eF3L
    , eForwardL
    , eFragmentL
    , eLinkerL
    , eMate1L
    , eMate2L
    , eR3L
    , eReverseL
    , erRNA_primerL
};
enum EReadType {
      eUnknownRT
    , eAdapterRT
    , eBarCodeRT
    , eForwardRT
    , eLinkerRT
    , ePrimerRT
    , eReverseRT
};
#define unsetReadClass -1
#define MAX_NREADS 5
typedef struct SReadXml {
    bool used;

    /* SRA_READ_TYPE_TECHNICAL SRA_READ_TYPE_BIOLOGICAL;
    unsetReadClass is a special value */
    int readClass;

    enum EReadType readType;
} SReadXml;
typedef struct MetaDataXml {
    INSDC_SRA_platform_id platform;
    uint32_t nreads;
    SReadXml read[MAX_NREADS];
    bool variableReadLen;
} MetaDataXml;
typedef struct SReadDb {
    bool used;
                                    /* READ_TYPE */
    /* SRA_READ_TYPE_TECHNICAL SRA_READ_TYPE_BIOLOGICAL;
    unsetReadClass is a special value */
    int readClass; 
    int readOrientation; /* SRA_READ_TYPE_FORWARD SRA_READ_TYPE_REVERSE */
    enum ELabel label;              /* LABEL, LABEL_START, LABEL_LEN */
} SReadDb;
typedef struct MetaDataDb {
    INSDC_SRA_platform_id platform; /* PLATFORM */
    uint32_t nreads;                 /* number of READ_START elements */
    SReadDb read[MAX_NREADS];
} MetaDataDb;
typedef struct CmdLine {
    bool notFound;
    const char* tbl;
	const char* exp;
	const char* run;
    const char* acc;
} CmdLine;
/* true if differ */
static bool Compare(const char* sample, const char* test, size_t test_len)
{
    if (!sample || !test || !test_len)
    {   return true; }
    if (strlen(sample) != test_len)
    {   return true; }
    return strncasecmp(test, sample, test_len);
}
typedef struct LabelStrEnum {
    enum ELabel e;
    const char* s;
} LabelStrEnum;
typedef struct PlatfromStrEnum {
    INSDC_SRA_platform_id e;
    const char* s;
} PlatfromStrEnum;
typedef struct ReadTypeStrEnum {
    enum EReadType e;
    const char* s;
} ReadTypeStrEnum;
#define TYPE_STR_CONVERT(name, elm, table) \
    static const char* Enum##name##2Str(elm e) { \
        int i = 0; \
        for (i = 0; i < sizeof table / sizeof table[0]; ++i) { \
            if (table[i].e == e) { return table[i].s; } \
        } \
        assert(0); \
        return NULL; \
    } \
    static elm Str##name##2Enum(const char* s) { \
        int i = 0; \
        assert(s); \
        for (i = 0; i < sizeof table / sizeof table[0]; ++i) { \
            assert(table[i].s); \
            if (strcmp(table[i].s, s) == 0) { return table[i].e; } \
        } \
        assert(0); \
        return 0; \
    } \
    static elm Strn##name##2Enum(const char* s, size_t l) { \
        int i = 0; \
        assert(s && l); \
        for (i = 0; i < sizeof table / sizeof table[0]; ++i) { \
            assert(table[i].s); \
            if (!Compare(table[i].s, s, l)) { return table[i].e; } \
        } \
     /* printf("Strn" #name "2Enum(%s) failed\n", s); \
        assert(0); */ \
        return 0; \
    }
#define ENUM_STR_CONVERT(name, elm, table) TYPE_STR_CONVERT(name,enum elm,table)
static PlatfromStrEnum platforms[] = {
      { SRA_PLATFORM_454              , "LS454"             }
    , { SRA_PLATFORM_ABSOLID          , "ABI_SOLID"         }
    , { SRA_PLATFORM_COMPLETE_GENOMICS, "COMPLETE_GENOMICS" }
    , { SRA_PLATFORM_HELICOS          , "HELICOS"           }
    , { SRA_PLATFORM_ILLUMINA         , "ILLUMINA"          }
    , { SRA_PLATFORM_PACBIO_SMRT      , "PACBIO_SMRT"       }
};
TYPE_STR_CONVERT(Platform, INSDC_SRA_platform_id, platforms)
static ReadTypeStrEnum readTypes[] = {
      { eAdapterRT, "Adapter" }
    , { eBarCodeRT, "Barcode" }
    , { eForwardRT, "Forward" }
    , { eLinkerRT , "Linker"  }
    , { ePrimerRT , "Primer"  }
    , { eReverseRT, "Reverse" }
};
ENUM_STR_CONVERT(ReadType, EReadType, readTypes)
static LabelStrEnum labels[] = {
      { eAdapterL,     "Adapter"     }
    , { eBarcodeL    , "Barcode"     }
    , { eFL          , "F"           }
    , { eF3L         , "F3"          }
    , { eForwardL    , "Fragment"    }
    , { eFragmentL   , "Forward"     }
    , { eLinkerL     , "Linker"      }
    , { eMate1L      , "Mate1"       }
    , { eMate2L      , "Mate2"       }
    , { eR3L         , "R3"          }
    , { eReverseL    , "Reverse"     }
    , { erRNA_primerL, "Rrna_primer" }
};
ENUM_STR_CONVERT(Label, ELabel, labels)
static rc_t ParseSpotDesc(const KXMLNode* nodeSpotDesc,
    MetaDataXml* md)
{
    rc_t rc = 0;
    const KXMLNodeset* eNs = NULL;
    assert(nodeSpotDesc);
    if (rc == 0) {
        uint32_t i = 0;
        const char path[] = "SPOT_DECODE_SPEC/READ_SPEC";
        rc = KXMLNodeOpenNodesetRead(nodeSpotDesc, &eNs, path);
        DISP_RC2(rc, path, "while opening Nodeset");
        if (rc == 0) {
            uint32_t n = 0;
            rc = KXMLNodesetCount(eNs, &n);
            DISP_RC2(rc, path, "while counting Nodeset");
            if (rc == 0) {
                DBGMSG(DBG_APP, DBG_COND_1, ("READ_SPEC Count = %d\n", n));
                md->nreads = n;
            }
            for (i = 0; i < n && rc == 0; ++i) {
                const KXMLNode* node = NULL;
                rc = KXMLNodesetGetNodeRead(eNs, &node, i);
                if (rc != 0) {
                    PLOGERR(klogInt, (klogInt, rc,
                        "while reading node $(n) of $(path)", "n=%d,path=%s",
                        i, path));
                }
                else {
                    uint32_t index = UINT32_MAX;
                    char* rclass = NULL;
                    char* rtype = NULL;
                    uint32_t j, c = 0;
                    rc = KXMLNodeCountChildNodes(node, &c);
                    DISP_RC2(rc, path, "while counting Children");
                    for (j = 0; j < c && rc == 0; ++j) {
                        const KXMLNode* child;
                        rc = KXMLNodeGetNodeRead(node, &child, j);
                        if (rc != 0) {
                            PLOGERR(klogInt, (klogInt, rc,
                                "while reading child $(n) of $(path)",
                                "n=%d,path=%s", j, path));
                        }
                        else {
                            const char* name = NULL;
                            rc = KXMLNodeElementName(child, &name);
                            if (rc != 0) {
                                PLOGERR(klogInt, (klogInt, rc,
                                  "while reading name of child $(n) of $(path)",
                                  "n=%d,path=%s", j, path));
                            }
                            else {
                                if (!strcmp(name, "BASE_COORD")) {
                                }
                                else if (!strcmp(name,
                                    "EXPECTED_BASECALL"))
                                {}
                                else if (!strcmp(name,
                                    "EXPECTED_BASECALL_TABLE"))
                                {}
                                else if (!strcmp(name, "READ_CLASS")) {
                                    rc = KXMLNodeReadCStr(child, &rclass, NULL);
                                    DISP_RC2(rc, name, "reading as CString");
                                    DBGMSG(DBG_APP, DBG_COND_1,
                                        ("%d %s %s\n", i, name, rclass));
                                }
                                else if (!strcmp(name, "READ_INDEX")) {
                                    rc = KXMLNodeReadAsU32(child, &index);
                                    DISP_RC2(rc, name, "reading as U32");
                                    DBGMSG(DBG_APP, DBG_COND_1,
                                        ("%d %s %d\n", i, name, index));
                                }
                                else if (!strcmp(name, "READ_LABEL")) {
                                }
                                else if (!strcmp(name, "READ_TYPE")) {
                                    rc = KXMLNodeReadCStr(child, &rtype, NULL);
                                    DISP_RC2(rc, name, "reading as CString");
                                    DBGMSG(DBG_APP, DBG_COND_1,
                                        ("%d %s %s\n", i, name, rtype));
                                }
                                else if (!strcmp(name, "RELATIVE_ORDER")) {
                                }
                                else {
                                    rc = RC(rcExe, rcXmlDoc,
                                        rcParsing, rcName, rcUnexpected);
                                    PLOGERR(klogInt, (klogInt, rc,
                                        "$(name)", "name=%s", name));
                                }
                            }
                        }
                        {
                            rc_t rc2 = KXMLNodeRelease(child);
                            if (rc2 && !rc)
                            {   rc = rc2; }
                            child = NULL;
                        }
                    }
                    if (rc == 0) {
                        /*if (bsCoord && expBCall) {
                            rc = RC(rcExe,
                                rcXmlDoc, rcParsing, rcData, rcInvalid);
                            LOGERR(klogInt, rc,
                                "both BASE_COORD and EXPECTED_BASECALL "
                                "are present");
                        }
                        else*/ if (index == UINT32_MAX
                            || index >= sizeof md->read / sizeof md->read[0]
                            || md->read[index].used)
                        {
                            RC(rcExe, rcXmlDoc, rcParsing, rcData, rcDuplicate);
                            PLOGERR(klogInt, (klogInt, rc,
                                "READ_INDEX $(i)", "i=%d", index));
                        }
                        /*else if (bsCoord) {
                            DBGMSG(DBG_APP, DBG_COND_1, ("%d %s %s bk=%d\n",
                                index, rclass, rtype, bsCoord));
                            md->read[index].baseCoord = bsCoord;
                        }
                        else if (expBCall) {
                            md->read[index].expectedBaseCall = expBCall;
                            DBGMSG(DBG_APP, DBG_COND_1,
                               ("%d %s %s eb=%s(%lu)\n", index,
                                rclass, rtype, expBCall, strlen(expBCall)));
                        }
                        else if (reltOrder) {
                            md->read[index].relativeOrder = true;
                        }
                        else {
                            rc = RC(rcExe,
                                rcXmlDoc, rcParsing, rcData, rcUnexpected);
                            LOGERR(klogInt, rc,
                                "XML Read coordinates were not recognized");
                        }*/
                    }
                    if (rc == 0) {
                        SReadXml* r = &md->read[index];
                        int readClass = unsetReadClass;
                        enum EReadType readType = eUnknownRT;

                        r->used = true;

                        if (!strcmp(rclass, "Technical Read"))
                        {   readClass = SRA_READ_TYPE_TECHNICAL; }
                        else if (!strcmp(rclass, "Application Read"))
                        {   readClass = SRA_READ_TYPE_BIOLOGICAL; }
                        r->readClass = readClass;

/*                      readType = ; TODO */
                        if (!strcmp(rtype, "Adapter"))
                        {   readType = eAdapterRT; }
                        else if (!strcmp(rtype, "BarCode"))
                        {   readType = eBarCodeRT; }
                        else if (!strcmp(rtype, "Forward"))
                        {   readType = eForwardRT; }
                        else if (!strcmp(rtype, "Linker"))
                        {   readType = eLinkerRT; }
                        else if (!strcmp(rtype, "Primer"))
                        {   readType = ePrimerRT; }
                        else if (!strcmp(rtype, "Reverse"))
                        {   readType = eReverseRT; }
                        else {
                            rc = RC(rcExe,
                                rcXmlDoc, rcParsing, rcData, rcUnexpected);
                            PLOGERR(klogInt, (klogInt, rc,
                                "READ_TYPE value: $(val)", "val=%s", rtype));
                        }
                        r->readType = readType;
                    }
                }
                {
                    rc_t rc2 = KXMLNodeRelease(node);
                    if (rc2 && !rc)
                    {   rc = rc2; }
                    node = NULL;
                }
            }
        }
        for (i = 0; i < md->nreads && rc == 0; ++i) {
            SReadXml* r = &md->read[i];
            if (!r->used) {
                rc = RC(rcExe, rcXmlDoc, rcParsing, rcData, rcInconsistent);
                PLOGERR(klogInt,
                    (klogInt, rc, "READ_INDEX $(i)", "i=%d", i));
            }
        }
    }
    DESTRUCT(KXMLNodeset, eNs);
    return rc;
}

static rc_t ParseXml(const KXMLDoc* exp, const KXMLDoc* run,
    MetaDataXml* md)
{
    rc_t rc = 0;
    const KXMLNode* nodeExp = NULL;
    const KXMLNode* nodeSpotDesc = NULL;
    assert(exp && run && md);
    if (rc == 0) {
        const KXMLNodeset* ns = NULL;
        const char runNode[] = "/RUN/SPOT_DESCRIPTOR";
        rc = KXMLDocOpenNodesetRead(run, &ns, runNode);
        DISP_RC2(rc, runNode, "while opening Nodeset");
        if (rc == 0) {
            uint32_t n = 0;
            rc = KXMLNodesetCount(ns, &n);
            DISP_RC2(rc, runNode, "while counting Nodeset");
            if (rc == 0) {
                if (n > 1) {
                    rc = RC(rcExe, rcXmlDoc, rcReading, rcNode, rcExcessive);
                    PLOGERR(klogInt, (klogInt, rc,
                        "$(path) in run XML", "path=%s", runNode));
                }
                else if (n == 1) {
                    rc = KXMLNodesetGetNodeRead(ns, &nodeSpotDesc, 0);
                    if (rc == 0) {
                        DBGMSG(DBG_APP, DBG_COND_1,
                            ("Use RUN SPOT_DECSRIPTOR\n"));
                        rc = ParseSpotDesc(nodeSpotDesc, md);
                    }
                }
            }
        }
        DESTRUCT(KXMLNodeset, ns);
    }
    if (rc == 0) {
        const char path[] = "/EXPERIMENT";
        const KXMLNodeset* ns = NULL;
        rc = KXMLDocOpenNodesetRead(exp, &ns, path);
        DISP_RC2(rc, path, "while opening Nodeset");
        if (rc == 0) {
            rc = KXMLNodesetGetNodeRead(ns, &nodeExp, 0);
            DISP_RC2(rc, path, "while accessing Node 0");
        }
    }
    if (rc == 0) {
        const char path[] = "PLATFORM";
        const KXMLNode* node = NULL;
        rc = KXMLNodeGetFirstChildNodeRead(nodeExp, &node, path);
        DISP_RC2(rc, path, "while accessing first Node");
        if (rc == 0) {
            const KNamelist* children = NULL;
            rc = KXMLNodeListChild(node, &children);
            DISP_RC2(rc, path, "while listing children");
            if (rc == 0) {
                const char* name = NULL;
                rc = KNamelistGet(children, 0, &name);
                DISP_RC2(rc, path, "while getting child[0]");
                if (rc == 0) {
                    DBGMSG(DBG_APP, DBG_COND_1, ("PLATFORM = %s\n", name));
                    md->platform = StrPlatform2Enum(name);
                    if (md->platform == SRA_PLATFORM_UNDEFINED) {
                        rc = RC(rcExe,
                            rcXmlDoc, rcParsing, rcId, rcUnrecognized);
                        DISP_RC2(rc, name, "PLATFORM");
                    }
                }
            }
        }
    }
    if (rc == 0 && nodeSpotDesc == NULL) {
        const char path[] = "DESIGN/SPOT_DESCRIPTOR";
        rc = KXMLNodeGetFirstChildNodeRead(nodeExp, &nodeSpotDesc, path);
        DISP_RC2(rc, path, "while accessing first Node");
        if (rc == 0) {
            DBGMSG(DBG_APP, DBG_COND_1, ("Use EXPERIMENT SPOT_DESCRIPTOR\n"));
            rc = ParseSpotDesc(nodeSpotDesc, md);
        }
    }
    DESTRUCT(KXMLNode, nodeExp);
    DESTRUCT(KXMLNode, nodeSpotDesc);
    return rc;
}

static void ReportDiff(const CmdLine* aArgs, const MetaDataXml* xml,
    const char *format, ...)
{
    static bool called = false;
    char buffer[256];
    va_list args;
    va_start(args, format);
    assert(aArgs && xml && format);
    if (!called) {
        called = true;
        OUTMSG(("%s %s ", aArgs->acc, EnumPlatform2Str(xml->platform)));
    }
    else { OUTMSG((" ")); }
    vsnprintf(buffer, sizeof buffer, format, args);
    OUTMSG((buffer));
    va_end(args);
}

static bool AnalyzeNReport(const MetaDataDb* db, const MetaDataXml* xml,
    const CmdLine* args)
{
    ReportDiff(args, xml, "NREADS(X:%d/M:%d)", xml->nreads, db->nreads);
    if (db->platform == SRA_PLATFORM_ABSOLID
        && xml->nreads == 2 && db->nreads == 1
        && xml->read[0].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[0].readType == eForwardRT
        && xml->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[1].readType == eForwardRT
        && db ->read[0].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[0].readOrientation == 0
        &&(db ->read[0].label == eF3L || db ->read[0].label == eR3L)
    ) /* 1/2: BioFw,BioFw / BioF3|R3 */
    {   ReportDiff(args, xml, "[FF-F]"); } /* ForwardForward-Fwd */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 1 && db->nreads == 2
        && xml->read[0].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[0].readType == eForwardRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eFragmentL
    ) /* 1/2: BioFw / TecAda,BioFra */
    {   ReportDiff(args, xml, "[F-AF]"); } /* Forward-AdapterFragment */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 3 && db->nreads == 2
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[0].readType == eAdapterRT
        && xml->read[1].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[1].readType == eBarCodeRT
        && xml->read[2].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[2].readType == eForwardRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eFragmentL
    ) /* 3/2: TecAd,TecBc,BioFw / TecAda,BioFra */
    {   ReportDiff(args, xml, "[BF]"); } /* BarcodeForward */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 3 && db->nreads == 2
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[0].readType == eAdapterRT
        && xml->read[1].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[1].readType == ePrimerRT
        && xml->read[2].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[2].readType == eAdapterRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eFragmentL
    ) /* 3/2: TecAd,TecPr,BioAd / TecAda,BioFra */
    {   ReportDiff(args, xml, "[PA]"); } /* PrimerAdapter */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 4 && db->nreads == 2
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[1].readType == eForwardRT
        && xml->read[2].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[2].readType == eLinkerRT
        && xml->read[3].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eFragmentL
        && (xml->read[0].readType == eAdapterRT ||
            xml->read[0].readType == eForwardRT)
        && (xml->read[3].readType == eForwardRT ||
            xml->read[3].readType == eReverseRT)
    ) /* 4/2: TecAd|Fw,BioFw,TecLn,BioFw|Rv, TecAda,BioFra */
    {   ReportDiff(args, xml, "[FLF]"); } /* ForwardLinkerForward */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 5 && db->nreads == 2
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[0].readType == eAdapterRT
        && xml->read[1].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[1].readType == eBarCodeRT
        && xml->read[2].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[2].readType == eForwardRT
        && xml->read[3].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[3].readType == eLinkerRT
        && xml->read[4].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[4].readType == eReverseRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eFragmentL
    ) /* 5/2: TecAd,TecBc,BioFw,TecLn,BioRv / TecAda,BioFra */
    /* BarcodeForwardLinkerReverse-Fragment */
    {   ReportDiff(args, xml, "[BFLR-F]"); }
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 2 && db->nreads == 4
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[0].readType == eAdapterRT
        && xml->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[1].readType == eForwardRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eMate1L
        && db ->read[2].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[2].readOrientation == 0
        && db ->read[2].label == eLinkerL
        && db ->read[3].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[3].readOrientation == 0
        && db ->read[3].label == eMate2L
    ) /* 2/4: TecAd,BioFw / TecAda,BioMt1,TecLin,MioMt2 */
    {   ReportDiff(args, xml, "[F-MLM]"); } /* Forward-MateLinkerMate */
    else if (db->platform == SRA_PLATFORM_454
        && xml->nreads == 5 && db->nreads == 4
        && xml->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[0].readType == eAdapterRT
        && xml->read[1].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[1].readType == eBarCodeRT
        && xml->read[2].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[2].readType == eForwardRT
        && xml->read[3].readClass == SRA_READ_TYPE_TECHNICAL
        && xml->read[3].readType == eLinkerRT
        && xml->read[4].readClass == SRA_READ_TYPE_BIOLOGICAL
        && xml->read[4].readType == eReverseRT
        && db ->read[0].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[0].readOrientation == 0
        && db ->read[0].label == eAdapterL
        && db ->read[1].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[1].readOrientation == 0
        && db ->read[1].label == eMate1L
        && db ->read[2].readClass == SRA_READ_TYPE_TECHNICAL
        && db ->read[2].readOrientation == 0
        && db ->read[2].label == eLinkerL
        && db ->read[3].readClass == SRA_READ_TYPE_BIOLOGICAL
        && db ->read[3].readOrientation == 0
        && db ->read[3].label == eMate2L
    ) /* 5/4: TecAd,TecBc,BioFw,TecLn,BioRv / TecAda,BioMt1,TecLin,MioMt2 */
    {   ReportDiff(args, xml, "[BF-M]"); } /* BarcodeForward-Mate */
    else {
        int i = 0;
        ReportDiff(args, xml, "UNKNOWN X{");
        for (i = 0; i < xml->nreads; ++i) {
            if (i != 0)
            {   ReportDiff(args, xml, ","); }
            ReportDiff(args, xml, "%d:", i);
            switch (xml->read[i].readClass) {
                case SRA_READ_TYPE_BIOLOGICAL:
                    ReportDiff(args, xml, "BIO"); break;
                case SRA_READ_TYPE_TECHNICAL:
                    ReportDiff(args, xml, "TEC"); break;
                default: assert(0);
            }
            switch (xml->read[i].readType) {
                case eAdapterRT:
                    ReportDiff(args, xml, "AD"); break;
                case eBarCodeRT:
                    ReportDiff(args, xml, "BC"); break;
                case eForwardRT:
                    ReportDiff(args, xml, "FW"); break;
                case eLinkerRT:
                    ReportDiff(args, xml, "LN"); break;
                case ePrimerRT:
                    ReportDiff(args, xml, "PR"); break;
                case eReverseRT:
                    ReportDiff(args, xml, "RV"); break;
                default: assert(0);
            }
        }
        ReportDiff(args, xml, "} D{");
        for (i = 0; i < db->nreads; ++i) {
            if (i != 0)
            {   ReportDiff(args, xml, ","); }
            ReportDiff(args, xml, "%d:", i);
            switch (db->read[i].readClass) {
                case SRA_READ_TYPE_BIOLOGICAL:
                    ReportDiff(args, xml, "BIO"); break;
                case SRA_READ_TYPE_TECHNICAL:
                    ReportDiff(args, xml, "TEC"); break;
                default: assert(0);
            }
            {
                const char* l = EnumLabel2Str(db->read[i].label);
                assert(l);
                ReportDiff(args, xml, l);
            }
            /*switch (db->read[i].label) {
                case eAdapterL:
                    ReportDiff(args, xml, "ADA"); break;
                case eF3L:
                    ReportDiff(args, xml, "F3"); break;
                case eFragmentL:
                    ReportDiff(args, xml, "FRA"); break;
                case eLinkerL:
                    ReportDiff(args, xml, "LNK"); break;
                case eMate1L:
                    ReportDiff(args, xml, "MT1"); break;
                case eMate2L:
                    ReportDiff(args, xml, "MT2"); break;
                case eR3L:
                    ReportDiff(args, xml, "R3"); break;
                default: assert(0);
            }*/
        }
        ReportDiff(args, xml, "}");
        return false;
    }
    return true;
}
static bool MDCompare(const MetaDataDb* db, const MetaDataXml* xml,
    const CmdLine* args)
{
    bool res = true;
    if (db->platform != xml->platform) {
        ReportDiff(args, xml, "PLATFORM");
        res = false;
    }
    if (db->platform != SRA_PLATFORM_PACBIO_SMRT) {
        if (db->nreads != xml->nreads) {
            bool k = AnalyzeNReport(db, xml, args);
            if (!k)
            {   OUTMSG(("\n")); }
            assert(k);
            res = false;
        }
        else {
            int i = 0;
            for (i = 0; i < db->nreads; ++i) {
                if (db->read[i].label == eInvalidF7L) {
                    ReportDiff(args, xml, "ERROR label=F,length=7");
                    res = false;
                }
            }
        }
    }
    if (res == true) {
        OUTMSG(("%s %s ", args->acc, EnumPlatform2Str(xml->platform)));
        if (db->platform != SRA_PLATFORM_PACBIO_SMRT)
        {   OUTMSG(("%d ", db->nreads));    }
        OUTMSG(("EQUALS\n"));
    }
    else { OUTMSG(("\n")); }
    return res;
}

static void MetaDataXmlInit(MetaDataXml* md)
{
    int i = 0;
    assert(md);
    memset(md, 0, sizeof *md);
    for (i = 0; i < sizeof md->read / sizeof md->read[0]; ++i) {
        SReadXml* r = &md->read[i];
        r->readClass = unsetReadClass;
    }
}

static void MetaDataDbInit(MetaDataDb* md)
{
    int i = 0;
    assert(md);
    memset(md, 0, sizeof *md);
    for (i = 0; i < sizeof md->read / sizeof md->read[0]; ++i) {
        SReadDb* r = &md->read[i];
        r->readClass = unsetReadClass;
    }
}

static rc_t ReadColumns(const VTable* tbl,
    Col* columns, int n, MetaDataDb* md, bool variableReadLen)
{
    rc_t rc = 0;
    const VCursor* curs = NULL;
    KNamelist* names = NULL;
    uint32_t nreads = 0;
    int i = 0;
    int colLabel = -1, colLabelLen = -1, colLabelStart = -1;
    assert(tbl && columns && md);
    rc = VTableListCol(tbl, &names);
    DISP_RC(rc, "while listing columns");
    if (rc == 0) {
        int i = 0;
        uint32_t count = 0;
        rc = KNamelistCount(names, &count);
        DISP_RC(rc, "while counting column list");
        for (i = 0; i < count && rc == 0; ++i) {
            int j = 0;
            const char* name = NULL;
            rc = KNamelistGet(names, i, &name);
            DISP_RC(rc, "while getting column name");
            for (j = 0; j < n && rc == 0; ++j) {
                if (strcmp(columns[j].name, name) == 0) {
                    columns[j].exists = true;
                    break;
                }
            }
        }
        DESTRUCT(KNamelist, names);
    }
    if (rc == 0) {
        rc = VTableCreateCursorRead(tbl, &curs);
        DISP_RC(rc, "while creating cursor");
    }
    for (i = 0; i < n && rc == 0; ++i) {
        Col* col = columns + i;
        assert(col);
        if (!col->exists)
        {   continue; }
        rc = VCursorAddColumn(curs, &col->idx, "%s", col->name);
        DISP_RC2(rc, col->name, "while adding column to cursor");
    }
    if (rc == 0) {
        rc = VCursorOpen(curs);
        DISP_RC(rc, "while opening cursor");
    }
    for (i = 0; i < n && rc == 0; ++i) {
        Col* col = &columns[i];
        int64_t row_id = 1;
        uint32_t elem_bits = 8;
        col->row_len = 0;
        if (!col->exists)
        {   continue; }
        if (rc == 0) {
            rc = VCursorReadDirect(curs, row_id, col->idx,
                elem_bits, col->buffer, sizeof col->buffer, &col->row_len);
            if (rc) {
                PLOGERR(klogInt,
                    (klogInt, rc, "$(col): $(expected)", "col=%s,expected=%d",
                     col->name, col->row_len));
            }
        }
        if (rc == 0 && col->type == eREAD_START) {
            nreads = col->row_len / 4;
            assert(col->row_len % 4 == 0);
            md->nreads = nreads;
            if (nreads > sizeof md->read / sizeof md->read[0]) {
                rc = RC
                    (rcExe, rcColumn, rcReading, rcBuffer, rcInsufficient);
                PLOGERR(klogInt, (klogInt, rc,
                    "while reading NREADS: received $(nreads)",
                    "nreads=%d", nreads));
            }
        }
        if (rc == 0) {
            int64_t expected = -1;
            int64_t max = sizeof col->buffer;
            switch (col->datatype) {
                case eAscii:
                    --max;
                    expected = 0; break;
                case eReadType:
                case eUint8:
                    expected = 1; break;
                case eUint32:
                    expected = 4; break;
                default:
                    assert(0)   ; break;
            }
            if (expected) {
                assert((expected * (1 + col->isArray * (nreads - 1)))
                    == col->row_len);
            }
            if (col->row_len > max) {
                rc = RC(rcExe, rcColumn, rcReading, rcBuffer, rcInsufficient);
                PLOGERR(klogInt, (klogInt, rc,
                    "while reading $(column): received $(len)",
                    "column=%s,len=%d", col->name, col->row_len));
            }
            else if (col->datatype == eAscii)
            {   col->buffer[col->row_len] = '\0'; }
            switch (col->type) {
                case eLABEL:
                    colLabel = i;
                    break;
                case eLABEL_LEN:
                    colLabelLen = i;
                    break;
                case eLABEL_START:
                    colLabelStart = i;
                    break;
                default: /* does not matter */
                    break;
            }
        }
    }
    for (i = 0; i < n && rc == 0; ++i) {
        Col* col = &columns[i];
        switch (col->datatype) {
            case eAscii: {
                const char* data = (char*)col->buffer;
                DBGMSG(DBG_APP, DBG_COND_1, ("%s: %s\n", col->name, data));
                break;
            }
            case eReadType: {
                uint8_t i = 0;
                char* data = (char*)col->buffer;
                for (i = 0; i < nreads; ++i) {
                    char t[64] = "";
                    if (data[i] & SRA_READ_TYPE_BIOLOGICAL) {
                        strcpy(t, "SRA_READ_TYPE_BIOLOGICAL");
                        data[i] &= ~SRA_READ_TYPE_BIOLOGICAL;
                        md->read[i].readClass = SRA_READ_TYPE_BIOLOGICAL;
                    } else {
                        assert(SRA_READ_TYPE_TECHNICAL == 0);
                        strcpy(t, "SRA_READ_TYPE_TECHNICAL");
                        data[i] &= ~SRA_READ_TYPE_TECHNICAL;
                        md->read[i].readClass = SRA_READ_TYPE_TECHNICAL;
                    }
                    if (data[i] & SRA_READ_TYPE_FORWARD) {
                        strcat(t, "|SRA_READ_TYPE_FORWARD");
                        data[i] &= ~SRA_READ_TYPE_FORWARD;
                        md->read[i].readOrientation = SRA_READ_TYPE_FORWARD;
                    }
                    if (data[i] & SRA_READ_TYPE_REVERSE) {
                        strcat(t, "|SRA_READ_TYPE_REVERSE");
                        data[i] &= ~SRA_READ_TYPE_REVERSE;
                        md->read[i].readOrientation = SRA_READ_TYPE_REVERSE;
                    }
                    if (data[i]) {
                        rc = RC(rcExe,
                            rcColumn, rcReading, rcData, rcUnexpected);
                        LOGERR(klogInt, rc, "unexpected READ_TYPE value");
                    }
                    else {
                        if (i == 0) {
                            DBGMSG(DBG_APP, DBG_COND_1,
                                ("%s: %s", col->name, t));
                        } else {    DBGMSG(DBG_APP, DBG_COND_1, (", %s", t)); }
                    }
                }
                DBGMSG(DBG_APP, DBG_COND_1, ("\n"));
                break;
            }
            case eUint8:
                DBGMSG(DBG_APP, DBG_COND_1,
                    ("%s: %d\n", col->name, *(uint8_t*)col->buffer));
                break;
            case eUint32: {
                uint8_t i = 0;
                uint32_t* data = (uint32_t*)col->buffer;
                DBGMSG(DBG_APP, DBG_COND_1, ("%s: %d", col->name, data[0]));
                for (i = 1; col->isArray && i < nreads; ++i)
                {   DBGMSG(DBG_APP, DBG_COND_1, (", %d", data[i])); }
                DBGMSG(DBG_APP, DBG_COND_1, ("\n"));
                break;
            }
            default:
                assert(0);
                break;
        }
        switch (col->type) {
            case ePLATFORM:
                md->platform = *(uint8_t*)col->buffer;
                break;
            default: /* does not matter */
                break;
        }
    }
    if (rc == 0) {
        if (colLabel == -1 && colLabelLen == -1 && colLabelStart == -1)
        {}
        else if (colLabel == -1
            || colLabelLen == -1 || colLabelStart == -1)
        {
            rc = RC(rcExe, rcCursor, rcReading, rcColumn, rcNotFound);
            LOGERR(klogInt, rc, "(one of LABEL columns)");
        }
        else {
            int j = 0;
            int labell = 0;
            const char* label = (const char*)columns[colLabel].buffer;
            const uint32_t* labelLen
                = (const uint32_t*)columns[colLabelLen].buffer;
            const uint32_t* labelStart
                = (const uint32_t*)columns[colLabelStart].buffer;
            for (j = 0; j < nreads; ++j) {
                enum ELabel elabel = eUnsetL;
                if (labelLen[j]) {
                    if (labelStart[j] == 0 && labelLen[j] == 7
                        && label[0] == 'F' && label[1] == '\0')
                    {   elabel = eInvalidF7L; }
                    else {
                        elabel =
                            StrnLabel2Enum(label + labelStart[j], labelLen[j]);
                    }
                    /* if (!Compare("Adapter", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eAdapterL; }
                    else if (!Compare("barcode", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eBarcodeL; }
                    else if (!Compare("F", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eFL; }
                    else if (!Compare("F3", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eF3L; }
                    else if (!Compare("Fragment", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eFragmentL; }
                    else if (!Compare("forward", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eForwardL; }
                    else if (!Compare("Linker", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eLinkerL; }
                    else if (!Compare("Mate1", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eMate1L; }
                    else if (!Compare("Mate2", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eMate2L; }
                    else if (!Compare("R3", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eR3L; }
                    else if (!Compare("reverse", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = eReverseL; }
                    else if (!Compare("rRNA_primer", label + labelStart[j],
                        labelLen[j]))
                    {   elabel = erRNA_primerL; }
                    else*/
                    if (elabel == eUnsetL) {
                        rc = RC(rcExe, rcData, rcParsing, rcData, rcUnexpected);
                        PLOGERR(klogInt,
                            (klogInt, rc, "LABEL value: $(val), length: $(len)",
                             "val=%.*s,len=%d",
                            labelLen[j], label + labelStart[j], labelLen[j]));
                    }
                }
                md->read[j].label = elabel;
                labell += labelLen[j];
            }
        }
    }
    DESTRUCT(VCursor, curs);
    return rc;
}

static rc_t MakeXmlDocs(const CmdLine* args,
    const KXMLDoc** exp, const KXMLDoc** run)
{
    rc_t rc = 0;
    const KXMLMgr* mgr = NULL;
    assert(args && exp && run);
    rc = KXMLMgrMakeRead(&mgr);
    DISP_RC(rc, "while calling KXMLMgrMakeRead");
    if (rc)
    {   return rc; }
    if (args->exp && args->run) {
        KDirectory* dir = NULL;
        const KFile* expF = NULL;
        const KFile* runF = NULL;
        if (rc == 0) {
            rc = KDirectoryNativeDir(&dir);
            DISP_RC(rc, "while calling KDirectoryNativeDir");
        }
        if (rc == 0) {
            rc = KDirectoryOpenFileRead(dir, &expF, args->exp);
            DISP_RC2(rc, args->exp, "while opening file");
        }
        if (rc == 0) {
            rc = KDirectoryOpenFileRead(dir, &runF, args->run);
            DISP_RC2(rc, args->run, "while opening file");
        }
        if (rc == 0) {
            rc = KXMLMgrMakeDocRead(mgr, exp, expF);
            DISP_RC2(rc, args->exp, "while making experiment XML doc");
        }
        if (rc == 0) {
            rc = KXMLMgrMakeDocRead(mgr, run, runF);
            DISP_RC2(rc, args->run, "while making run XML doc");
        }
        {
            rc_t rc2 = KFileRelease(runF);
            if (rc2 != 0 && rc == 0)
            {   rc = rc2; }
            runF = NULL;
        }
        {
            rc_t rc2 = KFileRelease(expF);
            if (rc2 != 0 && rc == 0)
            {   rc = rc2; }
            expF = NULL;
        }
        {
            rc_t rc2 = KDirectoryRelease(dir);
            if (rc2 != 0 && rc == 0)
            {   rc = rc2; }
            dir = NULL;
        }
    }
    else if (args->acc) {
        const DBManager* dbMgr = NULL;
        if (rc == 0) {
            rc = SybaseInit(OS_CS_VERSION);
            if (rc != 0)
            {   LOGERR(klogInt, rc, "failed to init Sybase"); }
        }
        if (rc == 0) {
            rc = DBManagerInit(&dbMgr, "sybase");
            if (rc != 0) {
                LOGERR(klogInt, rc, "failed to init Sybase");
            }
        }
        if (rc == 0) {
            Database* info = NULL;
            const char server[] = "NIHMS2";
            const char dbname[] = "SRA_Main";
            const char user[] = "anyone";
            rc = DBManagerConnect
                (dbMgr, server, dbname, user, "allowed", &info);
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc,
                    "failed to connect to $(srv).$(db) as user $(usr)",
                    "srv=%s,db=%s,usr=%s", server, dbname, user));
            }
            if (rc == 0) {
                DBResultSet* rs = NULL;
                rc = DatabaseExecute(info, &rs, "set textsize 1310720");
                DISP_RC(rc, "while setting DB textsize");
            }
            if (rc == 0) {
                DBResultSet* rs = NULL;
                rc = DatabaseExecute(info, &rs,
                    "SELECT r.meta, e.meta FROM Run r, Experiment e "
                    "WHERE r.acc = '%s' AND e.acc = r.experiment_ref",
                    args->acc);
                if (rc != 0) {
                    LOGERR(klogInt, rc, "metadata selecting");
                }
                else {
                    while (rc == 0) {
                        const String* runS = NULL;
                        const String* expS = NULL;
                        DBRow* row = NULL;
                        rc = DBResultSetNextRow(rs, &row);
                        if (rc != 0) {
                            if (rc == RC(rcRDBMS,
                                  rcData, rcRetrieving, rcData, rcNotAvailable))
                                rc = 0;
                            break;
                        }
                        rc = DBRowGetAsString(row, 0, &runS);
                        if (rc != 0) {
                            LOGERR(klogInt, rc, "accessing run meta");
                        }
                        else {
                            rc = KXMLMgrMakeDocReadFromMemory
                                (mgr, run, runS->addr, runS->size);
                            if (rc != 0)
                            {   LOGERR(klogInt, rc, "run.xml"); }
                        }
                        if (rc == 0) {
                            rc = DBRowGetAsString(row, 1, &expS);
                            if (rc != 0) {
                                LOGERR(klogInt, rc, "accessing exp meta");
                            }
                            else {
                                rc = KXMLMgrMakeDocReadFromMemory
                                    (mgr, exp, expS->addr, expS->size);
                            }
                        }
                    }
                    if (rc != 0)
                    {   LOGERR(klogInt, rc, "metadata retieving"); }
                }
            }
        }
        {
            rc_t rc2 = DBManagerRelease(dbMgr);
            if (rc2 != 0 && rc == 0)
            {   rc = rc2; }
            dbMgr = NULL;
        }
    }
    else { rc = RC(rcExe, rcArgv, rcParsing, rcArgv, rcInconsistent); }
    {
        rc_t rc2 = KXMLMgrRelease(mgr);
        if (rc2 != 0 && rc == 0)
        {   rc = rc2; }
        mgr = NULL;
    }
    return rc;
}

rc_t CC UsageSummary (const char * progname)
{
    rc_t rc = 0;
    return rc;
}
const char UsageDefaultName[] = "meta-sync";
rc_t CC Usage(const Args* args)
{
    rc_t rc = 0;
    return rc;
}

ver_t CC KAppVersion(void) { return META_SYNC_VERS; }

static char *vdm_translate_accession( SRAPath* my_sra_path,
    const char* accession,  const size_t bufsize, rc_t* rc)
{
    char* res = malloc(bufsize);
    if (res != NULL) {
        *rc = SRAPathFind(my_sra_path, accession, res, bufsize);
        if (GetRCState(*rc) == rcNotFound) {
            free(res);
            res = NULL;
        }
        else if (GetRCState(*rc) == rcInsufficient) {
            DBGMSG(DBG_APP, 0, ("bufsize %lu was insufficient\n", bufsize));
            free(res);
            res = vdm_translate_accession
                (my_sra_path, accession, bufsize * 2, rc);
        }
        else if (*rc != 0) {
            free(res);
            res = NULL;
        }
    }
    return res;
}

#define OPTION_EXPERIMENT  "experiment"
#define ALIAS_EXPERIMENT   "e"
static const char* experiment_usage [] = { "experiment.xml file" , NULL };
#define OPTION_RUN  "run"
#define ALIAS_RUN   "r"
static const char* run_usage [] = { "run.xml file" , NULL };
#define OPTION_ACC  "accession"
#define ALIAS_ACC   "a"
static const char* acc_usage [] = { "run accession" , NULL };
OptDef Options[] =  {
     { OPTION_ACC, ALIAS_ACC, NULL, acc_usage       , 1, true , true },
     { OPTION_EXPERIMENT,
            ALIAS_EXPERIMENT, NULL, experiment_usage, 1, true , true },
     { OPTION_RUN, ALIAS_RUN, NULL, run_usage       , 1, true , true }
};
static rc_t CmdLineInit(const Args* args, CmdLine* cmdArgs)
{
    rc_t rc = 0;
    bool acc_exp_runError = false;
    assert(args && cmdArgs);

    memset(cmdArgs, 0, sizeof *cmdArgs);

    while (rc == 0) {
        uint32_t pcount = 0;
        uint32_t acccount = 0;
        uint32_t expcount = 0;
        uint32_t runcount = 0;

        /* table path parameter */
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing table name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing table parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many table parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, &cmdArgs->tbl);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving table name");
            break;
        }

        /* run accession parameter */
        rc = ArgsOptionCount(args, OPTION_ACC, &acccount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing run accession");
            break;
        }
        if (acccount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many run accession parameters");
            break;
        }

        /* experiment file parameter */
        rc = ArgsOptionCount(args, OPTION_EXPERIMENT, &expcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing experiment file name");
            break;
        }
        if (expcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many experiment parameters");
            break;
        }

        /* run file parameter */
        rc = ArgsOptionCount(args, OPTION_RUN, &runcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing run file name");
            break;
        }
        if (runcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many run parameters");
            break;
        }

        if (acccount < 1) {
            if (expcount < 1) {
                acc_exp_runError = true;
                break;
            }
            else if (runcount < 1) {
                acc_exp_runError = true;
                break;
            }
            else {
                rc = ArgsOptionValue(args, OPTION_EXPERIMENT, 0, &cmdArgs->exp);
                if (rc) {
                    LOGERR(klogErr, rc,
                        "Failure retrieving experiment file name");
                    break;
                }

                rc = ArgsOptionValue (args, OPTION_RUN, 0, &cmdArgs->run);
                if (rc) {
                    LOGERR(klogErr, rc, "Failure retrieving run file name");
                    break;
                }
            }
        }
        else {
            rc = ArgsOptionValue (args, OPTION_ACC, 0, &cmdArgs->acc);
            if (rc) {
                LOGERR(klogErr, rc, "Failure retrieving run accession");
                break;
            }
        }

        break;
    }

    if (rc == 0) {
        if (strchr(cmdArgs->tbl, '/') == NULL) {
            SRAPath* sraPath = NULL;
            rc = SRAPathMake(&sraPath, NULL);
            if (rc == 0) {
                if (!SRAPathTest(sraPath, cmdArgs->tbl)) {
                    char* buf =
                        vdm_translate_accession(sraPath, cmdArgs->tbl, 64, &rc);
                    if (buf != NULL) {
                        DBGMSG
                            (DBG_APP, 0, ("sra-accession found! >%s<\n", buf));
                        if (acc_exp_runError) {
                            cmdArgs->acc = cmdArgs->tbl;
                            acc_exp_runError = false;
                        }
                        cmdArgs->tbl = buf;
                    }
                    else if (GetRCState(rc) == rcNotFound) {
                        OUTMSG(("%s NOT FOUND\n", cmdArgs->tbl));
                        cmdArgs->notFound = true;
                        rc = 0;
                    }
                    DISP_RC2(rc, cmdArgs->tbl, "while looking for table");
                }
            }
            else {
                if (GetRCState(rc) != rcNotFound ||
                    GetRCTarget(rc) != rcDylib)
                {   LOGERR( klogInt, rc, "SRAPathMake failed" ); }
                else { rc = 0; }
            }
        }
    }
    if (cmdArgs->notFound == false && rc == 0 && acc_exp_runError) {
        rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
        LOGERR(klogErr, rc,
            "Either accession or run/experiment XML should be specified");
    }
/*
    else
    {   MiniUsage (args); }
*/
    return rc;
}

static void ShutUp(void) {
                         StrLabel2Enum   (0); StrnPlatform2Enum(0, 0);
    EnumReadType2Str(0); StrReadType2Enum(0); StrnReadType2Enum(0, 0);
}

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;
    Args* args = NULL;
    CmdLine cmdArgs;
    Col columns[] = {
/*        { eNREADS         ,"NREADS"         , eUint8   , false } , */
        /* READ_START should be the first column in the array */
          { eREAD_START     ,"READ_START"     , eUint32  , true  }
        , { eFIXED_SPOT_LEN ,"FIXED_SPOT_LEN" , eUint32  , false }
        , { eLABEL          ,"LABEL"          , eAscii   , false }
        , { eLABEL_LEN      ,"LABEL_LEN"      , eUint32  , true  }
        , { eLABEL_START    ,"LABEL_START"    , eUint32  , true  }
        , { eLINKER_SEQUENCE,"LINKER_SEQUENCE", eAscii   , false }
        , { ePLATFORM       ,"PLATFORM"       , eUint8   , false }
        , { eREAD_LEN       ,"READ_LEN"       , eUint32  , true  }
        , { eREAD_TYPE      ,"READ_TYPE"      , eReadType, true  }
        , { eSPOT_GROUP     ,"SPOT_GROUP"     , eAscii   , false }
        , { eSPOT_LEN       ,"SPOT_LEN"       , eUint32  , false }
    };
    rc = ArgsMakeAndHandle
        (&args, argc, argv, 1, Options, sizeof Options / sizeof(Options[0]));
    if (rc == 0)
    {   rc = CmdLineInit(args, &cmdArgs); }
    if (cmdArgs.notFound == false) {
        bool equal = true;
        const KXMLDoc* exp = NULL;
        const KXMLDoc* run = NULL;
        const VDBManager* mgr = NULL;
        const VDatabase *db = NULL;
        const VTable* tbl = NULL;
        VSchema* schema = NULL;
        MetaDataXml xmlMd;
        MetaDataDb  runMd;
        MetaDataXmlInit(&xmlMd);
        MetaDataDbInit (&runMd);
        if (rc == 0)
        {   rc = MakeXmlDocs(&cmdArgs, &exp, &run); }
        if (rc == 0)
        {   rc = ParseXml(exp, run, &xmlMd); }
        if (rc == 0) {
            rc = VDBManagerMakeRead(&mgr, NULL);
        }
        if (rc == 0) {
            rc = VDBManagerMakeSRASchema(mgr, &schema);
        }
        if (rc == 0) {
            rc = VDBManagerOpenDBRead(mgr, &db, schema, "%s", cmdArgs.tbl);
            if (rc == 0) {
                const char path[] = "SEQUENCE";
                rc = VDatabaseOpenTableRead(db, &tbl, path);
                DISP_RC2(rc, cmdArgs.tbl, "while opening DB table SEQUENCE");
            }
            else {
                rc = VDBManagerOpenTableRead(mgr, &tbl, schema, "%s", cmdArgs.tbl);
                DISP_RC2(rc, cmdArgs.tbl, "while opening table");
            }
        }
        if (rc == 0) {
            rc = ReadColumns(tbl, columns, sizeof columns / sizeof columns[0],
                &runMd, xmlMd.variableReadLen);
        }
        if (rc == 0)
        {   equal = MDCompare(&runMd, &xmlMd, &cmdArgs); }
        DESTRUCT(VTable, tbl);
        DESTRUCT(VDatabase, db);
        DESTRUCT(VSchema, schema);
        DESTRUCT(VDBManager, mgr);
        DESTRUCT(KXMLDoc, run);
        DESTRUCT(KXMLDoc, exp);
    }
    {
        rc_t rc2 = ArgsWhack(args);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
            ShutUp();
        }
    }
    return rc;
}

/************************************ EOF ****************** ******************/
