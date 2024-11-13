// search-vdb.cpp

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

#include "search-vdb.h"

#include <sysalloc.h>

#include <search/extern.h>
#include <search/grep.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <set>
#include <algorithm>

#ifndef min
#define min(a, b) ((b) < (a) ? (b) : (a))
#endif

#define RELEASE_VDB_HANDLE(Type, pObject)\
    {\
        if (pObject)\
        {\
            ::Type##Release(pObject);\
            pObject = NULL;\
        }\
    }

namespace VDBSearchTest
{
    // Structures required for libs/search

    struct agrepinfo
    {
        AgrepBestMatch* pBestMatch; // TODO: remove this redundant reference and use AgrepBestMatch directly
    };

    // callback required for libs/search
    rc_t agrep_callback_find_best(void const* cbinfo_void, ::AgrepMatch const* match, ::AgrepContinueFlag* cont)
    {
        agrepinfo const* cbinfo = (agrepinfo*)cbinfo_void;
        *cont = AGREP_CONTINUE;
        if (cbinfo->pBestMatch->nScore > match->score && match->position < cbinfo->pBestMatch->nTrimLen)
        {
            cbinfo->pBestMatch->nScore = match->score;
            cbinfo->pBestMatch->indexStart = match->position;
            cbinfo->pBestMatch->nMatchLength = match->length;
        }

        return 0;
    }

/////////////////////////////////////////////////////////////////

    CFindLinker::CFindLinker()
        : m_pVDBManager(NULL), m_pVDBTable(NULL)
    {
    }

    CFindLinker::~CFindLinker()
    {
        ReleaseVDB();
    }

    void CFindLinker::ReleaseVDB()
    {
        cursorRead.Release();
        //cursorWrite.Release();
        RELEASE_VDB_HANDLE(VTable,     m_pVDBTable);

        if (m_pVDBManager)
        {
            //rc_t rc = ::VDBManagerLock(m_pVDBManager, m_sTableName.c_str());
            //PROCESS_RC_WARNING(rc, "VDBManagerLock");
        }

        RELEASE_VDB_HANDLE(VDBManager, m_pVDBManager);
    }

    bool CFindLinker::InitVDBRead(char const* pszTableName)
    {
        rc_t rc = 0;
        m_sTableName = pszTableName;

        rc = ::VDBManagerMakeRead(const_cast<const VDBManager**>(&m_pVDBManager), NULL);
        //rc = ::VDBManagerMakeUpdate(&m_pVDBManager, NULL);
        PROCESS_RC_ERROR(rc, "VDBManagerMakeRead");

        rc = ::VDBManagerOpenTableRead(m_pVDBManager, const_cast<const VTable**>(&m_pVDBTable), NULL, pszTableName);
        PROCESS_RC_ERROR(rc, "VDBManagerOpenTableRead");

        rc = ::VTableCreateCursorRead(m_pVDBTable, &cursorRead.PtrConstRef());
        PROCESS_RC_ERROR(rc, "VTableCreateCursorRead");

        //std::cout << "Opened table \"" << pszTableName << "\"" << std::endl;

        return true;
    }

#if 0
    bool CFindLinker::InitVDB(char const* pszTableName)
    {
        rc_t rc = 0;
        m_sTableName = pszTableName;

        rc = ::VDBManagerMakeUpdate(&m_pVDBManager, NULL);
        PROCESS_RC_ERROR(rc, "VDBManagerMakeUpdate");

        rc = ::VDBManagerUnlock(m_pVDBManager, pszTableName);
        PROCESS_RC_WARNING(rc, "VDBManagerUnlock");

        rc = ::VDBManagerOpenTableUpdate(m_pVDBManager, &m_pVDBTable, NULL, pszTableName);
        PROCESS_RC_ERROR(rc, "VDBManagerOpenTableUpdate");

        rc = ::VTableCreateCursorRead(m_pVDBTable, &cursorRead.PtrConstRef());
        PROCESS_RC_ERROR(rc, "VTableCreateCursorRead");

        rc = ::VTableCreateCursorWrite(m_pVDBTable, &cursorWrite.PtrRef(), kcmInsert);
        PROCESS_RC_ERROR(rc, "VTableCreateCursorWrite");

        //std::cout << "Opened table \"" << pszTableName << "\"" << std::endl;

        return true;
    }
#endif

