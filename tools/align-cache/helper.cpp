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

// helper.cpp

#include "helper.h"

#include <algorithm>
#include <stdio.h>
#include <iostream>

#include <vdb/vdb-priv.h>
#include <klib/rc.h>

#ifdef _WIN32
#pragma warning (disable:4503)
#endif

// TODO: remove printfs
namespace KLib
{
    CKVector::CKVector() : m_pSelf(NULL)
    {
        Make();
    }

    CKVector::~CKVector()
    {
        Release();
    }

    void CKVector::Make()
    {
        if (m_pSelf)
            throw VDBObjects::CErrorMsg (0, "Duplicated call to KVectorMake");

        rc_t rc = ::KVectorMake(&m_pSelf);
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "KVectorMake");
        printf("Created KVector %p\n", m_pSelf);
    }

    void CKVector::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing KVector %p\n", m_pSelf);
            ::KVectorRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    size_t const RECORD_SIZE_IN_BITS = 2;
    uint64_t const BIT_SET_MASK = 0x2;
    uint64_t const BIT_VALUE_MASK = 0x1;
    uint64_t const BIT_RECORD_MASK = BIT_SET_MASK | BIT_VALUE_MASK;

    void CKVector::SetBool(uint64_t key, bool value)
    {
#if USING_UINT64_BITMAP == 1
        uint64_t stored_bits = 0;
        uint64_t key_qword = key / 64;
        uint64_t key_bit = key % 64;
        rc_t rc = ::KVectorGetU64 ( m_pSelf, key_qword, &stored_bits );
        bool first_time = rc == RC ( rcCont, rcVector, rcAccessing, rcItem, rcNotFound ); // 0x1e615458
        if ( !first_time && rc )
            throw VDBObjects::CErrorMsg(rc, "KVectorGetU64");

        uint64_t new_bit = (uint64_t)value << key_bit;
        uint64_t stored_bit = (uint64_t)1 << key_bit & stored_bits;

        if ( first_time || new_bit != stored_bit )
        {
            if ( new_bit )
                stored_bits |= new_bit;
            else
                stored_bits &= ~new_bit;

            rc_t rc = ::KVectorSetU64 ( m_pSelf, key_qword, stored_bits );
            if (rc)
                throw VDBObjects::CErrorMsg(rc, "KVectorSetU64");
        }
#elif USING_UINT64_BITMAP == 2
        uint64_t stored_bits = 0;
        uint64_t key_qword = key / (sizeof(stored_bits) * 8 / RECORD_SIZE_IN_BITS);
        uint64_t bit_offset_in_qword = (key % (sizeof(stored_bits) * 8 / RECORD_SIZE_IN_BITS)) * RECORD_SIZE_IN_BITS;
        rc_t rc = ::KVectorGetU64 ( m_pSelf, key_qword, &stored_bits );
        bool first_time = rc == RC ( rcCont, rcVector, rcAccessing, rcItem, rcNotFound ); // 0x1e615458;
        if ( !first_time && rc )
            throw VDBObjects::CErrorMsg(rc, "KVectorGetU64");

        uint64_t new_bit_record = (BIT_SET_MASK | (uint64_t)value) << bit_offset_in_qword;
        uint64_t stored_bit_record = (uint64_t)BIT_RECORD_MASK << bit_offset_in_qword & stored_bits;

        if ( first_time || new_bit_record != stored_bit_record )
        {
            stored_bits &= ~((uint64_t)BIT_RECORD_MASK << bit_offset_in_qword); // clear stored record to assign a new value by bitwise OR
            stored_bits |= new_bit_record;

            rc_t rc = ::KVectorSetU64 ( m_pSelf, key_qword, stored_bits );
            if (rc)
                throw VDBObjects::CErrorMsg(rc, "KVectorSetU64");
        }
#else

        rc_t rc = ::KVectorSetBool ( m_pSelf, key, value );
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "KVectorSetBool");
#endif
    }

    struct UserDataU64toBool
    {
        rc_t ( * f ) ( uint64_t key, bool value, void *user_data );
        void* user_data;
    };
