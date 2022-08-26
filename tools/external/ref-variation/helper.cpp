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
#if DEBUG_PRINT != 0
#include <stdio.h>
#endif

//#include <vdb/vdb-priv.h>
#include <klib/rc.h>

#include <kdb/table.h>
#include <kproc/thread.h>
#include <kproc/lock.h>

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
            throw Utils::CErrorMsg (0, "Duplicated call to KVectorMake");

        rc_t rc = ::KVectorMake(&m_pSelf);
        if (rc)
            throw Utils::CErrorMsg(rc, "KVectorMake");
#if DEBUG_PRINT != 0
        printf("Created KVector %p\n", m_pSelf);
#endif
    }

    void CKVector::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing KVector %p\n", m_pSelf);
#endif
            ::KVectorRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

#if USING_UINT64_BITMAP == 2
    size_t const RECORD_SIZE_IN_BITS = 2;
    uint64_t const BIT_SET_MASK = 0x2;
    uint64_t const BIT_VALUE_MASK = 0x1;
    uint64_t const BIT_RECORD_MASK = BIT_SET_MASK | BIT_VALUE_MASK;
#endif

    void CKVector::SetBool(uint64_t key, bool value)
    {
#if USING_UINT64_BITMAP == 1
        uint64_t stored_bits = 0;
        uint64_t key_qword = key / 64;
        uint64_t key_bit = key % 64;
        rc_t rc = ::KVectorGetU64 ( m_pSelf, key_qword, &stored_bits );
        bool first_time = rc == RC ( rcCont, rcVector, rcAccessing, rcItem, rcNotFound ); // 0x1e615458
        if ( !first_time && rc )
            throw Utils::CErrorMsg(rc, "KVectorGetU64");

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
                throw Utils::CErrorMsg(rc, "KVectorSetU64");
        }
#elif USING_UINT64_BITMAP == 2
        uint64_t stored_bits = 0;
        uint64_t key_qword = key / (sizeof(stored_bits) * 8 / RECORD_SIZE_IN_BITS);
        uint64_t bit_offset_in_qword = (key % (sizeof(stored_bits) * 8 / RECORD_SIZE_IN_BITS)) * RECORD_SIZE_IN_BITS;
        rc_t rc = ::KVectorGetU64 ( m_pSelf, key_qword, &stored_bits );
        bool first_time = rc == RC ( rcCont, rcVector, rcAccessing, rcItem, rcNotFound ); // 0x1e615458;
        if ( !first_time && rc )
            throw Utils::CErrorMsg(rc, "KVectorGetU64");

        uint64_t new_bit_record = (BIT_SET_MASK | (uint64_t)value) << bit_offset_in_qword;
        uint64_t stored_bit_record = (uint64_t)BIT_RECORD_MASK << bit_offset_in_qword & stored_bits;

        if ( first_time || new_bit_record != stored_bit_record )
        {
            stored_bits &= ~((uint64_t)BIT_RECORD_MASK << bit_offset_in_qword); // clear stored record to assign a new value by bitwise OR
            stored_bits |= new_bit_record;

            rc_t rc = ::KVectorSetU64 ( m_pSelf, key_qword, stored_bits );
            if (rc)
                throw Utils::CErrorMsg(rc, "KVectorSetU64");
        }
#else

        rc_t rc = ::KVectorSetBool ( m_pSelf, key, value );
        if (rc)
            throw Utils::CErrorMsg(rc, "KVectorSetBool");
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
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CVCursor::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing VCursor %p\n", m_pSelf);
#endif
            ::VCursorRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVCursor::Clone(CVCursor const& x)
    {
        m_pSelf = x.m_pSelf;
        ::VCursorAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING VCursor %p\n", m_pSelf);
#endif
    }

    void CVCursor::Open() const
    {
        rc_t rc = ::VCursorOpen(m_pSelf);
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorOpen");
    }

    void CVCursor::PermitPostOpenAdd() const
    {
        rc_t rc = ::VCursorPermitPostOpenAdd ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorPermitPostOpenAdd");
    }