    bool CFindLinker::InitColumnIndex()
    {
        char const* ColumnNamesRead[] = {
            COLUMN_READ
            ,COLUMN_READ_START
            ,COLUMN_READ_LEN
            ,COLUMN_TRIM_START
            ,COLUMN_TRIM_LEN
        };
        //char const* ColumnNamesWrite[] = {
        //    COLUMN_READ_START
        //    ,COLUMN_READ_LEN
        //};

        return cursorRead.InitColumnIndex(ColumnNamesRead, countof(ColumnNamesRead));
                //&& cursorWrite.InitColumnIndex(ColumnNamesWrite, countof(ColumnNamesWrite));

    }

    bool CFindLinker::OpenCursor()
    {
        rc_t rc = ::VCursorOpen(cursorRead.PtrConstRef());
        PROCESS_RC_ERROR(rc, "VCursorOpen");
        return true;
    }

    char const* PrepareReadForSearch(char const* pszRead, size_t nReadBufSize, size_t nLinkerLen, int32_t nTrimStart, int32_t nTrimLen, char* pszReadTrimmed, size_t nReadTrimmedSize)
    {
        size_t nTrimmedLen = min((size_t)nTrimLen, nReadTrimmedSize - 1);
        strncpy(pszReadTrimmed, pszRead + nTrimStart, nTrimmedLen);
        pszReadTrimmed[nTrimmedLen] = '\0';

        size_t nExtendBufSize = min(nLinkerLen, nTrimmedLen);
        if (nTrimmedLen + nExtendBufSize > nReadTrimmedSize)
            nExtendBufSize = nReadTrimmedSize - nTrimmedLen;

        memmove(pszReadTrimmed + nTrimmedLen, pszReadTrimmed, nExtendBufSize - 1);
        pszReadTrimmed[nTrimmedLen + nExtendBufSize - 1] = '\0';
        return pszReadTrimmed;
    }