#if USING_UINT64_BITMAP == 1
    rc_t VisitU64toBoolAdapter ( uint64_t key, uint64_t value, void *user_data )
    {
        rc_t ( * bool_callback ) ( uint64_t key, bool value, void *user_data );
        bool_callback = ((UserDataU64toBool*) user_data) -> f;
        void* original_user_data = ((UserDataU64toBool*) user_data) -> user_data;

        rc_t rc = 0;
        for ( size_t i = 0; i < sizeof (value) * 8; ++i )
        {
            rc = bool_callback ( key * 64 + i, (bool) ((uint64_t)1 << i & value), original_user_data );
            if ( rc )
                return rc;
        }
        return rc;
    }
#elif USING_UINT64_BITMAP == 2
    rc_t VisitU64toBoolAdapter ( uint64_t key, uint64_t value, void *user_data )
    {
        rc_t ( * bool_callback ) ( uint64_t key, bool value, void *user_data );
        bool_callback = ((UserDataU64toBool*) user_data) -> f;
        void* original_user_data = ((UserDataU64toBool*) user_data) -> user_data;

        rc_t rc = 0;
        for ( size_t i = 0; i < sizeof (value) * 8 / RECORD_SIZE_IN_BITS; ++i )
        {
            uint64_t key_bool = key * sizeof(value) * 8 / RECORD_SIZE_IN_BITS + i;
            uint64_t record = value >> i * RECORD_SIZE_IN_BITS & BIT_RECORD_MASK;
            if ( record & BIT_SET_MASK )
            {
                rc = bool_callback ( key_bool, (bool) (record & BIT_VALUE_MASK), original_user_data );
                if ( rc )
                    return rc;
            }
        }
        return rc;
    }
#endif

    void CKVector::VisitBool(rc_t ( * f ) ( uint64_t key, bool value, void *user_data ), void *user_data) const
    {
#if USING_UINT64_BITMAP == 1
        UserDataU64toBool user_data_adapter = { f, user_data };
        ::KVectorVisitU64 ( m_pSelf, false, VisitU64toBoolAdapter, &user_data_adapter );
#elif USING_UINT64_BITMAP == 2
        UserDataU64toBool user_data_adapter = { f, user_data };
        ::KVectorVisitU64 ( m_pSelf, false, VisitU64toBoolAdapter, &user_data_adapter );
#else
        ::KVectorVisitBool ( m_pSelf, false, f, user_data );
#endif
    }
}

///////////////////////////////////////////////////////////////

namespace VDBObjects
{
    CVCursor::CVCursor() : m_pSelf(NULL)
    {}

    CVCursor::~CVCursor()
    {
        Release();
    }

    CVCursor::CVCursor(CVCursor const& x)
    {
        Clone(x);
    }

    CVCursor& CVCursor::operator=(CVCursor const& x)
    {
        Clone(x);
        return *this;
    }

    void CVCursor::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing VCursor %p\n", m_pSelf);
            ::VCursorRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVCursor::Clone(CVCursor const& x)
    {
        if (false && m_pSelf)
        {
            assert(0);
            Release();
        }
        m_pSelf = x.m_pSelf;
        ::VCursorAddRef ( m_pSelf );
        printf ("CLONING VCursor %p\n", m_pSelf);
    }

    void CVCursor::Open() const
    {
        rc_t rc = ::VCursorOpen(m_pSelf);
        if (rc)
            throw CErrorMsg(rc, "VCursorOpen");
    }