#if MANAGER_WRITABLE != 0
    void CVCursor::InitColumnIndex(char const* const* ColumnNames, uint32_t* pColumnIndex, size_t nCount, bool set_default)
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            rc_t rc = ::VCursorAddColumn(m_pSelf, & pColumnIndex[i], ColumnNames[i] );
            if (rc)
                throw Utils::CErrorMsg(rc, "VCursorAddColumn - [%s]", ColumnNames[i]);

            if ( set_default )
            {
                VTypedecl type;
                VTypedesc desc;
                uint32_t idx = pColumnIndex[i];

                rc = ::VCursorDatatype ( m_pSelf, idx, & type, & desc );
                if (rc)
                    throw Utils::CErrorMsg(rc, "VCursorDatatype (column idx=%u [%s])", idx, ColumnNames[i]);

                uint32_t elem_bits = ::VTypedescSizeof ( & desc );
                if (rc)
                    throw Utils::CErrorMsg(rc, "VTypedescSizeof (column idx=%u [%s])", idx, ColumnNames[i]);
                rc = ::VCursorDefault ( m_pSelf, idx, elem_bits, "", 0, 0 );
                if (rc)
                    throw Utils::CErrorMsg(rc, "VCursorDefault (column idx=%u [%s])", idx, ColumnNames[i]);
            }
        }
    }
#else
    void CVCursor::InitColumnIndex(char const* const* ColumnNames, uint32_t* pColumnIndex, size_t nCount)
    {
        for (size_t i = 0; i < nCount; ++i)
        {
            rc_t rc = ::VCursorAddColumn(m_pSelf, & pColumnIndex[i], ColumnNames[i] );
            if (rc)
                throw Utils::CErrorMsg(rc, "VCursorAddColumn - [%s]", ColumnNames[i]);
        }
    }
#endif

    void CVCursor::GetIdRange(int64_t& idFirstRow, uint64_t& nRowCount) const
    {
        rc_t rc = ::VCursorIdRange(m_pSelf, 0, &idFirstRow, &nRowCount);
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorIdRange");
    }

    int64_t CVCursor::GetRowId () const
    {
        int64_t row_id;
        rc_t rc = ::VCursorRowId ( m_pSelf, & row_id );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorRowId");

        return row_id;
    }

    void CVCursor::SetRowId (int64_t row_id) const
    {
        rc_t rc = ::VCursorSetRowId ( m_pSelf, row_id );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorSetRowId (%ld)", row_id);
    }

    void CVCursor::OpenRow () const
    {
        rc_t rc = ::VCursorOpenRow ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorOpenRow");
    }

#if MANAGER_WRITABLE != 0
    void CVCursor::CommitRow ()
    {
        rc_t rc = ::VCursorCommitRow ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorCommitRow");
    }

    void CVCursor::RepeatRow ( uint64_t count )
    {
        rc_t rc = ::VCursorRepeatRow ( m_pSelf, count );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorRepeatRow (%lu)", count);
    }
    void CVCursor::Commit ()
    {
        rc_t rc = ::VCursorCommit ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorCommit");
    }
