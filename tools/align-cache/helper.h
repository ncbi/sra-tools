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

// helper.h
#include <exception>

#include <string.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/printf.h>
#include <klib/vector.h>
#include <kapp/args.h>

#ifndef countof
#define countof(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#include <vector>

#ifdef _WIN32
#pragma warning (disable:4503)
#endif

#define USING_UINT64_BITMAP 0
#define MANAGER_WRITABLE 1

namespace KLib
{
    class CKVector
    {
    public:
        CKVector();
        CKVector (CKVector const& x);
        CKVector& operator= (CKVector const& x);
        ~CKVector();

        void SetBool (uint64_t key, bool value);
        void VisitBool (rc_t ( * f ) ( uint64_t key, bool value, void *user_data ), void *user_data) const;

    private:
        void Make();
        void Release();

        ::KVector* m_pSelf;
    };
}

namespace VDBObjects
{
    class CErrorMsg : public std::exception // TODO: move it out of VDBObjects
    {
    public:
        CErrorMsg(rc_t rc, char const* temp_str) // TODO: add constructor with fmt, ...
            : m_rc(rc)
        {
            strncpy ( m_szDescr, temp_str, countof(m_szDescr) );
        }

        rc_t getRC() const { return m_rc; }
        virtual char const* what() const throw() { return m_szDescr; }

    private:
        char m_szDescr[256];
        rc_t m_rc;
    };

/////////////////////////////////////////////////////////

    /* functor to remove trailing '\n' from char reads */
    template<typename T> class CPostReadAction
    {
        T* m_pBuf;
        uint32_t m_nCount;
    public:
        CPostReadAction(T* pBuf, uint32_t nCount) : m_pBuf(pBuf), m_nCount(nCount) {}
        void operator()() const;
    };
    template<typename T> inline void CPostReadAction<T>::operator()() const {}
    template<> inline void CPostReadAction<char>::operator()() const { m_pBuf[m_nCount] = '\0'; }
    template<> inline void CPostReadAction<unsigned char>::operator()() const { m_pBuf[m_nCount] = '\0'; }

    class CVCursor;
    class CVTable;
    class CVDatabase;
    class CVSchema;

/////////////////////////////////////////////////////////////////////////////////

    class CVDBManager
    {
    public:
        CVDBManager();
        ~CVDBManager();
        CVDBManager(CVDBManager const& x);
        CVDBManager& operator=(CVDBManager const& x);

        void Make();
        void Release();
        CVDatabase OpenDB ( char const* pszDBName ) const;
        CVDatabase CreateDB ( CVSchema const& schema, char const* pszTypeDesc, ::KCreateMode cmode, char const* pszPath );
        CVSchema MakeSchema () const;

    private:
        ::VDBManager* m_pSelf;
    };

/////////////////////////////////////////////////////////////////////////////////

    class CVSchema
    {
    public:
        friend CVSchema CVDBManager::MakeSchema () const;
        friend CVDatabase CVDBManager::CreateDB ( CVSchema const& schema, char const* pszTypeDesc, ::KCreateMode cmode, char const* pszPath );

        CVSchema();
        ~CVSchema();
        CVSchema(CVSchema const& x);
        CVSchema& operator=(CVSchema const& x);

        void Make();
        void Release();
        void VSchemaParseFile(char const* pszFilePath);

    private:
        void Clone ( CVSchema const& x );
        ::VSchema* m_pSelf;
    };

/////////////////////////////////////////////////////////////////////////////////

    class CVDatabase
    {
    public:
        friend CVDatabase CVDBManager::OpenDB ( char const* pszDBName ) const;
        friend CVDatabase CVDBManager::CreateDB ( CVSchema const& schema, char const* pszTypeDesc, ::KCreateMode cmode, char const* pszPath );

        CVDatabase();
        ~CVDatabase();
        CVDatabase(CVDatabase const& x);
        CVDatabase& operator=(CVDatabase const& x);

        void Release();
        CVTable OpenTable ( char const* pszTableName ) const;
        CVTable CreateTable ( char const* pszTableName, ::KCreateMode cmode );

    private:
        void Clone(CVDatabase const& x);
        ::VDatabase* m_pSelf;
    };

//////////////////////////////////////////////////////////////

    class CVTable
    {
    public:
        friend CVTable CVDatabase::OpenTable(char const* pszTableName) const;
        friend CVTable CVDatabase::CreateTable ( char const* pszTableName, ::KCreateMode cmode );

        CVTable();
        ~CVTable();
        CVTable(CVTable const& x);
        CVTable& operator=(CVTable const& x);