    void CVCursor::InitColumnIndex(char const* const* ColumnNames, uint32_t* pColumnIndex, size_t nCount, bool set_default)
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            rc_t rc = ::VCursorAddColumn(m_pSelf, & pColumnIndex[i], ColumnNames[i] );
            if (rc)
            {
                char szDescr[256];
                size_t num_written = 0;
                string_printf(szDescr, countof(szDescr), &num_written, "VCursorAddColumn - [%s]", ColumnNames[i]);
                throw CErrorMsg(rc, szDescr);
            }
            if ( set_default )
            {
                VTypedecl type;
                VTypedesc desc;
                uint32_t idx = pColumnIndex[i];

                rc = ::VCursorDatatype ( m_pSelf, idx, & type, & desc );
                if (rc)
                    throw CErrorMsg(rc, "VCursorDatatype");

                uint32_t elem_bits = ::VTypedescSizeof ( & desc );
                if (rc)
                    throw CErrorMsg(rc, "VTypedescSizeof");
                rc = ::VCursorDefault ( m_pSelf, idx, elem_bits, "", 0, 0 );
                if (rc)
                    throw CErrorMsg(rc, "VCursorDefault");
            }
        }
    }

    void CVCursor::GetIdRange(int64_t& idFirstRow, uint64_t& nRowCount) const
    {
        rc_t rc = ::VCursorIdRange(m_pSelf, 0, &idFirstRow, &nRowCount);
        if (rc)
            throw CErrorMsg(rc, "VCursorIdRange");
    }

    int64_t CVCursor::GetRowId () const
    {
        int64_t row_id;
        rc_t rc = ::VCursorRowId ( m_pSelf, & row_id );
        if (rc)
            throw CErrorMsg(rc, "VCursorRowId");

        return row_id;
    }

    void CVCursor::SetRowId (int64_t row_id) const
    {
        rc_t rc = ::VCursorSetRowId ( m_pSelf, row_id );
        if (rc)
            throw CErrorMsg(rc, "VCursorSetRowId");
    }

    void CVCursor::OpenRow () const
    {
        rc_t rc = ::VCursorOpenRow ( m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VCursorOpenRow");
    }

    void CVCursor::CommitRow ()
    {
        rc_t rc = ::VCursorCommitRow ( m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VCursorCommitRow");
    }

    void CVCursor::RepeatRow ( uint64_t count )
    {
        rc_t rc = ::VCursorRepeatRow ( m_pSelf, count );
        if (rc)
            throw CErrorMsg(rc, "VCursorRepeatRow");
    }

    void CVCursor::CloseRow () const
    {
        rc_t rc = ::VCursorCloseRow ( m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VCursorCloseRow");
    }

    void CVCursor::Commit ()
    {
        rc_t rc = ::VCursorCommit ( m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VCursorCommit");
    }

///////////////////////////////////////////////////////////////////////////////////

    CVTable::CVTable() : m_pSelf(NULL)
    {
    }
    
    CVTable::~CVTable()
    {
        Release();
    }

    CVTable::CVTable(CVTable const& x)
    {
        Clone(x);
    }

    CVTable& CVTable::operator=(CVTable const& x)
    {
        Clone(x);
        return *this;
    }

    void CVTable::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing VTable %p\n", m_pSelf);
            ::VTableRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVTable::Clone(CVTable const& x)
    {
        if (false && m_pSelf)
        {
            assert(0);
            Release();
        }
        m_pSelf = x.m_pSelf;
        ::VTableAddRef ( m_pSelf );
        printf ("CLONING VTable %p\n", m_pSelf);
    }

    CVCursor CVTable::CreateCursorRead ( size_t cache_size ) const
    {
        CVCursor cursor;
        rc_t rc = ::VTableCreateCachedCursorRead(m_pSelf, const_cast<VCursor const**>(& cursor.m_pSelf), cache_size);
        if (rc)
            throw CErrorMsg(rc, "VTableCreateCachedCursorRead");

        printf("Created cursor (rd) %p\n", cursor.m_pSelf);
        return cursor;
    }

    CVCursor CVTable::CreateCursorWrite (::KCreateMode mode)
    {
        CVCursor cursor;
        rc_t rc = ::VTableCreateCursorWrite ( m_pSelf, & cursor.m_pSelf, mode );
        if (rc)
            throw CErrorMsg(rc, "VTableCreateCursorWrite");

        printf("Created cursor (wr) %p\n", cursor.m_pSelf);
        return cursor;
    }

//////////////////////////////////////////////////////////////////////

    CVDatabase::CVDatabase() : m_pSelf(NULL)
    {}

    CVDatabase::~CVDatabase()
    {
        Release();
    }

    CVDatabase::CVDatabase(CVDatabase const& x)
    {
        Clone(x);
    }

    CVDatabase& CVDatabase::operator=(CVDatabase const& x)
    {
        Clone(x);
        return *this;
    }

    void CVDatabase::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing VDatabase %p\n", m_pSelf);
            ::VDatabaseRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVDatabase::Clone(CVDatabase const& x)
    {
        if (false && m_pSelf)
        {
            assert(0);
            Release();
        }
        m_pSelf = x.m_pSelf;
        ::VDatabaseAddRef ( m_pSelf );
        printf ("CLONING VDatabase %p\n", m_pSelf);
    }

    CVTable CVDatabase::OpenTable(char const* pszTableName) const
    {
        CVTable table;
        rc_t rc = ::VDatabaseOpenTableRead(m_pSelf, const_cast<VTable const**>(& table.m_pSelf), pszTableName);
        if (rc)
            throw CErrorMsg(rc, "VDatabaseOpenTableRead");

        printf("Opened table %p\n", table.m_pSelf);
        return table;
    }

    CVTable CVDatabase::CreateTable ( char const* pszTableName, ::KCreateMode cmode )
    {
        CVTable table;
        rc_t rc = ::VDatabaseCreateTable ( m_pSelf, & table.m_pSelf, pszTableName, cmode, pszTableName );
        if (rc)
            throw CErrorMsg(rc, "VDatabaseCreateTable");

        printf("Created table %p\n", table.m_pSelf);
        return table;
    }

//////////////////////////////////////////////////////////////////////

    CVSchema::CVSchema() : m_pSelf (NULL)
    {
    }
    CVSchema::~CVSchema()
    {
        Release();
    }

    CVSchema::CVSchema(CVSchema const& x)
    {
        Clone (x);
    }

    CVSchema& CVSchema::operator=(CVSchema const& x)
    {
        Clone (x);
        return *this;
    }

    void CVSchema::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing VSchema %p\n", m_pSelf);
            ::VSchemaRelease ( m_pSelf );
            m_pSelf = NULL;
        }
    }

    void CVSchema::Clone ( CVSchema const& x )
    {
        if (false && m_pSelf)
        {
            assert(0);
            Release();
        }
        m_pSelf = x.m_pSelf;
        ::VSchemaAddRef ( m_pSelf );
        printf ("CLONING VSchema %p\n", m_pSelf);
    }
    
    void CVSchema::VSchemaParseFile ( char const* pszFilePath )
    {
        rc_t rc = ::VSchemaParseFile ( m_pSelf, pszFilePath );
        if (rc)
            throw CErrorMsg(rc, "VSchemaParseFile");
    }

//////////////////////////////////////////////////////////////////////

    CVDBManager::CVDBManager() : m_pSelf(NULL)
    {}

    CVDBManager::~CVDBManager()
    {
        Release();
    }

    void CVDBManager::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing VDBManager %p\n", m_pSelf);
            ::VDBManagerRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

#if MANGER_WRITABLE != 0
    void CVDBManager::Make()
    {
        assert(m_pSelf == NULL);
        if (m_pSelf)
            throw CErrorMsg(0, "Double call to VDBManagerMakeRead");

        rc_t rc = ::VDBManagerMakeRead(const_cast<VDBManager const**>(&m_pSelf), NULL);
        if (rc)
            throw CErrorMsg(rc, "VDBManagerMakeRead");

        printf("Created VDBManager (rd) %p\n", m_pSelf);
    }
#else
    void CVDBManager::Make()
    {
        assert(m_pSelf == NULL);
        if (m_pSelf)
            throw CErrorMsg(0, "Double call to VDBManagerMakeUpdate");

        rc_t rc = ::VDBManagerMakeUpdate ( & m_pSelf, NULL );
        if (rc)
            throw CErrorMsg(rc, "VDBManagerMakeUpdate");

	    /*rc = VDBManagerDisablePagemapThread ( m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VDBManagerDisablePagemapThread");*/

        printf("Created VDBManager (wr) %p\n", m_pSelf);
    }
#endif

    CVDatabase CVDBManager::OpenDB(char const* pszDBName) const
    {
        CVDatabase vdb;
        rc_t rc = ::VDBManagerOpenDBRead(m_pSelf, const_cast<VDatabase const**>(& vdb.m_pSelf), NULL, pszDBName);
        if (rc)
            throw CErrorMsg(rc, "VDBManagerOpenDBRead");

        printf("Opened database %p\n", vdb.m_pSelf);
        return vdb;
    }
    CVDatabase CVDBManager::CreateDB ( CVSchema const& schema, char const* pszTypeDesc, ::KCreateMode cmode, char const* pszPath )
    {
        CVDatabase vdb;
        rc_t rc = ::VDBManagerCreateDB ( m_pSelf, & vdb.m_pSelf, schema.m_pSelf, pszTypeDesc, cmode, pszPath );
        if (rc)
            throw CErrorMsg(rc, "VDBManagerCreateDB");

        printf("Created database %p\n", vdb.m_pSelf);
        return vdb;
    }

    CVSchema CVDBManager::MakeSchema () const
    {
        CVSchema schema;
        rc_t rc = ::VDBManagerMakeSchema ( m_pSelf, & schema.m_pSelf );
        if (rc)
            throw CErrorMsg(rc, "VDBManagerMakeSchema");

        printf("Created Schema %p\n", schema.m_pSelf);
        return schema;
    }
}

namespace KApp
{
    CArgs::CArgs (int argc, char** argv, ::OptDef* pOptions, size_t option_count)
        : m_pSelf(NULL)
    {
        MakeAndHandle ( argc, argv, pOptions, option_count );
    }

    CArgs::~CArgs()
    {
        Release();
    }

    void CArgs::MakeAndHandle (int argc, char** argv, ::OptDef* pOptions, size_t option_count)
    {
        if (m_pSelf)
            throw VDBObjects::CErrorMsg (0, "Duplicated call to ArgsMakeAndHandle");

        rc_t rc = ::ArgsMakeAndHandle (&m_pSelf, argc, argv, 1, pOptions, option_count);
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "ArgsMakeAndHandle");
        printf("Created Args %p\n", m_pSelf);
    }

    void CArgs::Release()
    {
        if (m_pSelf)
        {
            printf("Releasing Args %p\n", m_pSelf);
            ::ArgsRelease (m_pSelf);
            m_pSelf = NULL;
        }
    }

    ::Args const* CArgs::GetArgs () const
    {
        return m_pSelf;
    }

    uint32_t CArgs::GetParamCount () const
    {
        uint32_t ret = 0;
        rc_t rc = ::ArgsParamCount ( m_pSelf, &ret );
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "ArgsParamCount");

        return ret;
    }

    char const* CArgs::GetParamValue ( uint32_t iteration ) const
    {
        char const* ret = NULL;
        rc_t rc = ::ArgsParamValue ( m_pSelf, iteration, &ret );
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "ArgsParamValue");

        return ret;
    }

    uint32_t CArgs::GetOptionCount ( char const* option_name ) const
    {
        uint32_t ret = 0;
        rc_t rc = ::ArgsOptionCount ( m_pSelf, option_name, &ret );
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "ArgsOptionCount");

        return ret;
    }

    char const* CArgs::GetOptionValue ( char const* option_name, uint32_t iteration ) const
    {
        char const* ret = NULL;
        rc_t rc = ::ArgsOptionValue ( m_pSelf, option_name, iteration, &ret );
        if (rc)
            throw VDBObjects::CErrorMsg(rc, "ArgsOptionValue");

        return ret;
    }

}