#endif
    void CVCursor::CloseRow () const
    {
        rc_t rc = ::VCursorCloseRow ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VCursorCloseRow");
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
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CVTable::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing VTable %p\n", m_pSelf);
#endif
            ::VTableRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVTable::Clone(CVTable const& x)
    {
        m_pSelf = x.m_pSelf;
        ::VTableAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING VTable %p\n", m_pSelf);
#endif
    }

    CVCursor CVTable::CreateCursorRead ( ) const
    {
        CVCursor cursor;
        rc_t rc = ::VTableCreateCursorRead(m_pSelf, const_cast<VCursor const**>(& cursor.m_pSelf));
        if (rc)
            throw Utils::CErrorMsg(rc, "VTableCreateCursorRead");

#if DEBUG_PRINT != 0
        printf("Created cursor (rd) %p\n", cursor.m_pSelf);
#endif
        return cursor;
    }

    CVCursor CVTable::CreateCursorRead ( size_t cache_size ) const
    {
        CVCursor cursor;
        rc_t rc = ::VTableCreateCachedCursorRead(m_pSelf, const_cast<VCursor const**>(& cursor.m_pSelf), cache_size);
        if (rc)
            throw Utils::CErrorMsg(rc, "VTableCreateCachedCursorRead (%zu)", cache_size);

#if DEBUG_PRINT != 0
        printf("Created cached cursor (rd) %p\n", cursor.m_pSelf);
#endif
        return cursor;
    }

#if MANAGER_WRITABLE != 0
    CVCursor CVTable::CreateCursorWrite (::KCreateMode mode)
    {
        CVCursor cursor;
        rc_t rc = ::VTableCreateCursorWrite ( m_pSelf, & cursor.m_pSelf, mode );
        if (rc)
            throw Utils::CErrorMsg(rc, "VTableCreateCursorWrite");

#if DEBUG_PRINT != 0
        printf("Created cursor (wr) %p\n", cursor.m_pSelf);
#endif
        return cursor;
    }
#endif

    KDBObjects::CKTable CVTable::OpenKTableRead () const
    {
        KDBObjects::CKTable obj;
        rc_t rc = ::VTableOpenKTableRead(m_pSelf, const_cast<KTable const**>(& obj.m_pSelf));
        if (rc)
            throw Utils::CErrorMsg(rc, "VTableOpenKTableRead");

#if DEBUG_PRINT != 0
        printf("Opened KTable %p\n", obj.m_pSelf);
#endif
        return obj;
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
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CVDatabase::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing VDatabase %p\n", m_pSelf);
#endif
            ::VDatabaseRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CVDatabase::Clone(CVDatabase const& x)
    {
        m_pSelf = x.m_pSelf;
        ::VDatabaseAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING VDatabase %p\n", m_pSelf);
#endif
    }

    CVTable CVDatabase::OpenTable(char const* pszTableName) const
    {
        CVTable table;
        rc_t rc = ::VDatabaseOpenTableRead(m_pSelf, const_cast<VTable const**>(& table.m_pSelf), pszTableName);
        if (rc)
            throw Utils::CErrorMsg(rc, "VDatabaseOpenTableRead (%s)", pszTableName);

#if DEBUG_PRINT != 0
        printf("Opened table %p (%s)\n", table.m_pSelf, pszTableName);
#endif
        return table;
    }

#if MANAGER_WRITABLE != 0
    CVTable CVDatabase::CreateTable ( char const* pszTableName )
    {
        CVTable table;
        //rc_t rc = ::VDatabaseCreateTableDefault ( m_pSelf, & table.m_pSelf, pszTableName, pszTableName );
        rc_t rc = ::VDatabaseCreateTableByMask ( m_pSelf, & table.m_pSelf, pszTableName, 0, 0, pszTableName );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDatabaseCreateTableDefault (%s)", pszTableName);

#if DEBUG_PRINT != 0
        printf("Created table %p (%s)\n", table.m_pSelf, pszTableName);
#endif
        return table;
    }

    void CVDatabase::ColumnCreateParams ( ::KCreateMode cmode, ::KChecksum checksum, size_t pgsize )
    {
        rc_t rc = ::VDatabaseColumnCreateParams ( m_pSelf, cmode, checksum, pgsize );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDatabaseColumnCreateParams");
    }
#endif

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
#if DEBUG_PRINT != 0
            printf("Releasing VSchema %p\n", m_pSelf);
#endif
            ::VSchemaRelease ( m_pSelf );
            m_pSelf = NULL;
        }
    }

    void CVSchema::Clone ( CVSchema const& x )
    {
        if (m_pSelf)
            Release();

        m_pSelf = x.m_pSelf;
        ::VSchemaAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING VSchema %p\n", m_pSelf);
#endif
    }

    void CVSchema::VSchemaParseFile ( char const* pszFilePath )
    {
        rc_t rc = ::VSchemaParseFile ( m_pSelf, pszFilePath );
        if (rc)
            throw Utils::CErrorMsg(rc, "VSchemaParseFile (%s)", pszFilePath);
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
#if DEBUG_PRINT != 0
            printf("Releasing VDBManager %p\n", m_pSelf);
#endif
            ::VDBManagerRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

#if MANGER_WRITABLE != 0
    void CVDBManager::Make()
    {
        assert(m_pSelf == NULL);
        if (m_pSelf)
            throw Utils::CErrorMsg(0, "Double call to VDBManagerMakeUpdate");

        rc_t rc = ::VDBManagerMakeUpdate ( & m_pSelf, NULL );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerMakeUpdate");

	    /*rc = VDBManagerDisablePagemapThread ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerDisablePagemapThread");*/

#if DEBUG_PRINT != 0
        printf("Created VDBManager (wr) %p\n", m_pSelf);
#endif
    }
#else
    void CVDBManager::Make()
    {
        assert(m_pSelf == NULL);
        if (m_pSelf)
            throw Utils::CErrorMsg(0, "Double call to VDBManagerMakeRead");

        rc_t rc = ::VDBManagerMakeRead(const_cast<VDBManager const**>(&m_pSelf), NULL);
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerMakeRead");

#if DEBUG_PRINT != 0
        printf("Created VDBManager (rd) %p\n", m_pSelf);
#endif
    }
#endif

    CVDatabase CVDBManager::OpenDB(char const* pszDBName) const
    {
        CVDatabase vdb;
        rc_t rc = ::VDBManagerOpenDBRead(m_pSelf, const_cast<VDatabase const**>(& vdb.m_pSelf), NULL, pszDBName);
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerOpenDBRead (%s)", pszDBName);

#if DEBUG_PRINT != 0
        printf("Opened database %p (%s)\n", vdb.m_pSelf, pszDBName);
#endif
        return vdb;
    }

    CVTable CVDBManager::OpenTable(char const* pszPath) const
    {
        CVTable table;
        rc_t rc = ::VDBManagerOpenTableRead(m_pSelf, const_cast<VTable const**>(& table.m_pSelf), NULL, pszPath);
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerOpenTableRead (%s)", pszPath);

#if DEBUG_PRINT != 0
        printf("Opened table directly from manager %p (%s)\n", vdb.m_pSelf, pszDBName);
#endif
        return table;
    }

#if MANAGER_WRITABLE != 0
    CVDatabase CVDBManager::CreateDB ( CVSchema const& schema, char const* pszTypeDesc, ::KCreateMode cmode, char const* pszPath )
    {
        CVDatabase vdb;
        rc_t rc = ::VDBManagerCreateDB ( m_pSelf, & vdb.m_pSelf, schema.m_pSelf, pszTypeDesc, cmode, pszPath );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerCreateDB (%s)", pszPath);

#if DEBUG_PRINT != 0
        printf("Created database %p (%s)\n", vdb.m_pSelf, pszPath);
#endif

        // set creation mode of objects ( tables, columns, etc. ) to
        // create new or re-initialize existing, plus attach md5 checksums
        // to all files.
        // set blob creation mode to record 32-bit CRC within blob
        // continue to use default page size...
        vdb.ColumnCreateParams ( kcmInit | kcmMD5, kcsCRC32, 0 );
        return vdb;
    }
#endif

    CVSchema CVDBManager::MakeSchema () const
    {
        CVSchema schema;
        rc_t rc = ::VDBManagerMakeSchema ( m_pSelf, & schema.m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "VDBManagerMakeSchema");

#if DEBUG_PRINT != 0
        printf("Created Schema %p\n", schema.m_pSelf);
#endif
        return schema;
    }
}