    void PrintCharRange(char const* pszStr, size_t nCount)
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            std::cout << pszStr[i];
        }
    }

    void PrintFindBestResult(AgrepBestMatch const& bm, int64_t idRow, int32_t nTrimStart, int32_t nTrimLen, size_t nLinkerLen, char const* pszReadTrimmed, char const* pszRead, char const* pszLinker)
    {
        std::cout << std::endl
            << "row_id=" << idRow;
        if (bm.nScore == AgrepBestMatch::NOT_FOUND)
        {
            std::cout << " (NOT FOUND)";
        }
        else
        {
            std::cout << std::endl
            << COLUMN_READ << " [" << nTrimStart << ", " << (nTrimStart + nTrimLen) << "]: " << pszRead << std::endl
            << "Linker found: " << pszLinker
            << ", start=" << bm.indexStart + nTrimStart << ", len=" << bm.nMatchLength
            << ", end=" << (bm.indexStart + bm.nMatchLength) % nTrimLen + nTrimStart
            << ", score=" << bm.nScore << " (" << (int)(100. * (double)bm.nScore/nLinkerLen) << "%): ";

            PrintCharRange(pszReadTrimmed + bm.indexStart, min(bm.nMatchLength, nTrimLen - bm.indexStart));
            bool bCutInBetween = (bm.indexStart + bm.nMatchLength) > nTrimLen;
            if (bCutInBetween)
            {
                std::cout << "|";
                PrintCharRange(pszReadTrimmed, bm.nMatchLength - (nTrimLen - bm.indexStart));
            }
        }
        std::cout << std::endl;
    }

    enum CFindLinker::SearchResult CFindLinker::SearchSingleRow(::AgrepFlags flags, int64_t idRow)
    {
        size_t const BUF_SIZE = 4096;
        char szRead[BUF_SIZE], szReadTrimmed[BUF_SIZE];
        uint32_t nItemsRead, nReadLen, nLinkerLen;
        int32_t bufReadLen[4], bufReadStart[4], nTrimStart, nTrimLen;

        if (!ReadItems(idRow, COLUMN_READ,            szRead,      countof(szRead), &nReadLen))
            return CFindLinker::SEARCH_RESULT_ERROR;

        if (!ReadItems(idRow, COLUMN_READ_START,      bufReadStart,   countof(bufReadStart), &nItemsRead))
            return CFindLinker::SEARCH_RESULT_ERROR;
        //std::cout << "items read = " << nItemsRead << std::endl;
        //assert(nItemsRead == countof(bufReadStart));
        //int32_t nVDBLinkerStart = bufReadStart[2];

        if (!ReadItems(idRow, COLUMN_READ_LEN,        bufReadLen,   countof(bufReadLen), &nItemsRead))
            return CFindLinker::SEARCH_RESULT_ERROR;
        //assert(nItemsRead == countof(bufReadLen));
        //int32_t nVDBLinkerLen = bufReadLen[2];

        if (!ReadItems(idRow, COLUMN_TRIM_START,      &nTrimStart, 1, &nItemsRead))
            return CFindLinker::SEARCH_RESULT_ERROR;

        if (!ReadItems(idRow, COLUMN_TRIM_LEN,        &nTrimLen,   1, &nItemsRead))
            return CFindLinker::SEARCH_RESULT_ERROR;
        // fix case when trim is not set properly (len == 0)
        if (nTrimLen == 0)
        {
            nTrimStart = 0;
            nTrimLen = nReadLen;
        }

        static char const* arrLinkers[] = {
            "GTTGGAACCGAAAGGGTTTGAATTCAAACCCTTTCGGTTCCAAC"
            ,"TCGTATAACTTCGTATAATGTATGCTATACGAAGTTATTACG"
            ,"CGTAATAACTTCGTATAGCATACATTATACGAAGTTATACGA"
        };

        // TODO: do something with hardcoded flags
        static CAgrepMyersSearch arrLinkerSearch[] = {
            CAgrepMyersSearch(AGREP_MODE_ASCII|AGREP_ALG_MYERS,  arrLinkers[0])
            ,CAgrepMyersSearch(AGREP_MODE_ASCII|AGREP_ALG_MYERS, arrLinkers[1])
            ,CAgrepMyersSearch(AGREP_MODE_ASCII|AGREP_ALG_MYERS, arrLinkers[2])
        };

        AgrepBestMatch bmBest = {AgrepBestMatch::NOT_FOUND, 0, 0};
        char const* pszBestLinker = NULL;

        for (size_t iLinker = 0; iLinker < countof(arrLinkers); ++iLinker)
        {
            char const* pszLinker = arrLinkers[iLinker];
            nLinkerLen = strlen(pszLinker);

            // Make szBuf somewhat circular to be able to find pattern cut in between
            // And also here we enforce using trimmed region of the pszRead
            char const* pszReadTrimmed = PrepareReadForSearch(szRead, countof(szRead), nLinkerLen, nTrimStart, nTrimLen, szReadTrimmed, countof(szReadTrimmed));;
            AgrepBestMatch bm = {AgrepBestMatch::NOT_FOUND, 0, 0, nTrimLen};

            arrLinkerSearch[iLinker].FindBest(pszReadTrimmed, countof(szRead)-nTrimStart, idRow, nTrimLen, &bm);

            if (bm.nScore != AgrepBestMatch::NOT_FOUND)
            {
                if (bmBest.nScore == AgrepBestMatch::NOT_FOUND || bm.nScore < bmBest.nScore)
                {
                    bmBest = bm;
                    pszBestLinker = pszLinker;
                }
            }
        }

        if (bmBest.nScore != AgrepBestMatch::NOT_FOUND)
        {
            //std::cout << "id=" << idRow << ", linker=\"" << pszBestLinker << "\", start=" << bmBest.indexStart << ", read=\"" << szRead << "\"" << std::endl;
            nLinkerLen = strlen(pszBestLinker);

            double nScoreAgrep = (double)bmBest.nScore/nLinkerLen;
            if (0.22 < nScoreAgrep && nScoreAgrep < 0.333)
            {
                size_t start;
                size_t len = FindLongestCommonSubstring(szReadTrimmed, pszBestLinker, nTrimLen+nLinkerLen, nLinkerLen, &start, NULL);
                double nScoreSubstr = (double)len/nLinkerLen;
                char szBuf[512] = "";
                string_printf(szBuf, countof(szBuf), NULL, "start=%zu, len=%zu: \"%.*s\", %d%%", start, len, len, szReadTrimmed+start, (int)(100.*nScoreSubstr));
                if (nScoreSubstr > 0.3)
                {
                    PrintFindBestResult(bmBest, idRow, nTrimStart, nTrimLen, strlen(pszBestLinker), szReadTrimmed, szRead, pszBestLinker);
                    std::cout << "Approved by substring search: " << szBuf << std::endl;
                }
                else
                {
                    bmBest.nScore = AgrepBestMatch::NOT_FOUND;
                    //PrintFindBestResult(bmBest, idRow, nTrimStart, nTrimLen, strlen(pszBestLinker), szReadTrimmed, szRead, pszBestLinker);
                    //std::cout << "Declined by substring search: " << szBuf << std::endl;
                }

            }
            else
                PrintFindBestResult(bmBest, idRow, nTrimStart, nTrimLen, strlen(pszBestLinker), szReadTrimmed, szRead, pszBestLinker);


            // Write results
            //std::cout
            //    << "Stored start: " << nVDBLinkerStart << std::endl
            //    << "Stored end:   " << nVDBLinkerLen << std::endl
            //    << "New start:    " << bmBest.indexStart + nTrimStart << std::endl
            //    << "New len:      " << bmBest.nMatchLength << std::endl;

            //rc_t rc = ::VCursorOpenRow(cursorWrite.PtrRef());
            //PROCESS_RC_WARNING(rc, "VCursorOpenRow [for writing]")
            //if (rc == 0)
            //{
            //    bufReadStart[2] = bmBest.indexStart + nTrimStart;
            //    bufReadLen[2] = bmBest.nMatchLength;
            //    rc = ::VCursorWrite(cursorWrite.PtrRef(), cursorRead.GetColumnIndex(COLUMN_READ_START), 8 * sizeof(bufReadStart[0]), bufReadStart, 0, countof(bufReadStart));
            //    PROCESS_RC_WARNING(rc, "VCursorWrite (start)")
            //    rc = ::VCursorWrite(cursorWrite.PtrRef(), cursorRead.GetColumnIndex(COLUMN_READ_LEN), 8 * sizeof(bufReadLen[0]), bufReadLen, 0, countof(bufReadLen));
            //    PROCESS_RC_WARNING(rc, "VCursorWrite (len)")
            //}
            //if (rc == 0)
            //{
            //    rc = ::VCursorCommitRow(cursorWrite.PtrRef());
            //    PROCESS_RC_WARNING(rc, "VCursorCommitRow")
            //}
            //if (rc == 0)
            //{
            //    rc = ::VCursorCloseRow(cursorWrite.PtrRef());
            //    PROCESS_RC_WARNING(rc, "VCursorCloseRow")
            //}

        }

        return bmBest.nScore == AgrepBestMatch::NOT_FOUND ?
            CFindLinker::SEARCH_RESULT_NOT_FOUND : CFindLinker::SEARCH_RESULT_FOUND;
    }

    bool CFindLinker::Run(::AgrepFlags flags, char const* pszTableName)
    {
        if (!InitVDBRead(pszTableName))
            return false;

        if (!InitColumnIndex())
            return false;

        if (!OpenCursor())
            return false;

        int64_t idRow = 0;
        uint64_t nRowCount = 0;

        rc_t rc = ::VCursorIdRange(cursorRead.PtrConstRef(), 0, &idRow, &nRowCount);
        PROCESS_RC_ERROR(rc, "VCursorIdRange");

        size_t nCountFound = 0;
        // Trying random row ids to figure out if it worth full scan to find linkers
        std::set<int64_t> arrIDsToTry;
        
        size_t const nTrialSize = min(100, nRowCount);
        size_t nThreshold = 0.25*nTrialSize;
        bool bDoTrial = (nRowCount >= nTrialSize*1000.);
        bDoTrial = false; // TODO: debug

        if (bDoTrial)
        {
            // generating nTrialSize unique ids to try

            while (arrIDsToTry.size() < nTrialSize)
            {
                arrIDsToTry.insert((double)rand()/(double)RAND_MAX*nRowCount);
            }
            // Trial scan of random rows

            for (std::set<int64_t>::const_iterator cit = arrIDsToTry.begin(); cit != arrIDsToTry.end(); ++cit)
            {
                int64_t const& id = *cit;
                enum CFindLinker::SearchResult search_res = SearchSingleRow(flags, id);
                if (search_res == CFindLinker::SEARCH_RESULT_ERROR)
                    return false;
                else if (search_res == CFindLinker::SEARCH_RESULT_FOUND)
                    ++nCountFound;

                if (nCountFound >= nThreshold)
                    break;
            }
        }

        if (nCountFound >= nThreshold || !bDoTrial)
        {
            nCountFound = 0;
            for (/*nRowCount = min(10, nRowCount)*/; (uint64_t)idRow < nRowCount; ++idRow )
            {
                enum CFindLinker::SearchResult search_res = SearchSingleRow(flags, idRow);
                if (search_res == CFindLinker::SEARCH_RESULT_ERROR)
                    return false;
                else if (search_res == CFindLinker::SEARCH_RESULT_FOUND)
                    ++nCountFound;
            }
            std::cout
                << "Run \"" << pszTableName
                << "\": found " << nCountFound << "/" << nRowCount
                << " (" << (int)(100.*nCountFound/(double)nRowCount)  << "%)" << std::endl;

        }
        else
        {
            std::cout
                << "SKIPPED (row count: " << nRowCount << "): Run \"" << pszTableName
                << "\": found " << nCountFound << "/" << arrIDsToTry.size()
                << " (" << (int)(100.*nCountFound/(double)arrIDsToTry.size())  << "%)" << std::endl;
        }

        ReleaseVDB();
        return true;
    }

    /////////////////////////////
    CAgrepMyersSearch::CAgrepMyersSearch(::AgrepFlags flags, char const* pszLinker)
        : m_nLinkerLen (strlen(pszLinker))
    {
        ::AgrepMake(&m_self, flags, pszLinker);
    }
    CAgrepMyersSearch::~CAgrepMyersSearch()
    {
        ::AgrepWhack(m_self);
    }

    void CAgrepMyersSearch::FindBest(char const* pszReadTrimmed, size_t nReadBufSize, int64_t idRow, int32_t nTrimLen, AgrepBestMatch* pBestMatch) const
    {
        // Initializing Agrep structures
        agrepinfo info = {pBestMatch};

        ::AgrepCallArgs args;

        args.self      = m_self;
        args.threshold = (int32_t)(0.33*m_nLinkerLen);
        args.buf       = pszReadTrimmed;
        args.buflen    = min(nTrimLen + m_nLinkerLen, nReadBufSize);
        args.cb        = agrep_callback_find_best;
        args.cbinfo    = (void*)&info;

        // Run the search algorithm
        ::AgrepFindAll(&args);
    }

///////////////////////////////////////

    CVDBCursor::CVDBCursor() : m_pVDBCursor(NULL)
    {
    }
    CVDBCursor::~CVDBCursor()
    {
        Release();
    }
    void CVDBCursor::Release()
    {
        RELEASE_VDB_HANDLE(VCursor, m_pVDBCursor);
    }

    VCursor*& CVDBCursor::PtrRef()
    {
        return m_pVDBCursor;
    }
    VCursor const*& CVDBCursor::PtrConstRef()
    {
        return const_cast<VCursor const*&>(m_pVDBCursor);
    }

    bool CVDBCursor::InitColumnIndex(char const* const* ColumnNames, size_t nCount)
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            rc_t rc = ::VCursorAddColumn(m_pVDBCursor, &m_mapColumnIndex[ColumnNames[i]], ColumnNames[i] );
            PROCESS_RC_WARNING(rc, ColumnNames[i]);
        }
        return true;
    }
    uint32_t CVDBCursor::GetColumnIndex(char const* pszColumnName) const
    {
        MapColumnIndex::const_iterator cit = m_mapColumnIndex.find(pszColumnName);
        assert(cit != m_mapColumnIndex.end());
        return (*cit).second;
    }
}