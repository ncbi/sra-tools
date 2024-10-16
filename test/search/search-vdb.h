// search-vdb.h

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

#ifndef _SEARCH_VDB_H_
#define _SEARCH_VDB_H_

#include <string>
#include <map>
#include <iostream>

#include "../../libs/search/search-priv.h"

#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/printf.h>

#ifndef countof
#define countof(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#define PROCESS_RC_ERROR(rc, descr)\
    {\
        if (rc)\
        {\
            char szBufErr[512] = "";\
            rc_t res = string_printf(szBufErr, countof(szBufErr), NULL, "ERROR: %s failed with error 0x%08x (%d) [%R]", descr, rc, rc, rc);\
            if (res == rcBuffer || res == rcInsufficient)\
                szBufErr[countof(szBufErr) - 1] = '\0';\
            std::cout << szBufErr << std::endl;\
            return false;\
        }\
    }

#define PROCESS_RC_WARNING(rc, descr)\
    {\
        if (rc)\
        {\
            char szBufErr[512] = "";\
            rc_t res = string_printf(szBufErr, countof(szBufErr), NULL, "WARNING: %s failed with error 0x%08x (%d) [%R]", descr, rc, rc, rc);\
            if (res == rcBuffer || res == rcInsufficient)\
                szBufErr[countof(szBufErr) - 1] = '\0';\
            std::cout << szBufErr << std::endl;\
        }\
    }

#if 0
namespace SearchTimeTest
{

#ifdef WIN32
    typedef int time_res_t;
#else
    typedef size_t time_res_t;
#endif

    time_res_t TimeTest(char const* TableName, int enumAlgorithm);
}
#endif

namespace VDBSearchTest
{
    /* remove trailing '\n' from char reads */
    template<typename T> class CPostReadAction
    {
        T* m_pBuf;
        uint32_t m_nCount;
    public:
        CPostReadAction(T* pBuf, uint32_t nCount) : m_pBuf(pBuf), m_nCount(nCount) {}
        void operator()() const;
    };
    template<typename T> inline void CPostReadAction<T>::operator()() const
    {
    }
    template<> inline void CPostReadAction<char>::operator()() const
    {
        m_pBuf[m_nCount] = '\0';
    }
    template<> inline void CPostReadAction<unsigned char>::operator()() const
    {
        m_pBuf[m_nCount] = '\0';
    }

    char const COLUMN_READ[]            = "READ";
    char const COLUMN_READ_2NA_PACKED[] = "(INSDC:2na:packed)READ";
    char const COLUMN_READ_2NA_BIN[]    = "(INSDC:2na:bin)READ";
//    char const COLUMN_READ_4NA_PACKED[] = "(INSDC:4na:packed)READ";
//    char const COLUMN_READ_4NA_BIN[]    = "(INSDC:4na:bin)READ";
    char const COLUMN_LINKER_SEQUENCE[] = "LINKER_SEQUENCE";
    char const COLUMN_READ_START[]      = "READ_START";
    char const COLUMN_READ_LEN[]        = "READ_LEN";
    char const COLUMN_TRIM_START[]      = "TRIM_START";
    char const COLUMN_TRIM_LEN[]        = "TRIM_LEN";

/////////////////////
    struct AgrepBestMatch
    {
        enum {NOT_FOUND = 9999};

        int32_t nScore;
        int32_t indexStart;
        int32_t nMatchLength;

        int32_t nTrimLen;
    };

    class CAgrepMyersSearch // this class is to reuse allocated AgrepParams instead of malloc/free it for every search
    {
    public:
        CAgrepMyersSearch(::AgrepFlags flags, char const* pszLinker);
        CAgrepMyersSearch(CAgrepMyersSearch const& rhs);
        CAgrepMyersSearch& operator=(CAgrepMyersSearch const& rhs);

        ~CAgrepMyersSearch();

        void FindBest(char const* pszReadTrimmed, size_t nReadBufSize, int64_t idRow, int32_t nTrimLen, AgrepBestMatch* pBestMatch) const;
        void FindBest2NAPacked(unsigned char const* p2NAPacked, size_t nReadBufSize, int64_t idRow, int32_t nTrimLen, AgrepBestMatch* pBestMatch) const;
    private:
        ::AgrepParams* m_self;
        size_t m_nLinkerLen;
    };

    class CVDBCursor
    {
    public:

        CVDBCursor();
        CVDBCursor(CVDBCursor const& o);
        CVDBCursor& operator=(CVDBCursor const& o);
        ~CVDBCursor();

        VCursor*& PtrRef();
        VCursor const*& PtrConstRef();

        bool InitColumnIndex(char const* const* ColumnNames, size_t nCount);
        uint32_t GetColumnIndex(char const* pszColumnName) const;

        void Release();

    private:
        typedef std::map<std::string, uint32_t> MapColumnIndex;
        MapColumnIndex m_mapColumnIndex;

        VCursor* m_pVDBCursor;
    };

    class CFindLinker
    {
    public:
#if 0
#ifdef WIN32
        friend int ::SearchTimeTest::TimeTest(char const* TableName, int enumAlgorithm);
#else
        friend size_t ::SearchTimeTest::TimeTest(char const* TableName, int enumAlgorithm);
#endif
#endif

        CFindLinker();
        ~CFindLinker();

        bool Run(::AgrepFlags flags, char const* pszTableName);

    private:
        VDBManager*       m_pVDBManager;
        VTable*           m_pVDBTable;

        CVDBCursor cursorRead;
        CVDBCursor cursorWrite;

        std::string m_sTableName;

        typedef std::map<std::string, uint32_t> MapColumnIndex;
        MapColumnIndex m_mapColumnIndex;

        enum SearchResult
        {
            SEARCH_RESULT_ERROR
            ,SEARCH_RESULT_NOT_FOUND
            ,SEARCH_RESULT_FOUND
        };

        bool InitVDB(char const* pszTableName);
        bool InitVDBRead(char const* pszTableName);
        void ReleaseVDB();
        bool InitColumnIndex();
        bool OpenCursor();
        enum SearchResult SearchSingleRow(::AgrepFlags flags, int64_t idRow);

    public:
        template<typename T> bool ReadItems(int64_t idRow, char const* pszColumnName, T* pBuf, uint32_t nBufLen, uint32_t* pnItemsRead)
        {
            uint32_t idxCol = cursorRead.GetColumnIndex(pszColumnName);

            rc_t rc = ::VCursorReadDirect(cursorRead.PtrConstRef(), idRow, idxCol, 8*sizeof(T), pBuf, nBufLen, pnItemsRead);
            PROCESS_RC_ERROR(rc, "VCursorReadDirect")

            CPostReadAction<T>(pBuf, *pnItemsRead)();

            return true;
        }
    };
}

#endif