namespace KDBObjects
{
//////////////////////////////////////////////////////////////
    CKIndex::CKIndex() : m_pSelf(NULL)
    {
    }

    CKIndex::~CKIndex()
    {
        Release();
    }

    CKIndex::CKIndex(CKIndex const& x)
    {
        Clone(x);
    }

    CKIndex& CKIndex::operator=(CKIndex const& x)
    {
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CKIndex::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing KIndex %p\n", m_pSelf);
#endif
            ::KIndexRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CKIndex::Clone(CKIndex const& x)
    {
        m_pSelf = x.m_pSelf;
        ::KIndexAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING KIndex %p\n", m_pSelf);
#endif
    }

    // returns false if not found
    bool CKIndex::FindText(const char *key, int64_t *start_id, uint64_t *id_count,
        int (CC * custom_cmp)(const void *item, struct PBSTNode const *n, void *data ),
        void *data) const
    {
            rc_t rc = ::KIndexFindText(m_pSelf, key, start_id, id_count, NULL, NULL );
            if (rc == SILENT_RC(rcDB,rcIndex,rcSelecting,rcString,rcNotFound))
                return false;
            else if (rc)
                throw Utils::CErrorMsg(rc, "KIndexFindText: key=%s", key);
            else
                return true;
    }


//////////////////////////////////////////////////////////////
    CKTable::CKTable() : m_pSelf(NULL)
    {
    }

    CKTable::~CKTable()
    {
        Release();
    }

    CKTable::CKTable(CKTable const& x)
    {
        Clone(x);
    }

    CKTable& CKTable::operator=(CKTable const& x)
    {
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CKTable::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing KTable %p\n", m_pSelf);
#endif
            ::KTableRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }

    void CKTable::Clone(CKTable const& x)
    {
        m_pSelf = x.m_pSelf;
        ::KTableAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING KTable %p\n", m_pSelf);
#endif
    }

    CKIndex CKTable::OpenIndexRead(char const* name) const
    {
        KDBObjects::CKIndex obj;
        rc_t rc = ::KTableOpenIndexRead(m_pSelf, const_cast<KIndex const**>(& obj.m_pSelf), name);
        if (rc)
            throw Utils::CErrorMsg(rc, "KTableOpenIndexRead(%s)", name);

#if DEBUG_PRINT != 0
        printf("Opened KIndex(%s) %p\n", name, obj.m_pSelf);
#endif
        return obj;
    }


//////////////////////////////////////////////////////////////
}


namespace KApp
{
    CArgs::CArgs () : m_pSelf(NULL)
    {
    }