        void Release();
        CVCursor CreateCursorRead ( size_t cache_size ) const;
        CVCursor CreateCursorWrite ( ::KCreateMode mode );

    private:
        void Clone(CVTable const& x);
        ::VTable* m_pSelf;
    };

////////////////////////////////////////////////////////////////////////////

    class CVCursor
    {
    public:
        friend CVCursor CVTable::CreateCursorRead ( size_t cache_size ) const;
        friend CVCursor CVTable::CreateCursorWrite (::KCreateMode mode);

        CVCursor();
        ~CVCursor();
        CVCursor(CVCursor const& x);
        CVCursor& operator=(CVCursor const& x);

        void Release();
        void InitColumnIndex(char const* const* ColumnNames, uint32_t* pColumnIndex, size_t nCount, bool set_default);
        void Open() const;
        void GetIdRange(int64_t& idFirstRow, uint64_t& nRowCount) const;

        template <typename T> uint32_t ReadItems (int64_t idRow, uint32_t idxCol, T* pBuf, uint32_t nBufLen) const
        {
            uint32_t nItemsRead = 0;

            rc_t rc = ::VCursorReadDirect(m_pSelf, idRow, idxCol, 8*sizeof(T), pBuf, nBufLen, &nItemsRead);
            if (rc)
                throw CErrorMsg(rc, "VCursorReadDirect");

            //CPostReadAction<T>(pBuf, nItemsRead)();

            return nItemsRead;
        }

        template <typename T> void Write (uint32_t idxCol, T const* pBuf, uint64_t count)
        {
            rc_t rc = ::VCursorWrite ( m_pSelf, idxCol, 8 * sizeof(T), pBuf, 0, count );
            if (rc)
                throw CErrorMsg(rc, "VCursorWrite");
        }

        int64_t GetRowId () const;
        void SetRowId (int64_t row_id) const;
        void OpenRow () const;
        void CommitRow ();
        void RepeatRow ( uint64_t count );
        void CloseRow () const;
        void Commit ();

    private:
        void Clone(CVCursor const& x);
        ::VCursor* m_pSelf;
    };
}

///////////////////////

namespace KApp
{
    template <typename T> T atoi_t ( char const* str_val ) // TODO: throw std::runtime_exception instead of CErrorMsg
    {
        if ( str_val [0] == '\0' )
            throw VDBObjects::CErrorMsg(0, "atoi_t: invalid input string (empty)");

        size_t i = 0;
        char sign = '+';
        if ( str_val[0] == '-' || str_val[0] == '+' )
        {
            ++i;
            sign = str_val[0];
        }

        T ret = 0;
        for (; str_val[i] != '\0'; ++i )
        {
            if ( str_val[i] < '0' || str_val[i] > '9' )
                throw VDBObjects::CErrorMsg(0, "atoi_t: invalid input string (invalid character)");
            ret = ret*10 + str_val[i] - '0';
        }

        return sign == '-' ? -ret : ret;
    }

    template <typename T> T atou_t ( char const* str_val ) // TODO: throw std::runtime_exception instead of CErrorMsg
    {
        if ( str_val [0] == '\0' )
            throw VDBObjects::CErrorMsg(0, "atou_t: invalid input string (empty)");

        T ret = 0;
        for ( size_t i = 0; str_val[i] != '\0'; ++i )
        {
            if ( str_val[i] < '0' || str_val[i] > '9' )
                throw VDBObjects::CErrorMsg(0, "atou_t: invalid input string (invalid character)");
            ret = ret*10 + str_val[i] - '0';
        }

        return ret;
    }

    class CArgs
    {
    public:
        CArgs ( int argc, char** argv, ::OptDef* pOptions, size_t option_count );
        CArgs ( CArgs const& x );
        CArgs& operator= ( CArgs const& x );
        ~CArgs ();

        ::Args const* GetArgs () const;
        uint32_t GetParamCount () const;
        char const* GetParamValue ( uint32_t iteration ) const;
        uint32_t GetOptionCount ( char const* option_name ) const;
        char const* GetOptionValue ( char const* option_name, uint32_t iteration ) const;

        template <typename T> T GetOptionValueInt ( char const* option_name, uint32_t iteration ) const
        {
            char const* str_val = GetOptionValue ( option_name, iteration );
            return atoi_t <T> ( str_val );
        }
        template <typename T> T GetOptionValueUInt ( char const* option_name, uint32_t iteration ) const
        {
            char const* str_val = GetOptionValue ( option_name, iteration );
            return atou_t <T> ( str_val );
        }

    private:

        void MakeAndHandle ( int argc, char** argv, ::OptDef* pOptions, size_t option_count );
        void Release ();

        ::Args* m_pSelf;
    };
}