    CArgs::~CArgs()
    {
        Release();
    }

    void CArgs::MakeAndHandle (int argc, char** argv, ::OptDef const* pOptions, size_t option_count)
    {
        if (m_pSelf)
            throw Utils::CErrorMsg (0, "Duplicated call to ArgsMakeAndHandle");

        rc_t rc = ::ArgsMakeAndHandle (&m_pSelf, argc, argv, 1, pOptions, option_count);
        if (rc)
            throw Utils::CErrorMsg(rc, "ArgsMakeAndHandle");
#if DEBUG_PRINT != 0
        printf("Created Args %p\n", m_pSelf);
#endif
    }
    void CArgs::MakeAndHandle ( int argc, char** argv, ::OptDef const* pOptions1, size_t option_count1, ::OptDef const* pOptions2, size_t option_count2 )
    {
        if (m_pSelf)
            throw Utils::CErrorMsg (0, "Duplicated call to ArgsMakeAndHandle");

        rc_t rc = ::ArgsMakeAndHandle (&m_pSelf, argc, argv, 2, pOptions1, option_count1, pOptions2, option_count2);
        if (rc)
            throw Utils::CErrorMsg(rc, "ArgsMakeAndHandle(2)");
#if DEBUG_PRINT != 0
        printf("Created Args(2) %p\n", m_pSelf);
#endif
    }

    void CArgs::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing Args %p\n", m_pSelf);
#endif
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
            throw Utils::CErrorMsg(rc, "ArgsParamCount");

        return ret;
    }

    char const* CArgs::GetParamValue ( uint32_t iteration ) const
    {
        void const* ret = NULL;
        rc_t rc = ::ArgsParamValue ( m_pSelf, iteration, & ret );
        if (rc)
            throw Utils::CErrorMsg(rc, "ArgsParamValue");

        return static_cast <char const*> (ret);
    }

    uint32_t CArgs::GetOptionCount ( char const* option_name ) const
    {
        uint32_t ret = 0;
        rc_t rc = ::ArgsOptionCount ( m_pSelf, option_name, &ret );
        if (rc)
            throw Utils::CErrorMsg(rc, "ArgsOptionCount (%s)", option_name);

        return ret;
    }

    char const* CArgs::GetOptionValue ( char const* option_name, uint32_t iteration ) const
    {
        void const* ret = NULL;
        rc_t rc = ::ArgsOptionValue ( m_pSelf, option_name, iteration, & ret );
        if (rc)
            throw Utils::CErrorMsg(rc, "ArgsOptionValue (%s)", option_name);

        return static_cast <char const*> (ret);
    }

////////////////////////////////

    CProgressBar::CProgressBar ( uint64_t size )
    {
        Make ( size );
    }

    CProgressBar::~CProgressBar ()
    {
        Release ();
    }

    void CProgressBar::Make ( uint64_t size )
    {
        rc_t rc = ::KLoadProgressbar_Make ( &m_pSelf, size );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLoadProgressbar_Make");
#if DEBUG_PRINT != 0
        printf ( "Created ProgressBar %p\n", m_pSelf );
#endif
    }
    void CProgressBar::Release ()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing ProgressBar %p\n", m_pSelf);
#endif
            ::KLoadProgressbar_Release ( m_pSelf, true );
            m_pSelf = NULL;
        }
    }

    void CProgressBar::Append ( uint64_t chunk )
    {
        rc_t rc = ::KLoadProgressbar_Append ( m_pSelf, chunk );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLoadProgressbar_Append");
    }

    void CProgressBar::Process ( uint64_t chunk, bool force_report )
    {
        rc_t rc = ::KLoadProgressbar_Process ( m_pSelf, chunk, force_report );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLoadProgressbar_Process");
    }

///////////////////////////////////////////
    CXMLLogger::CXMLLogger ( CArgs const& args )
    {
        Make ( args );
    }

    CXMLLogger::~CXMLLogger ()
    {
        Release ();
    }

    void CXMLLogger::Make ( CArgs const& args )
    {
        rc_t rc = ::XMLLogger_Make ( &m_pSelf, NULL, args.m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "XMLLogger_Make");
#if DEBUG_PRINT != 0
        printf ( "Created XMLLogger %p\n", m_pSelf );
#endif
    }

    void CXMLLogger::Release ()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing XMLLogger %p\n", m_pSelf);
#endif
            ::XMLLogger_Release ( m_pSelf );
            m_pSelf = NULL;
        }
    }
}

namespace Utils
{
    CErrorMsg::CErrorMsg(rc_t rc, char const* fmt_str, ...)
        : m_rc(rc)
    {
        va_list args;
        va_start(args, fmt_str);
        string_vprintf (m_szDescr, countof(m_szDescr), NULL, fmt_str, args);
        va_end(args);
    }

    CErrorMsg::CErrorMsg(char const* fmt_str, ...)
        : m_rc(0)
    {
        va_list args;
        va_start(args, fmt_str);
        string_vprintf (m_szDescr, countof(m_szDescr), NULL, fmt_str, args);
        va_end(args);
    }

    rc_t CErrorMsg::getRC() const
    {
        return m_rc;
    }
    char const* CErrorMsg::what() const throw()
    {
        return m_szDescr;
    }


    void HandleException ()
    {
        try
        {
            throw;
        }
        catch (Utils::CErrorMsg const& e)
        {
            char szBufErr[512] = "";
            size_t rc = e.getRC();
            rc_t res;
            if (rc != 0)
                res = string_printf(szBufErr, countof(szBufErr), NULL, "ERROR: %s failed with error 0x%08x (%u) [%R]", e.what(), rc, rc, rc);
            else
                res = string_printf(szBufErr, countof(szBufErr), NULL, "ERROR: %s", e.what());
            if (res == rcBuffer || res == rcInsufficient)
                szBufErr[countof(szBufErr) - 1] = '\0';
            LOGMSG ( klogErr, szBufErr );
        }
        catch (std::exception const& e)
        {
            char szBufErr[512];
            rc_t res = string_printf(szBufErr, countof(szBufErr), NULL,
                "std::exception: %s", e.what());
            if (res == rcBuffer || res == rcInsufficient)
                szBufErr[countof(szBufErr) - 1] = '\0';
            LOGMSG ( klogErr, szBufErr );
        }
        catch (...)
        {
            LOGMSG ( klogErr, "Unexpected exception occured" );
        }
    }
}

namespace KSearch
{
////////////////////////////////////////////////

    CVRefVariation::CVRefVariation()
    {
        ClearMembers();
    }

    CVRefVariation::~CVRefVariation()
    {
        Release();
    }

    CVRefVariation::CVRefVariation(CVRefVariation const& x)
    {
        Clone(x);
    }

    CVRefVariation& CVRefVariation::operator=(CVRefVariation const& x)
    {
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CVRefVariation::ClearMembers()
    {
        m_pSelf             = NULL;
        m_bases_start       = KSearch::UNINITIALIZED_POSITION;
        m_allele            = NULL;
        m_query             = NULL;
        m_allele_len        = KSearch::UNINITIALIZED_POSITION;
        m_allele_len_on_ref = KSearch::UNINITIALIZED_POSITION;
        m_query_len         = KSearch::UNINITIALIZED_POSITION;
        m_query_len_on_ref  = KSearch::UNINITIALIZED_POSITION;
        m_allele_start      = KSearch::UNINITIALIZED_POSITION;
        m_query_start       = KSearch::UNINITIALIZED_POSITION;
    }

    void CVRefVariation::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing VRefVariation %p\n", m_pSelf);
#endif
            ::RefVariationRelease(m_pSelf); // don't throw an exception since this function is called in the destructor
            ClearMembers();
        }
    }

    void CVRefVariation::Clone(CVRefVariation const& x)
    {
        m_pSelf = x.m_pSelf;
        ::RefVariationAddRef ( m_pSelf ); // TODO: maybe we have to check rc and throw here
        m_bases_start = x.m_bases_start;

        m_allele            = x.m_allele;
        m_query             = x.m_query;
        m_allele_len        = x.m_allele_len;
        m_allele_len_on_ref = x.m_allele_len_on_ref;
        m_query_len         = x.m_query_len;
        m_query_len_on_ref  = x.m_query_len_on_ref;
        m_allele_start      = x.m_allele_start;
        m_query_start       = x.m_query_start;

#if DEBUG_PRINT != 0
        printf ("CLONING VRefVariation %p\n", m_pSelf);
#endif
    }
    char const* CVRefVariation::GetSearchQuery() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetSearchQuery on uninitialized RefVariation");

        if ( m_query == NULL )
        {
            rc_t rc = ::RefVariationGetIUPACSearchQuery ( m_pSelf, & m_query, & m_query_len, & m_query_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetIUPACSearchQuery");
        }

        return m_query;
    }

    size_t CVRefVariation::GetSearchQueryStartRelative() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetSearchQueryStartRelative on uninitialized RefVariation");

        if ( m_query_start == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetIUPACSearchQuery ( m_pSelf, & m_query, & m_query_len, & m_query_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetIUPACSearchQuery");
        }

        return m_query_start;
    }

    size_t CVRefVariation::GetSearchQueryStartAbsolute() const
    {
        return GetSearchQueryStartRelative() + m_bases_start;
    }

    size_t CVRefVariation::GetSearchQuerySize() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetSearchQuerySize on uninitialized RefVariation");

        if ( m_query_len == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetIUPACSearchQuery ( m_pSelf, & m_query, & m_query_len, & m_query_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetIUPACSearchQuery");
        }

        return m_query_len;
    }

    size_t CVRefVariation::GetSearchQueryLenOnRef() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetSearchQueryLenOnRef on uninitialized RefVariation");

        if ( m_query_len_on_ref == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetSearchQueryLenOnRef ( m_pSelf, & m_query_len_on_ref );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetSearchQueryLenOnRef");
        }

        return m_query_len_on_ref;
    }


    char const* CVRefVariation::GetAllele() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetAllele on uninitialized RefVariation");

        if ( m_allele == NULL )
        {
            rc_t rc = ::RefVariationGetAllele ( m_pSelf, & m_allele, & m_allele_len, & m_allele_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetAllele");
        }

        return m_allele;
    }

    size_t CVRefVariation::GetAlleleStartRelative() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetAllele on uninitialized RefVariation");

        if ( m_allele_start == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetAllele ( m_pSelf, & m_allele, & m_allele_len, & m_allele_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetAllele");
        }

        return m_allele_start;
    }

    size_t CVRefVariation::GetAlleleStartAbsolute() const
    {
        return GetAlleleStartRelative() + m_bases_start;
    }

    size_t CVRefVariation::GetAlleleSize() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetAlleleSize on uninitialized RefVariation");

        if ( m_allele_len == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetAllele ( m_pSelf, & m_allele, & m_allele_len, & m_allele_start );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetAllele");
        }

        return m_allele_len;
    }

    size_t CVRefVariation::GetAlleleLenOnRef() const
    {
        if ( m_pSelf == NULL )
            throw Utils::CErrorMsg("GetAlleleLenOnRef on uninitialized RefVariation");

        if ( m_allele_len_on_ref == KSearch::UNINITIALIZED_POSITION )
        {
            rc_t rc = ::RefVariationGetAlleleLenOnRef ( m_pSelf, & m_allele_len_on_ref );
            if (rc)
                throw Utils::CErrorMsg(rc, "RefVariationGetAlleleLenOnRef");
        }

        return m_allele_len_on_ref;
    }

    CVRefVariation VRefVariationIUPACMake ( ::RefVarAlg alg, char const* ref, size_t ref_size,
            size_t ref_pos_var, char const* variation, size_t variation_size,
            size_t var_len_on_ref, size_t bases_start)
    {
        CVRefVariation obj;
        rc_t rc = ::RefVariationIUPACMake ( & obj.m_pSelf,
            ref, ref_size, ref_pos_var, var_len_on_ref, variation, variation_size, alg );
        if (rc)
            throw Utils::CErrorMsg(rc, "RefVariationIUPACMake");

#if DEBUG_PRINT != 0
        printf("Created RefVariation (rd) %p\n", obj.m_pSelf);
#endif
        obj.m_bases_start = bases_start;
        return obj;
    }
}

namespace KProc
{
    rc_t KThreadFunc ( KThread const* , void* data )
    {
        CKThread* obj = static_cast<CKThread*>(data);

        return ( * obj -> m_ThreadFunc ) ( obj -> m_pData );
    }

    CKThread::CKThread ()
        : m_pSelf(NULL), m_ThreadFunc(NULL), m_pData(NULL)
    {
    }

    CKThread::~CKThread()
    {
        Release();
    }
    CKThread::CKThread(CKThread const& x)
    {
        Clone(x);
    }
    CKThread& CKThread::operator=(CKThread const& x)
    {
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CKThread::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing KThread %p\n", m_pSelf);
#endif
            ::KThreadRelease(m_pSelf);
            m_pSelf = NULL;
            m_ThreadFunc = NULL;
            m_pData = NULL;
        }
    }
    void CKThread::Clone(CKThread const& x)
    {
        m_pSelf = x.m_pSelf;
        m_ThreadFunc = x.m_ThreadFunc;
        m_pData = x.m_pData;

        if ( m_pSelf != NULL )
        {
            ::KThreadAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
            printf ("CLONING KThread %p\n", m_pSelf);
#endif
        }
    }

    void CKThread::Make ( THREAD_FUNC thread_func, void* data )
    {
        assert (m_pSelf == NULL);
        assert (m_ThreadFunc == NULL);
        assert (m_pData == NULL);

        m_ThreadFunc = thread_func;
        m_pData = data;

        rc_t rc = ::KThreadMakeStackSize ( & m_pSelf, KThreadFunc, this, 0 );
        if (rc)
            throw Utils::CErrorMsg(rc, "KThreadMake");
    }

    rc_t CKThread::Wait ()
    {
        rc_t status;
        rc_t rc = ::KThreadWait ( m_pSelf, & status );
        if (rc)
            throw Utils::CErrorMsg(rc, "KThreadWait");

        return status;
    }

///////////////////////////////////////////////////////

    CKLock::CKLock ( )
    {
        rc_t rc = ::KLockMake ( & m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLockMake");
    }

    CKLock::~CKLock()
    {
        Release();
    }

    CKLock::CKLock(CKLock const& x)
    {
        Clone(x);
    }
    CKLock& CKLock::operator=(CKLock const& x)
    {
        if (m_pSelf)
            Release();

        Clone(x);
        return *this;
    }

    void CKLock::Release()
    {
        if (m_pSelf)
        {
#if DEBUG_PRINT != 0
            printf("Releasing KLock %p\n", m_pSelf);
#endif
            ::KLockRelease(m_pSelf);
            m_pSelf = NULL;
        }
    }
    void CKLock::Clone(CKLock const& x)
    {
        m_pSelf = x.m_pSelf;
        ::KLockAddRef ( m_pSelf );
#if DEBUG_PRINT != 0
        printf ("CLONING KLock %p\n", m_pSelf);
#endif
    }

    void CKLock::Acquire ()
    {
        rc_t rc = ::KLockAcquire ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLockAcquire");
    }

    void CKLock::Lock ()
    {
        Acquire ();
    }

    void CKLock::Unlock ()
    {
        rc_t rc = ::KLockUnlock ( m_pSelf );
        if (rc)
            throw Utils::CErrorMsg(rc, "KLockUnlock");
    }

    void CKLock::lock ()
    {
        Lock();
    }
    void CKLock::unlock ()
    {
        Unlock();
    }
}
