#ifndef WINDOWS
//#define DEPURAR 1
#endif
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
* ==============================================================================
*
*/

#include "diagnose/diagnose.h" /* KDiagnoseAdvanced */

#include <kfg/config.h> /* KConfigReadString */

#include <kfs/directory.h> /* KDirectoryRelease */
#include <kfs/file.h> /* KFile */

#include <klib/data-buffer.h> /* KDataBuffer */
#include <klib/out.h> /* KOutMsg */
#include <klib/printf.h> /* string_vprintf */
#include <klib/rc.h>
#include <klib/text.h> /* String */
#include <klib/vector.h> /* Vector */

#include <ascp/ascp.h> /* aspera_get */
#include <kns/endpoint.h> /* KNSManagerInitDNSEndpoint */
#include <kns/http.h> /* KHttpRequest */
#include <kns/manager.h> /* KNSManager */
#include <kns/kns-mgr-priv.h> /* KNSManagerMakeReliableHttpFile */
#include <kns/stream.h> /* KStream */

#include <kproc/cond.h> /* KConditionRelease */
#include <kproc/lock.h> /* KLockRelease */

#include <vfs/manager.h> /* VFSManagerOpenDirectoryRead */
#include <vfs/path.h> /* VFSManagerMakePath */
#include <vfs/resolver.h> /* VResolverRelease */

#include <strtol.h> /* strtoi64 */

#include <ctype.h> /* isprint */
#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

static rc_t CC OutMsg ( int level, unsigned type,
                        const char * fmt, va_list args )
{
    return KOutVMsg ( fmt, args );
}

static rc_t ( CC * LOGGER )
    ( int level, unsigned type, const char * fmt, va_list args );

LIB_EXPORT rc_t CC KDiagnoseLogHandlerSet ( KDiagnose * self,
        rc_t ( CC * logger ) ( int level, unsigned type,
                               const char * fmt, va_list args )
    )
{
    LOGGER = logger;
    return 0;
}

LIB_EXPORT
rc_t CC KDiagnoseLogHandlerSetKOutMsg ( KDiagnose * self )
{
    return KDiagnoseLogHandlerSet ( self, OutMsg );
}

static rc_t LogOut ( int level, unsigned type, const char * fmt, ... )
{
    rc_t rc = 0;

    va_list args;
    va_start ( args, fmt );

    if ( LOGGER != NULL )
        rc = LOGGER ( level, type, fmt, args );

    va_end ( args );

    return rc;
}

typedef struct { const char * begin; char * end; } Block;

typedef struct {
    KDataBuffer response; /* cgi response */
    uint32_t code;        /* cgi response status code */
    char * location;      /* cgi response redirect location */
    KDataBuffer redirect; /* redirect response */

    Block p;
    Block mailto;

    String ip;
    String date;
    char * server;
} Abuse;

static rc_t AbuseFini ( Abuse * self ) {
    rc_t rc = 0, r2 = 0;

    assert ( self );

    free ( self -> location );
    free ( self -> server );

    rc = KDataBufferWhack ( & self -> response );

    r2 = KDataBufferWhack ( & self -> redirect );
    if ( r2 != 0 && rc == 0 )
        rc = r2;

    memset ( self, 0, sizeof * self );

    return rc;
}

static void AbuseInit ( Abuse * self ) {
    assert ( self );

    memset ( self, 0, sizeof * self );

    self -> response . elem_bits = self -> redirect . elem_bits = 8;
}

static void AbuseSetStatus ( Abuse * self, uint32_t code ) {
    assert ( self );

    self -> code = code;
}

static
rc_t AbuseSetLocation ( Abuse * self, const char * str, size_t size )
{
    assert ( self );

    self -> location = string_dup ( str, size );
    return self -> location != NULL
        ? 0 : RC ( rcRuntime, rcString, rcCopying, rcMemory, rcExhausted );
}

static
rc_t AbuseSetCgi ( Abuse * self, const String * cgi ) {
    const char * s = NULL, * e = NULL;

    size_t i = 0;

    assert ( self && cgi );

    s = cgi -> addr;
    if ( s == NULL )
        return 0;

    for ( i = 0; i < cgi -> size ; ++ i ) {
        switch ( * s ) {
         case '\0':
          return 0;

         case ':' :
          if ( * ++ s != '/' )
            return 0;
          if ( * ++ s != '/' )
            return 0;
          if ( i >= cgi -> size )
            return 0;

          e = string_chr ( ++ s, cgi -> size - i, '/' );
          if ( e == NULL )
            return 0;

          self -> server = string_dup ( s, e - s );
          if ( self -> server == NULL )
            return RC ( rcRuntime, rcString, rcCopying, rcMemory, rcExhausted );
          else
            return 0;

         default:
          ++ s;
        }
    }

    return 0;
}

static rc_t AbuseAdd ( Abuse * self, const char * txt, int sz ) {
    rc_t rc = 0;

    assert ( self );

    if ( rc == 0 )
        return KDataBufferPrintf ( & self -> response,  "%s", txt );
    else
        return KDataBufferPrintf ( & self -> response,  "%.*s", sz, txt );
}

static char * find ( const char * haystack, uint64_t n,
                     const String * needle )
{
    uint64_t i = 0;

    assert ( needle && needle -> addr );

    for ( i = 0; ; ++ i ) {
        char * c = NULL;
        uint64_t size = n - i;

        assert ( n >= i );

        c = string_chr ( haystack + i, size, * needle -> addr );
        if ( c == NULL )
            return NULL;

        i = c - haystack;
        if ( i < needle -> size && i > 0)
            return NULL;

        if ( string_cmp ( c, needle -> size, needle -> addr, needle -> size,
                             ( uint32_t ) needle -> size) == 0)
        {
            return c;
        }
    }
}

typedef struct {
    bool incompleteGapKfg;
    bool firewall;
    bool blocked;

    Abuse abuse;
} Report;

struct KDiagnose {
    atomic32_t refcount;

    KConfig    * kfg;
    KNSManager * kmgr;
    VFSManager * vmgr;
    rc_t       (CC * quitting) (void);

    int verbosity;

    Vector tests;
    Vector errors;

    KDiagnoseTestDesc * desc;

    enum EState {
        eRunning,
        ePaused,
        eCanceled,
    } state;
    KLock * lock;
    KCondition * condition;

    Report report;
};

struct KDiagnoseTest {
    struct KDiagnoseTest * parent;
    const struct KDiagnoseTest * next;
    const struct KDiagnoseTest * nextChild;
    const struct KDiagnoseTest * firstChild;
    struct KDiagnoseTest * crntChild;
    char * name;
    uint64_t code;
    uint32_t level;
    char * message;
    EKDiagTestState state;

    char * number;
    char * numberNode;
};

static void KDiagnoseTestWhack ( KDiagnoseTest * self ) {
    assert ( self );
    free ( self -> name );
    free ( self -> message );
    free ( self -> number );
    free ( self -> numberNode );
    memset ( self, 0, sizeof * self );
    free ( self );
}


LIB_EXPORT rc_t CC KDiagnoseGetTests ( const KDiagnose * self,
                                       const KDiagnoseTest ** test )
{
    if ( test == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * test = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    * test = VectorGet ( & self -> tests, 0 );
    return 0;
}

LIB_EXPORT rc_t CC KDiagnoseTestNext ( const KDiagnoseTest * self,
                                       const KDiagnoseTest ** test )
{
    if ( test == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * test = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    * test = self -> next;
    return 0;
}

LIB_EXPORT rc_t CC KDiagnoseTestChild ( const KDiagnoseTest * self,
                          uint32_t idx, const KDiagnoseTest ** test )
{
    const KDiagnoseTest * t = NULL;
    uint32_t i;

    if ( test == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * test = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    for ( i = 0, t = self -> firstChild; i < idx && t != NULL;
          ++ i, t = t->nextChild );

    * test = t;
    return 0;
}

#define TEST_GET_INT( PROPERTY )       \
    do {                               \
        if ( PROPERTY == NULL )        \
            return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull ); \
        * PROPERTY = 0;             \
        if ( self == NULL )            \
            return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );  \
        * PROPERTY = self -> PROPERTY; \
        return 0;                      \
    } while ( 0 )

#define TEST_GET( PROPERTY )       \
    do {                               \
        if ( PROPERTY == NULL )        \
            return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull ); \
        * PROPERTY = NULL;             \
        if ( self == NULL )            \
            return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );  \
        * PROPERTY = self -> PROPERTY; \
        return 0;                      \
    } while ( 0 )

LIB_EXPORT rc_t CC KDiagnoseTestName ( const KDiagnoseTest * self,
                                       const char ** name )
{   TEST_GET ( name ); }

LIB_EXPORT rc_t CC KDiagnoseTestCode ( const KDiagnoseTest * self,
                                       uint64_t * code )
{   TEST_GET_INT ( code ); }

LIB_EXPORT rc_t CC KDiagnoseTestLevel ( const KDiagnoseTest * self,
                                        uint32_t * level )
{   TEST_GET_INT ( level ); }

LIB_EXPORT rc_t CC KDiagnoseTestNumber ( const KDiagnoseTest * self,
                                         const char ** number )
{   TEST_GET ( number ); }

LIB_EXPORT rc_t CC KDiagnoseTestMessage ( const KDiagnoseTest * self,
                                          const char ** message )
{   TEST_GET ( message ); }

LIB_EXPORT rc_t CC KDiagnoseTestState ( const KDiagnoseTest * self,
                                        EKDiagTestState * state )
{   TEST_GET_INT ( state ); }


struct KDiagnoseError {
    atomic32_t refcount;

    char * message;
};

static const char DIAGNOSERROR_CLSNAME [] = "KDiagnoseError";

LIB_EXPORT
rc_t CC KDiagnoseErrorAddRef ( const KDiagnoseError * self )
{
    if ( self != NULL )
        switch ( KRefcountAdd ( & self -> refcount, DIAGNOSERROR_CLSNAME ) ) {
            case krefLimit:
                return RC ( rcRuntime,
                            rcData, rcAttaching, rcRange, rcExcessive );
        }

    return 0;
}

static void KDiagnoseErrorWhack ( KDiagnoseError * self ) {
    assert ( self );
    free ( self -> message );
    memset ( self, 0, sizeof * self );
    free ( self );
}

LIB_EXPORT
rc_t CC KDiagnoseErrorRelease ( const KDiagnoseError * cself )
{
    rc_t rc = 0;

    KDiagnoseError * self = ( KDiagnoseError * ) cself;

    if ( self != NULL )
        switch ( KRefcountDrop ( & self -> refcount,
                                 DIAGNOSERROR_CLSNAME ) )
        {
            case krefWhack:
                KDiagnoseErrorWhack ( self );
                break;
            case krefNegative:
                return RC ( rcRuntime,
                            rcData, rcReleasing, rcRange, rcExcessive );
        }

    return rc;
}

LIB_EXPORT rc_t CC KDiagnoseErrorGetMsg ( const KDiagnoseError * self,
                                          const char ** message )
{
    if ( message == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * message = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    * message = self -> message;
    return 0;
}

static rc_t KDiagnoseErrorMake ( const KDiagnoseError ** self,
                                 const char * message )
{
    KDiagnoseError * p = NULL;

    assert ( self );

    * self = NULL;

    p = calloc ( 1, sizeof * p );
    if ( p == NULL )
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );

    p -> message = string_dup_measure ( message, NULL );
    if ( p == NULL ) {
        KDiagnoseErrorWhack ( p );
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );
    }

    KRefcountInit ( & p -> refcount, 1, DIAGNOSERROR_CLSNAME, "init", "" );

    * self = p;

    return 0;
}

static void ( CC * CALL_BACK )
    ( EKDiagTestState state, const KDiagnoseTest * test, void * data );
static void * CALL_BACK_DATA;

LIB_EXPORT rc_t CC KDiagnoseTestHandlerSet ( KDiagnose * self,
    void ( CC * callback )
        ( EKDiagTestState state, const KDiagnoseTest * test, void * data ),
    void * data
)
{
    CALL_BACK = callback;
    CALL_BACK_DATA = data;
    return 0;
}

LIB_EXPORT
rc_t CC KDiagnoseSetVerbosity ( KDiagnose * self, int verbosity )
{
    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    self -> verbosity = verbosity - 1;

    return 0;
}

LIB_EXPORT rc_t CC KDiagnoseGetErrorCount ( const KDiagnose * self,
                                            uint32_t * count )
{
    if ( count == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * count = 0;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    * count = VectorLength ( & self -> errors );
    return 0;
}

LIB_EXPORT rc_t CC KDiagnoseGetError ( const KDiagnose * self, uint32_t idx,
                                       const KDiagnoseError ** error )
{
    rc_t rc = 0;

    const KDiagnoseError * e = NULL;

    if ( error == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * error = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    if ( idx >= VectorLength ( & self -> errors ) )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcInvalid );

    e = VectorGet ( & self -> errors, idx );

    rc = KDiagnoseErrorAddRef ( e );
    if ( rc == 0 )
        * error = e;

    return rc;
}

typedef struct {
    int n [ 7 ];
    int level;
    bool ended;
    bool started;           /*  TestStart did not terminale string by EOL */
    bool failedWhileSilent;

    int verbosity; /* -3    none  ( KVERBOSITY_NONE  )
                      -2    error ( KVERBOSITY_ERROR )
                      -1    info  ( KVERBOSITY_INFO  )
                       0... last printed index of n [] */

    int total;
    int failures;
    int warnings;
    KDiagnoseTest * crnt;
    KDiagnoseTest * root;
    Vector * tests;
    Vector * errors;

    KDataBuffer msg;

    const KConfig * kfg;
    const KNSManager * kmgr;
    const VFSManager * vmgr;
    VResolver * resolver;
    VResolverEnableState cacheState;
    KDirectory * dir;

    bool ascpChecked;
    const char * ascp;
    const char * asperaKey;

    KDiagnose * dad;
} STest;

static void STestInit ( STest * self, KDiagnose * test )
{
    rc_t rc = 0;

    assert ( self && test );

    memset ( self, 0, sizeof * self );

    self -> dad = test;

    self -> level = -1;

    self -> kfg = test -> kfg;
    self -> kmgr = test -> kmgr;
    self -> vmgr = test -> vmgr;
    self -> errors = & test -> errors;
    self -> tests = & test -> tests;

    self -> verbosity = test -> verbosity;
    if ( self -> verbosity > 0 )
        -- self -> verbosity;
    else if ( self -> verbosity == 0 ) /* max */
        self -> verbosity = sizeof self -> n / sizeof self -> n [ 0 ] - 1;

    rc = KDirectoryNativeDir ( & self -> dir );
    if ( rc != 0 )
        LogOut ( KVERBOSITY_ERROR, 0, "CANNOT KDirectoryNativeDir: %R\n", rc );

    rc = VFSManagerGetResolver ( self -> vmgr, & self -> resolver);
    if ( rc != 0 )
        LogOut ( KVERBOSITY_ERROR, 0, "CANNOT GetResolver: %R\n", rc );
    else
        self -> cacheState = VResolverCacheEnable ( self -> resolver,
                                                    vrAlwaysEnable );
}

static void STestReport ( const STest * self ) {
    assert ( self && self -> dad );

    if ( self -> level < KVERBOSITY_INFO )
        return;

    if ( self -> n [ 0 ] == 0 || self -> n [ 1 ] != 0 ||
            self -> level != 0 )
    {   LogOut ( KVERBOSITY_INFO, 0, "= TEST WAS NOT COMPLETED\n" ); }

    LogOut ( KVERBOSITY_INFO, 0, "= %d (%d) tests performed, %d failed\n",
                self -> n [ 0 ], self -> total, self -> failures );

    if ( self -> failures > 0 ) {
        uint32_t i = 0;
        LogOut ( KVERBOSITY_INFO, 0, "Errors:\n" );
        for ( i = 0; i < VectorLength ( self -> errors ); ++ i ) {
            const KDiagnoseError * e = VectorGet ( self -> errors, i );
            assert ( e );
            LogOut ( KVERBOSITY_INFO, 0, " %d: %s\n", i + 1, e -> message );
        }
    }

    LogOut ( KVERBOSITY_INFO, 0, "\n\nANALYSIS:\n\n" );

    if ( self -> failures == 0 )
        LogOut ( KVERBOSITY_INFO, 0, "No errors detected.\n" );
    else {
        const Report * report = & self -> dad -> report;

        if ( report -> firewall )
            LogOut ( KVERBOSITY_INFO, 0,
 "Most likely access to NCBI is blocked by your firewall.\n"
 "Please look over the information about firewalls at\n"
 "https://github.com/ncbi/sra-tools/wiki/Firewall-and-Routing-Information\n"
 "and make sure that your IT people are aware of the requirements\n"
 "for accessing SRA data at NCBI.\n\n" );

        if ( report -> blocked ) {
            const Abuse * test = & report -> abuse;
            LogOut ( KVERBOSITY_INFO, 0,
       "Your access to the NCBI website at %s has been\n"
       "temporarily blocked due to a possible misuse/abuse situation\n"
       "involving your site. This is not an indication of a security issue\n"
       "such as a virus or attack.\n"
       "To restore access and understand how to better interact with our site\n"
       "to avoid this in the future, please have your system administrator\n"
       "send an email with subject \"NCBI Web site BLOCKED: %S\"\n"
       "to info@ncbi.nlm.nih.gov with the following information:\n"
       "Error=blocked for possible abuse\n"
       "Server=%s\n"
       "Client=%S\n"
       "Time=%S\n\n"
                , test -> server
                , & test -> ip
                , test -> server
                , & test -> ip
                , & test -> date );
        }
    }

    LogOut ( KVERBOSITY_INFO, 0,
        "For more infotrmation mail the complete output\n"
        "and your questions to sra-tools@ncbi.nlm.nih.gov .\n" );
}

static void STestFini ( STest * self ) {
    rc_t rc = 0;

    assert ( self );

    if ( self -> level >= KVERBOSITY_INFO )
        STestReport ( self );

    VResolverCacheEnable ( self -> resolver, self -> cacheState );

    RELEASE ( KDirectory, self -> dir );
    RELEASE ( VResolver, self -> resolver );

    KDataBufferWhack ( & self -> msg );

    free ( ( void * ) self -> ascp );
    free ( ( void * ) self -> asperaKey );

    memset ( self, 0, sizeof * self );
}

static rc_t KDiagnoseCheckState(KDiagnose * self) {
    rc_t rc = 0;

    assert ( self );

    if ( self -> quitting && ( rc = self -> quitting () ) != 0 )
        if ( rc == SILENT_RC ( rcExe,
                               rcProcess, rcExecuting, rcProcess, rcCanceled ) )
        {
            LogOut ( KVERBOSITY_INFO, 0,
                     "= Signal caught: CANCELED DIAGNOSTICS\n" );
            self -> state = eCanceled;
            if ( CALL_BACK )
                CALL_BACK ( eKDTS_Canceled, NULL, CALL_BACK_DATA );
        }

    while ( self -> state != eRunning ) {
        rc_t r2;

        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
            switch ( self -> state ) {
                case eRunning:
                    break;

                case ePaused:
                    LogOut ( KVERBOSITY_INFO, 0, "= PAUSED DIAGNOSTICS\n" );
                    if ( CALL_BACK )
                        CALL_BACK ( eKDTS_Paused, NULL, CALL_BACK_DATA );

                    rc = KConditionWait ( self -> condition, self -> lock );
                    if ( rc != 0 )
                        LogOut ( KVERBOSITY_INFO, 0,
                                 "= FAILURE DURING PAUSE: %R\n" );
                    else if ( self -> state == eRunning ) {
                        LogOut ( KVERBOSITY_INFO, 0,
                                 "= RESUMED DIAGNOSTICS\n" );
                        if ( CALL_BACK )
                            CALL_BACK ( eKDTS_Resumed, NULL, CALL_BACK_DATA );
                    }

                    break;

                case eCanceled:
                    LogOut ( KVERBOSITY_INFO, 0, "= CANCELED DIAGNOSTICS\n" );
                    if ( rc == 0 )
                        rc = RC ( rcRuntime, rcProcess, rcExecuting,
                                  rcProcess, rcCanceled );
                    if ( CALL_BACK )
                        CALL_BACK ( eKDTS_Canceled, NULL, CALL_BACK_DATA );

                    break;
            }

        r2 = KLockUnlock ( self -> lock );
        if ( rc == 0 && r2 != 0 )
            rc = r2;

        if ( self -> state == eCanceled )
            break;
    }

    return rc;
}

static rc_t STestVStart ( STest * self, bool checking, uint64_t code,
    const char * fmt, va_list args  )
{
    KDiagnoseTest * test = NULL;
    rc_t rc = 0;
    int i = 0;
    char b [ 512 ] = "";
    bool next = false;
    KDataBuffer bf;

    memset ( & bf, 0, sizeof bf );
    rc = string_vprintf ( b, sizeof b, NULL, fmt, args );
    if ( rc != 0 ) {
        LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R\n", rc );
        return rc;
    }

    assert ( self );

    test = calloc ( 1, sizeof * test );
    if ( test == NULL )
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );
    test -> code = code;
    test -> name = strdup (b); /*TODO*/
    if ( test -> name == NULL ) {
        free ( test );
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );
    }

    if ( self -> ended ) {
        next = true;
        self -> ended = false;
    }
    else
        ++ self -> level;

    test -> level = self -> level;
    test -> state = eKDTS_Started;

    if ( self -> crnt != NULL ) {
        if ( next ) {
            self -> crnt -> next = test;
            test -> parent = self -> crnt -> parent;
        }
        else {
            if ( self -> crnt -> firstChild == NULL )
                self -> crnt -> firstChild = test;
            else {
                KDiagnoseTest * child = self -> crnt -> crntChild;
                assert ( child );
                child -> nextChild = test;
            }
            self -> crnt -> crntChild = test;
            test -> parent = self -> crnt;
        }
    }
    else
        self -> root = test;
    self -> crnt = test;
    rc = VectorAppend ( self -> tests, NULL, test );
    if ( rc != 0 )
        return rc;

    assert ( self -> level >= 0 );
    assert ( self -> level < sizeof self -> n / sizeof self -> n [ 0 ] );

    ++ self -> n [ self -> level ];

    if ( self -> msg . elem_count > 0 ) {
        assert ( self -> msg . base );
        ( ( char * ) self -> msg . base)  [ 0 ] = '\0';
        self -> msg . elem_count = 0;
    }
    rc = KDataBufferPrintf ( & self -> msg,  "< %d", self -> n [ 0 ] );
#ifdef DEPURAR
const char*c=self->msg.base;
#endif
    if ( rc != 0 )
        LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R\n", rc );
    else {
        size_t size = 0;
        for ( i = 1; rc == 0 && i <= self -> level; ++ i ) {
            rc = KDataBufferPrintf ( & self -> msg, ".%d", self -> n [ i ] );
            if ( rc != 0 )
                LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R\n", rc );
        }
        assert ( self -> msg . base && self -> msg . elem_count > 2 );
        test -> number = string_dup_measure ( ( char * ) self -> msg . base + 2,
                                              NULL );
        if ( test -> number == NULL )
            return RC ( rcRuntime,
                        rcData, rcAllocating, rcMemory, rcExhausted );

        test -> numberNode = string_dup_measure ( test -> number, & size );
        if ( test -> numberNode == NULL )
            return RC ( rcRuntime,
                        rcData, rcAllocating, rcMemory, rcExhausted );
        else {
            while ( true ) {
                char * c = string_chr ( test -> numberNode, size, '.' );
                if ( c == NULL )
                    break;
                else
                    * c = '/';
            }
        }

    }
    if ( rc == 0 )
    {
        rc = KDataBufferPrintf ( & self -> msg, " %s ", b );
        if ( rc != 0 )
            LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R\n", rc );
    }

    if ( self -> level <= self -> verbosity ) {
        rc = LogOut ( self -> level, 0, "> %d", self -> n [ 0 ] );
        for ( i = 1; i <= self -> level; ++ i )
            rc = LogOut ( self -> level, 0, ".%d", self -> n [ i ] );

        rc = LogOut ( self -> level, 0, " %s%s%s",
                      checking ? "Checking " : "", b, checking ? "..." : " " );
        if ( checking ) {
            if ( self -> level < self -> verbosity ) {
                rc = LogOut ( self -> level, 0, "\n" );
                self -> started = false;
            }
            else {
                rc = LogOut ( self -> level, 0, " " );
                self -> started = true;
            }
        }
    }

    if ( CALL_BACK )
         CALL_BACK ( eKDTS_Started, test, CALL_BACK_DATA );

    return rc;
}

typedef enum {
    eFAIL,
    eOK,
    eMSG,
    eEndFAIL,
    eEndOK,
    eDONE, /* never used */
    eCANCELED,
    eWarning
} EOK;

static rc_t STestVEnd ( STest * self, EOK ok,
                        const char * fmt, va_list args )
{
    rc_t rc = 0;
#ifdef DEPURAR
switch(ok){
case eFAIL:
rc=0;
break;
case eOK:
rc=1;
break;
case eMSG:
rc=2;
break;
case eEndFAIL:
rc=3;
break;
case eEndOK:
rc=4;
break;
case eDONE:
rc=5; /* never used */
break;
}
rc=0;
#endif
    bool failedWhileSilent = self -> failedWhileSilent;
    bool print = false;
    char b [ 1024 ] = "";
    size_t num_writ = 0;
    size_t num_warn = 0;

    assert ( self );

    if ( ok != eMSG ) {
        if ( self -> ended ) {
            self -> crnt = self -> crnt -> parent;
            self -> n [ self -> level -- ] = 0;
        }
        else {
            self -> ended = true;
            ++ self -> total;
            if ( ok == eFAIL || ok == eEndFAIL )
                ++ self -> failures;
            else if ( ok == eWarning )
                ++ self -> warnings;
        }
    }

    assert ( self -> level >= 0 );
#ifdef DEPURAR
const char*c=self->msg.base;
#endif
    if ( ok == eWarning )
        rc = string_printf ( b, sizeof b, & num_warn, "WARNING: " );
    else if ( ok == eEndFAIL )
        rc = string_printf ( b, sizeof b, & num_warn, "FAILURE: " );
    rc = string_vprintf ( b + num_warn, sizeof b - num_warn,
                          & num_writ, fmt, args );
    num_writ += num_warn;
    if ( rc != 0 ) {
        LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R", rc );
        return rc;
    }

    if ( self -> crnt -> message == NULL )
        self -> crnt -> message = string_dup_measure ( b, NULL );
    else {
        size_t m = string_measure ( self -> crnt -> message, NULL);
        size_t s = m + num_writ + 1;
        char * tmp = realloc ( self -> crnt -> message, s );
        if ( tmp == NULL )
            return RC ( rcRuntime,
                        rcData, rcAllocating, rcMemory, rcExhausted );
        self -> crnt -> message = tmp;
        rc = string_printf ( self -> crnt -> message + m, s, NULL, b );
        assert ( rc == 0 );
    }
    if ( ok == eOK ) {
        free ( self -> crnt -> message );
        self -> crnt -> message = string_dup_measure ( "OK", NULL );
    }

    if ( ok == eEndFAIL || ok == eMSG ) {
        rc = KDataBufferPrintf ( & self -> msg, b );
        if ( rc != 0 )
            LogOut ( KVERBOSITY_ERROR, 0, "CANNOT PRINT: %R", rc );
        else if ( ok == eEndFAIL ) {
            const KDiagnoseError * e = NULL;
            rc = KDiagnoseErrorMake ( & e, self -> msg . base );
            if ( rc != 0 )
                return rc;
            rc = VectorAppend ( self -> errors, NULL, e );
            if ( rc != 0 ) {
                LogOut ( KVERBOSITY_ERROR, 0, "CANNOT rcRuntime: %R", rc );
                return rc;
            }
        }
    }

    if ( self -> level > self -> verbosity ) {
        if ( ok == eEndFAIL || ok == eMSG ) {
            if ( ok == eEndFAIL ) {
                rc = KDataBufferPrintf ( & self -> msg, "\n" );
                if ( self -> started ) {
                    LogOut ( KVERBOSITY_ERROR, 0,  "\n" );
                    self -> failedWhileSilent = true;
                    self -> started = false;
                }
                if ( self -> level >= KVERBOSITY_ERROR )
                    LogOut ( KVERBOSITY_ERROR, 0, self -> msg . base );
                assert ( self -> msg . base );
                ( ( char * ) self -> msg . base)  [ 0 ] = '\0';
                self -> msg . elem_count = 0;
            }
        }
    }
    else {
        print = self -> level < self -> verbosity || failedWhileSilent;
        if ( ok == eFAIL || ok == eOK || ok == eDONE || ok == eCANCELED) {
            if ( print ) {
                int i = 0;
                rc = LogOut ( self -> level, 0, "< %d", self -> n [ 0 ] );
                for ( i = 1; i <= self -> level; ++ i )
                    rc = LogOut ( self -> level, 0, ".%d", self -> n [ i ] );
                rc = LogOut ( self -> level, 0, " " );
            }
        }
        if ( print ||
                ( self -> level == self -> verbosity &&
                  ok != eFAIL && ok != eOK ) )
        {
            rc = LogOut ( self -> level, 0, b );
        }

        if ( print )
            switch ( ok ) {
                case eFAIL: rc = LogOut ( self -> level, 0, ": FAILURE\n" );
                            break;
                case eOK  : rc = LogOut ( self -> level, 0, ": OK\n"      );
                            break;
                case eCANCELED:
                case eEndFAIL:
                case eEndOK :
                case eWarning:
                case eDONE: rc = LogOut ( self -> level, 0, "\n"      );
                            break;
                default   : break;
            }
        else if ( self -> level == self -> verbosity )
            switch ( ok ) {
                case eFAIL: rc = LogOut ( self -> level, 0, "FAILURE\n" );
                            break;
                case eOK  : rc = LogOut ( self -> level, 0, "OK\n"      );
                            break;
                case eEndFAIL:
                case eEndOK :
                case eWarning:
                case eDONE: rc = LogOut ( self -> level, 0, "\n"      );
                            break;
                default   : break;
            }

        self -> failedWhileSilent = false;
    }

    if (  ok != eMSG ) {
        EKDiagTestState state = eKDTS_Succeed;
        switch ( ok ) {
            case eEndOK   : state = eKDTS_Succeed ; break;
            case eOK      : state = eKDTS_Succeed ; break;
            case eFAIL    : state = eKDTS_Failed  ; break;
            case eCANCELED: state = eKDTS_Canceled; break;
            case eWarning : state = eKDTS_Warning ; break;
            default       : state = eKDTS_Failed  ; break;
        }
        self -> crnt -> state = state;
        if ( CALL_BACK )
            CALL_BACK ( state, self -> crnt, CALL_BACK_DATA );
    }

    if ( rc == 0 ) {
        assert ( self -> dad );
        rc = KDiagnoseCheckState ( self -> dad );
    }

    return rc;
}

static bool _RcCanceled ( rc_t rc ) {
    return rc == SILENT_RC ( rcExe,
                             rcProcess, rcExecuting, rcProcess, rcCanceled )
        || rc == SILENT_RC ( rcRuntime,
                             rcProcess, rcExecuting, rcProcess, rcCanceled );
}
static bool STestCanceled ( const STest * self, rc_t rc ) {
    assert ( self && self -> dad );
    return _RcCanceled ( rc ) && self -> dad -> state == eCanceled;
}

static rc_t STestFailure ( const STest * self ) {
    rc_t failure = 0;

    rc_t rc = RC ( rcRuntime, rcProcess, rcExecuting, rcProcess, rcCanceled );
    const KConfigNode * node = NULL;

    assert ( self && self -> crnt && self -> crnt -> numberNode );

    rc = KConfigOpenNodeRead ( self -> kfg, & node,
        "tools/test-sra/diagnose/%s", self -> crnt -> numberNode );
    if ( rc == 0 ) {
        uint64_t result = 0;
        rc = KConfigNodeReadU64 ( node, & result );
        if ( rc == 0 ) {
            failure = ( rc_t ) result;
// if ( _RcCanceled ( failure ) && CALL_BACK ) CALL_BACK ( eKDTS_Canceled,NULL);
        }

        KConfigNodeRelease ( node );
        node = NULL;
    }

    return failure;
}

static rc_t STestEndOr ( STest * self, rc_t * failure,
                         EOK ok, const char * fmt, ...  )
{
    rc_t rc = 0;
    bool canceled = false;

    va_list args;
    va_start ( args, fmt );

    assert ( failure );
    * failure = 0;

    rc = STestVEnd ( self, ok, fmt, args );
    canceled = STestCanceled ( self, rc );

    va_end ( args );

    if ( LOGGER == OutMsg ) {
        assert ( rc == 0 || canceled );
    }

    if ( canceled )
        * failure = rc;
    else if ( rc == 0 )
        * failure = STestFailure ( self );

    return rc;
}

static rc_t STestEnd ( STest * self, EOK ok, const char * fmt, ...  ) {
    rc_t rc = 0;

    va_list args;
    va_start ( args, fmt );

    rc = STestVEnd ( self, ok, fmt, args );

    va_end ( args );

    if ( LOGGER == OutMsg ) {
        assert ( rc == 0 || STestCanceled ( self, rc ) );
    }

    return rc;
}

static rc_t STestStart ( STest * self, bool checking, uint64_t code,
                         const char * fmt, ...  )
{
    rc_t rc = 0;

    va_list args;
    va_start ( args, fmt );

    rc = STestVStart ( self, checking, code, fmt, args );

    va_end ( args );

    if ( LOGGER == OutMsg ) {
        assert ( rc == 0 );
    }

    return rc;
}

static rc_t STestFail ( STest * self, rc_t failure, uint64_t code,
                        const char * start,  ...  )
{
    va_list args;

    rc_t rc = 0;

    rc_t r2 = 0;

    va_start ( args, start );

    rc = STestVStart ( self, false, code, start, args );

    r2 = STestEnd ( self, eEndFAIL, "%R", failure );
    if ( rc == 0 && r2 != 0 )
        rc = r2;

    va_end ( args );

    return rc;
}

typedef struct {
    VPath * vpath;
    const String * acc;
} Data;

static rc_t DataInit ( Data * self, const VFSManager * mgr,
                       const char * path )
{
    rc_t rc = 0;

    assert ( self );

    memset ( self, 0, sizeof * self );

    rc = VFSManagerMakePath ( mgr, & self -> vpath, path );
    if ( rc != 0 )
        LogOut ( KVERBOSITY_ERROR, 0,
                 "VFSManagerMakePath(%s) = %R\n", path, rc );
    else {
        VPath * vacc = NULL;
        rc = VFSManagerExtractAccessionOrOID ( mgr, & vacc, self -> vpath );
        if ( rc != 0 )
            rc = 0;
        else {
            String acc;
            rc = VPathGetPath ( vacc, & acc );
            if ( rc == 0 )
                StringCopy ( & self -> acc, & acc );
            else
                LogOut ( KVERBOSITY_ERROR, 0, "Cannot VPathGetPath"
                           "(VFSManagerExtractAccessionOrOID(%R))\n", rc );
            RELEASE ( VPath, vacc );
        }
    }

    return rc;
}

static rc_t DataFini ( Data * self ) {
    rc_t rc = 0;

    assert ( self );

    free ( ( void * ) self -> acc );

    rc = VPathRelease ( self -> vpath );

    memset ( self, 0, sizeof * self );

    return rc;
}

static const ver_t HTTP_VERSION = 0x01010000;

static rc_t STestCheckFile ( STest * self, const String * path,
                             uint64_t * sz, rc_t * rc_read )
{
    rc_t rc = 0;

    const KFile * file = NULL;

    assert ( self && sz && rc_read );

    STestStart ( self, false, 0,
                 "KFile = KNSManagerMakeReliableHttpFile(%S):", path );

    rc = KNSManagerMakeReliableHttpFile ( self -> kmgr, & file, NULL,
                                          HTTP_VERSION, true, false, false, "%S", path );
    if ( rc != 0 )
        STestEnd ( self, eEndFAIL, "%R", rc );
    else {
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
        else {
            STestStart ( self, false, 0, "KFileSize(KFile(%S)) =", path );
            rc = KFileSize ( file, sz );
            if ( rc == 0 )
                STestEndOr ( self, & rc, eEndOK, "%lu: OK", * sz );
            if ( rc != 0 ) {
                if ( _RcCanceled ( rc ) )
                    STestEnd ( self, eCANCELED, "CANCELED" );
                else
                    STestEnd ( self, eEndFAIL, "%R", rc );
            }
        }
    }

    if ( rc == 0 ) {
        char buffer [ 304 ] = "";
        uint64_t pos = 0;
        size_t bsize = sizeof buffer;
        size_t num_read = 0;
        if ( * sz < 256 ) {
            pos = 0;
            bsize = ( size_t ) * sz;
        }
        else
            pos = ( * sz - sizeof buffer ) / 2;
        STestStart ( self, false, 0,
                    "KFileRead(%S,%lu,%zu):", path, pos, bsize );
        * rc_read = KFileRead ( file, pos, buffer, bsize, & num_read );
        if ( * rc_read == 0 )
            STestEndOr ( self, rc_read, eEndOK, "OK" );
        if ( * rc_read != 0 ) {
            if ( _RcCanceled ( * rc_read ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", * rc_read );
        }
    }

    KFileRelease ( file );
    file = NULL;

    return rc;
}

static
rc_t STestCheckRanges ( STest * self, const Data * data, uint64_t sz )
{
    rc_t rc = 0;
    uint64_t pos = 0;
    size_t bytes = 4096;
    uint64_t ebytes = bytes;
    bool https = false;
    char buffer [ 2048 ] = "";
    size_t num_read = 0;
    KClientHttp * http = NULL;
    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;
    String host;
    String scheme;
    assert ( self && data );
    STestStart ( self, true, 0, "Support of Range requests" );
    rc = VPathGetHost ( data -> vpath, & host );
    if ( rc != 0 )
        STestFail ( self, rc, 0, "VPathGetHost" );
    if ( rc == 0 )
        rc = VPathGetScheme ( data -> vpath, & scheme );
    if ( rc != 0 )
        STestFail ( self, rc, 0, "VPathGetScheme" );
    if ( rc == 0 ) {
        String sHttps;
        String sHttp;
        CONST_STRING ( & sHttp, "http" );
        CONST_STRING ( & sHttps, "https" );
        if ( StringEqual ( & scheme, & sHttps ) )
            https = true;
        else if ( StringEqual ( & scheme, & sHttp ) )
            https = false;
        else {
            LogOut ( KVERBOSITY_ERROR, 0,
                     "Unexpected scheme '(%S)'\n", & scheme );
            return 0;
        }
    }
    if ( rc == 0 ) {
        if ( https ) {
            STestStart ( self, false, 0, "KClientHttp = "
                         "KNSManagerMakeClientHttps(%S):", & host );
            rc = KNSManagerMakeClientHttps ( self -> kmgr, & http, NULL,
                                             HTTP_VERSION, & host, 0 );
        }
        else {
            STestStart ( self, false, 0, "KClientHttp = "
                         "KNSManagerMakeClientHttp(%S):", & host );
            rc = KNSManagerMakeClientHttp ( self -> kmgr, & http, NULL,
                                            HTTP_VERSION, & host, 0 );
        }
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        String path;
        rc = VPathGetPath ( data -> vpath, & path );
        if ( rc != 0 )
            STestFail ( self, rc, 0, "VPathGetPath" );
        else {
            rc = KHttpMakeRequest ( http, & req, "%S", & path );
            if ( rc != 0 ) {
                self -> dad -> report . firewall = true;
                STestFail ( self, rc, 0, "KHttpMakeRequest(%S)", & path );
            }
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0, "KHttpResult = "
            "KHttpRequestHEAD(KHttpMakeRequest(KClientHttp)):" );
        rc = KHttpRequestHEAD ( req, & rslt );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
                     "KHttpResultGetHeader(KHttpResult, Accept-Ranges) =" );
        rc = KHttpResultGetHeader ( rslt, "Accept-Ranges",
                                    buffer, sizeof buffer, & num_read );
        if ( rc == 0 ) {
            const char bytes [] = "bytes";
            if ( string_cmp ( buffer, num_read, bytes, sizeof bytes - 1,
                              sizeof bytes - 1 ) == 0 )
            {
                rc = STestEnd ( self, eEndOK, "'%.*s': OK",
                                        ( int ) num_read, buffer );
            }
            else {
                STestEnd ( self, eEndFAIL, "'%.*s'", ( int ) num_read, buffer );
                rc = RC ( rcRuntime,
                          rcFile, rcOpening, rcFunction, rcUnsupported );
            }
        }
        else
            STestEnd ( self, eEndFAIL, "%R", rc );
    }
    KHttpResultRelease ( rslt );
    rslt = NULL;
    if ( sz < ebytes )
        ebytes = sz;
    if ( sz > bytes * 2 )
        pos = sz / 2;
    if ( rc == 0 ) {
        STestStart ( self, false, 0, "KHttpResult = KHttpRequestByteRange"
                        "(KHttpMakeRequest, %lu, %zu):", pos, bytes );
        rc = KHttpRequestByteRange ( req, pos, bytes );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
            "KHttpResult = KHttpRequestGET(KHttpMakeRequest(KClientHttp)):" );
        rc = KHttpRequestGET ( req, & rslt );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        uint64_t po = 0;
        size_t byte = 0;
        rc = KClientHttpResultRange ( rslt, & po, & byte );
        if ( rc == 0 ) {
            if ( po != pos || ( ebytes > 0 && byte != ebytes ) ) {
                rc = RC ( rcRuntime, rcFile, rcReading, rcRange, rcOutofrange );
                STestFail ( self, rc, 0,
                    "KClientHttpResultRange(KHttpResult,&p,&b): "
                    "got:{%lu,%zu}", pos, ebytes, po, byte );
/*              STestStart ( self, false,
                             "KClientHttpResultRange(KHttpResult,&p,&b):" );
                STestEnd ( self, eEndFAIL, "FAILURE: expected:{%lu,%zu}, "
                            "got:{%lu,%zu}", pos, ebytes, po, byte );*/
            }
        }
        else {
            STestFail ( self, rc, 0, "KClientHttpResultRange(KHttpResult)" );
/*          STestStart ( self, false, "KClientHttpResultRange(KHttpResult):" );
            STestEnd ( self, eEndFAIL, "FAILURE: %R", rc );*/
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
                     "KHttpResultGetHeader(KHttpResult, Content-Range) =" );
        rc = KHttpResultGetHeader ( rslt, "Content-Range",
                                    buffer, sizeof buffer, & num_read );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "'%.*s': OK",
                                    ( int ) num_read, buffer );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    KHttpResultRelease ( rslt );
    rslt = NULL;
    KHttpRequestRelease ( req );
    req = NULL;
    KHttpRelease ( http );
    http = NULL;

    if ( rc == 0 ) {
        rc_t r2 = STestEnd ( self, eOK, "Support of Range requests" );
        if ( r2 != 0 && rc == 0 )
            rc = r2;
    }
    else if ( _RcCanceled ( rc ) )
        STestEnd ( self, eCANCELED, "Support of Range requests: CANCELED" );
    else
        STestEnd ( self, eFAIL, "Support of Range requests" );

    return rc;
}

static KPathType STestRemoveCache ( STest * self, const char * cache ) {
    KPathType type = kptNotFound;

    assert ( self );

    type = KDirectoryPathType ( self -> dir, cache );

    if ( type != kptNotFound ) {
        if ( ( type & ~ kptAlias ) == kptFile ) {
            rc_t rc = KDirectoryRemove ( self -> dir, false, cache );
            if ( rc != 0 )
                STestFail ( self, rc, 0, "KDirectoryRemove(%s)", cache );
            else
                type = kptNotFound;
        }
        else
            LogOut ( KVERBOSITY_ERROR, 0,
                     "UNEXPECTED FILE TYPE OF '%s': %d\n", cache, type );
    }

    return type;
}

static rc_t STestCheckStreamRead ( STest * self, const KStream * stream,
    const char * cache, uint64_t * cacheSize, uint64_t sz, bool print,
    const char ** exp, size_t esz, bool * tooBig )
{
    rc_t rc = 0;
    size_t total = 0;
    char buffer [ 1024 ] = "";
    KFile * out = NULL;
    rc_t rw = 0;
    uint64_t pos = 0;
    assert ( cache && cacheSize && tooBig );
    if ( cache [ 0 ] != '\0' ) {
        if ( STestRemoveCache ( self, cache ) == kptNotFound ) {
            rw = KDirectoryCreateFile ( self -> dir, & out, false,  0664,
                                        kcmCreate | kcmParents, cache );
            if ( rw != 0 )
                LogOut ( KVERBOSITY_ERROR, 0,
                         "CANNOT CreateFile '%s': %R\n", cache, rw );
        }
    }
    STestStart ( self, false, 0, "KStreamRead(KHttpResult):" );
    while ( rc == 0 ) {
        size_t num_read = 0;
        rc = KStreamRead ( stream, buffer, sizeof buffer, & num_read );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, "%R", rc );
        else if ( num_read != 0 ) {
            if ( rw == 0 && out != NULL ) {
                size_t num_writ = 0;
                rw = KFileWriteAll ( out, pos, buffer, num_read, & num_writ );
                if ( rw == 0 ) {
                    assert ( num_writ == num_read );
                    pos += num_writ;
                }
                else
                    LogOut ( KVERBOSITY_ERROR, 0,
                             "CANNOT WRITE TO '%s': %R\n", cache, rw );
            }
            if ( total == 0 && esz > 0 ) {
                int i = 0;
                size_t s = esz;
                if ( num_read < esz )
                    s = num_read;
                rc = STestEnd ( self, eMSG, "'" );
                if ( rc != 0 ) {
                    if ( STestCanceled ( self, rc ) )
                        STestEnd ( self, eCANCELED, "CANCELED" );
                    else
                        STestEnd ( self, eEndFAIL, "%R", rc );
                    break;
                }
                for ( i = 0; i < s && rc == 0; ++ i ) {
                    if ( isprint ( ( unsigned char ) buffer [ i ] ) )
                        rc = STestEnd ( self, eMSG, "%c", buffer [ i ] );
                    else if ( buffer [ i ] == 0 )
                        rc = STestEnd ( self, eMSG, "\\0" );
                    else
                        rc = STestEnd ( self, eMSG, "\\%03o",
                                               ( unsigned char ) buffer [ i ] );
                }
                if ( rc == 0 )
                    rc = STestEnd ( self, eMSG, "': " );
                if ( rc != 0 ) {
                    if ( STestCanceled ( self, rc ) )
                        STestEnd ( self, eCANCELED, "CANCELED" );
                    else
                        STestEnd ( self, eEndFAIL, "%R", rc );
                    break;
                }
                for ( i = 0; i < 2; ++ i ) {
                    if ( string_cmp ( buffer, num_read, exp [ i ], esz,
                                      (uint32_t) esz ) == 0 )
                    {   break; }
                    else if ( i == 1 ) {
                        STestEnd ( self, eEndFAIL, "bad content" );
                        rc = RC ( rcRuntime,
                                  rcFile, rcReading, rcString, rcUnequal );
                    }
                }
            }
            total += num_read;
            if ( total > 1000000 /* 1mb */ ) {
                * tooBig = true;
                rc = STestEnd ( self, eEndOK,
                                "Interrupted (file is too big): OK" );
                assert ( total < sz );
                break;
            }
        }
        else {
            assert ( num_read == 0 );
            if ( total == sz ) {
                if ( print ) {
                    if ( total >= sizeof buffer )
                        buffer [ sizeof buffer - 1 ] = '\0';
                    else {
                        buffer [ total ] = '\0';
                        while ( total > 0 ) {
                            -- total;
                            if ( buffer [ total ] == '\n' )
                                buffer [ total ] = '\0';
                            else
                                break;
                        }
                    }
                    rc = STestEnd ( self, eMSG, "%s: ", buffer );
                }
                if ( rc == 0 )
                    rc = STestEnd ( self, eEndOK, "OK" );
                if ( rc != 0 ) {
                    if ( STestCanceled ( self, rc ) )
                        STestEnd ( self, eCANCELED, "CANCELED" );
                    else
                        STestEnd ( self, eEndFAIL, "%R", rc );
                }
            }
            else
                STestEnd ( self, eEndFAIL, "%s: SIZE DO NOT MATCH (%zu)\n",
                                           total );
            break;
        }
    }
    {
        rc_t r2 = KFileRelease ( out );
        if ( rw == 0 )
            rw = r2;
    }
    if ( rw == 0 )
        * cacheSize = pos;
    return rc;
}

static rc_t STestCheckHttpUrl ( STest * self, uint64_t tests, uint64_t atest,
    const Data * data, const char * cache, uint64_t * cacheSize,
    bool print, const char ** exp, size_t esz, bool * tooBig )
{
    rc_t rc = 0;
    rc_t rc_read = 0;
    rc_t r2 = 0;
    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;
    const String * full = NULL;
    uint64_t sz = 0;
    assert ( self && data );
    rc = VPathMakeString ( data -> vpath, & full );
    if ( rc != 0 )
        STestFail ( self, rc, 0, "VPathMakeString" );
    if ( rc == 0 )
        STestStart ( self, true, atest, "Access to '%S'", full );
    if ( rc == 0 )
        rc = STestCheckFile ( self, full, & sz, & rc_read );
    r2 = STestCheckRanges ( self, data, sz );
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
                     "KHttpRequest = KNSManagerMakeRequest(%S):", full );
        rc = KNSManagerMakeRequest ( self -> kmgr, & req,
                                     HTTP_VERSION, NULL, "%S", full );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK"  );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else {
                self -> dad -> report . firewall = true;
                STestEnd ( self, eEndFAIL, "%R", rc );
            }
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
                     "KHttpResult = KHttpRequestGET(KHttpRequest):" );
        rc = KHttpRequestGET ( req, & rslt );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK" );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        uint32_t code = 0;
        STestStart ( self, false, 0, "KHttpResultStatus(KHttpResult) =" );
        rc = KHttpResultStatus ( rslt, & code, NULL, 0, NULL );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, "%R", rc );
        else {
            rc = STestEnd ( self, eMSG, "%u: ", code );
            if ( rc == 0 )
                if ( code == 200 )
                    STestEnd ( self, eEndOK, "OK" );
                else {
                    STestEnd ( self, eEndFAIL, "bad status" );
                    rc = RC ( rcRuntime, rcFile, rcReading, rcFile, rcInvalid );
                }
            else  if ( STestCanceled ( self, rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 && tests & KDIAGN_DOWNLOAD_HTTP ) {
        KStream * stream = NULL;
        rc = KHttpResultGetInputStream ( rslt, & stream );
        if ( rc != 0 )
            STestFail ( self, rc, KDIAGN_DOWNLOAD_HTTP,
                        "KHttpResultGetInputStream(KHttpResult)" );
        else
            rc = STestCheckStreamRead ( self, stream, cache, cacheSize,
                                        sz, print, exp, esz, tooBig );
        KStreamRelease ( stream );
        stream = NULL;
    }
    if ( rc == 0 && r2 != 0 )
        rc = r2;
    if ( rc == 0 && rc_read != 0 )
        rc = rc_read;
    KHttpRequestRelease ( req );
    req = NULL;
    KHttpResultRelease ( rslt );
    rslt = NULL;

    if ( rc == 0 )
        STestEnd ( self, eOK, "Access to '%S'", full );
    else if ( _RcCanceled ( rc ) )
        STestEnd ( self, eCANCELED, "Access to '%S': CANCELED", full );
    else
        STestEnd ( self, eFAIL, "Access to '%S'", full );

    free ( ( void * ) full );
    full = NULL;
    return rc;
}

static bool DataIsAccession ( const Data * self ) {
    assert ( self );

    if ( self -> acc == NULL )
        return false;
    else
        return self -> acc -> size != 0;
}

static rc_t STestCheckVfsUrl ( STest * self, uint64_t atest, const Data * data,
                               bool warn )
{
    rc_t rc = 0;

    const KDirectory * d = NULL;

    String path;

    assert ( self && data );

    if ( ! DataIsAccession ( data ) )
        return 0;

    rc = VPathGetPath ( data -> vpath, & path );
    if ( rc != 0 ) {
        STestFail ( self, rc, 0, "VPathGetPath" );
        return rc;
    }

    STestStart ( self, false, atest, "VFSManagerOpenDirectoryReadDecrypt(%S):",
                                                                  & path );
    rc = VFSManagerOpenDirectoryReadDecrypt ( self -> vmgr, & d,
                                              data -> vpath );
    if ( rc == 0 )
        STestEndOr ( self, & rc, eEndOK, "OK"  );
    if ( rc != 0 ) {
        if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "CANCELED" );
        else
            STestEnd ( self, eEndFAIL, "%R", rc );
    }

    RELEASE ( KDirectory, d );

    return rc;
}

static rc_t STestCheckUrlImpl ( STest * self, uint64_t tests, uint64_t htest,
    uint64_t vtest,  const Data * data, const char * cache,
    uint64_t * cacheSize, bool print, const char ** exp, size_t esz,
    bool * tooBig )
{
    rc_t rc = 0;
    rc_t r2 = 0;
    if ( tests & htest )
        rc = STestCheckHttpUrl ( self, tests, htest, data,
                                 cache, cacheSize, print, exp, esz, tooBig );
    if ( tests & vtest )
        r2 = STestCheckVfsUrl  ( self, vtest, data, cacheSize == 0 );
    return rc != 0 ? rc : r2;
}

static rc_t STestCheckUrl ( STest * self, uint64_t tests, uint64_t htest,
    uint64_t vtest, const Data * data, const char * cache, uint64_t * cacheSize,
    bool print, const char ** exp, size_t esz, bool * tooBig )
{
    rc_t rc = 0;

    String path;

    bool dummy;
    if ( tooBig == NULL )
        tooBig = & dummy;

    assert ( data );

    rc = VPathGetPath ( data -> vpath, & path );
    if ( rc != 0 ) {
        STestFail ( self, rc, 0, "VPathGetPath" );
        return rc;
    }

    if ( path . size == 0 ) /* does not exist */
        return 0;

    return STestCheckUrlImpl ( self, tests, htest, vtest, data,
                               cache, cacheSize, print, exp, esz, tooBig );
}

static String * KConfig_Resolver ( const KConfig * self ) {
    String * s = NULL;

    rc_t rc = KConfigReadString ( self,
                                  "tools/test-sra/diagnose/resolver-cgi", & s );
    if ( rc != 0 ) {
        String str;
        CONST_STRING ( & str,
                       "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi" );
        rc = StringCopy ( ( const String ** ) & s, & str );
        assert ( rc == 0 );
    }

    assert ( s );
    return s;
}

static int KConfig_Verbosity ( const KConfig * self ) {
    int64_t v = -1;

    String * s = NULL;
    rc_t rc = KConfigReadString ( self,
                                  "tools/test-sra/diagnose/verbosity", & s );
    if ( rc != 0 )
        return 0;

    assert ( s );

    if ( s -> size > 0 )
        if ( isdigit ( s -> addr [ 0 ] ) )
            v = strtoi64 ( s -> addr, NULL, 0 );

    free ( s );
    s = NULL;

    return ( int ) v;
}

static rc_t STestAbuseRedirect ( const STest * self,
                                 Abuse * test, bool * abuse )
{
    rc_t rc = 0;

    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;
    KStream * stream = NULL;

    size_t i = 0;
    size_t total = 0;
    char * base = NULL;
    const char * c = NULL;

    Block * p = NULL;
    KDataBuffer * buffer = NULL;

    String open, close, needle;
    CONST_STRING ( & open,   "<p>" );
    CONST_STRING ( & close, "</p>" );

    assert ( self && test && abuse );

    buffer = & test -> redirect;
    p = & test -> p;

    if ( rc == 0 )
        rc = KNSManagerMakeRequest ( self -> kmgr, & req, HTTP_VERSION,
                                     NULL, test -> location );
    if ( rc == 0 )
        rc = KHttpRequestGET ( req, & rslt );

    if ( rc == 0 )
        rc = KHttpResultGetInputStream ( rslt, & stream );

#if 0
if(false){
char*c="<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\"> \n<html>\n<head>\n<title>NCBI - WWW Error Blocked Diagnostic</title>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n<meta http-equiv=\"pragma\" content=\"no-cache\" />\n<meta http-equiv=\"cache-control\" content=\"no-cache\" />\n<meta name=\"robots\" content=\"noarchive,none\" />\n\n<style type=\"text/css\">\nbody {\nmin-width: 950px;\n_width: 950px;\n}\nh1.error {color: red; font-size: 40pt}\n\n#mainContent {\npadding: 1em 2em;\n}\n\ndl#diags {\npadding: 0.5em 0.5em 1.5em 0.5em;\npadding-left: 2em;\nborder: solid 1px #888;\nborder-left: none;\nborder-right: none;\nmargin-bottom:0;\nbackground-color:#eeeeee;\ncolor:#666;\nfont-size: 80%;\n_font-size: 70%;\nfont-family: Verdana, sans-serif;\n}\n\ndl#diags dt {\nfloat: left;\nfont-weight: bold;\nwidth: auto;\n}\n\ndl#diags dd {\nmargin-left:1em;\nfloat: left;\nmargin-right: 2em;\n}\n\n#footer span {\nfloat: left;\ncolor: #888;\nfont-size: 80%;\n}\n\n#footer {\ntext-align: right;\npadding: 0 0.5em;\nclear: left;\n}\n\n#footer img {\nborder: none;\n}\n\n.ncbi {\nmargin: 0;\npadding:0;\nfont-size:240%;\nfont-weight: bold;\nfont-family: Times, serif;\ncolor: #336699;\nfloat: left;\ndisplay: inline;\n}\n\n.ncbi a {\ntext-decoration: none;\ncolor: #336699;\n}\n\n.ncbi a:visited {\ntext-decoration: none;\ncolor: #336699;\n}\n\n.ncbi a:hover {\ntext-decoration: underline;\n}\n\n.message {\nfont-family: Verdana, sans-serif;\nbackground-color: #336699;\ncolor: white;\npadding: .35em;\nmargin-left: 7em;\nmargin-top: .67em;\n_margin-top: 0.5em;\nfont-weight: bold;\nfont-size: 100%;\nmargin-bottom: 0;\n}\n\nh1 {\nclear: left;\nfont-size: 110%;\nfont-family: Verdana, sans-serif;\n}\n\n\nbody.denied {\nbackground-color: black;\ncolor: white;\n}\n\nbody.denied h1 {\ncolor: red;\n}\n\nbody.denied a {\ncolor: green;\n}\n\nbody.denied #footer, body.denied #diags {\ncolor: black;\n}\n\n#searchme:focus {\nbackground-color: #ffa;\n}\n\n.errurl {\nletter-spacing: 0;\nmargin: 0 1em;\npadding: 0.25em;\nbackground-color: #fff0f0;\ncolor: #c00;\nfont-family: \"Courier New\", Courier, monospace;\nfont-size: 90%;\n_font-size: 80%;\n}\n\nbody.denied .errurl {\nbackground-color: black;\ncolor: yellow;\n}\n\nspan.x {\ndisplay: none;\n}\n\n</style>\n\n</head>\n<body class='denied'>\n\n\n\n\n\n\n<div id='header'>\n<a href=\"#mainContent\" title=\"Skip to main content\" />\n<p class=\"ncbi\"><a href=\"http://www.ncbi.nlm.nih.gov\">NCBI</a></p>\n<p class=\"message\">Error</p>\n</div>\n\n<div id='mainContent'>\n\n\n<h1 class=\"error\">Access Denied</h1>\n\n\n<p>\nYour access to the NCBI website at <b>www.ncbi.nlm.nih.gov</b> has been\ntemporarily blocked due to a possible misuse/abuse situation\ninvolving your site. This is not an indication of a security issue\nsuch as a virus or attack. It could be something as simple as a run\naway script or learning how to better use E-utilities,\n<a href=\"http://www.ncbi.nlm.nih.gov/books/NBK25497/\">http://www.ncbi.nlm.nih.gov/books/NBK25497/</a>,\nfor more efficient work such that your work does not impact the ability of other researchers\nto also use our site.\nTo restore access and understand how to better interact with our site\nto avoid this in the future, please have your system administrator\ncontact \n<a href=\"mailto:info@ncbi.nlm.nih.gov?subject=NCBI Web site BLOCKED: 12.34.567.890&amp;body=%3E%20Error%3Dblocked for possible abuse%0D%3E%20Server%3Dmisuse.ncbi.nlm.nih.gov%0D%3E%20Client%3D12.34.567.890%0D%3E%20Time%3DTuesday, 03-Apr-2018 13:30:17 EDT  %0D%0DPlease%20enter%20comments%20below:%0D%0D\">info@ncbi.nlm.nih.gov</a>.\n</p>\n\n</div>\n\n<dl id='diags'>\n\n<dt>Error</dt><dd>blocked for possible abuse </dd>\n<dt>Server</dt><dd>misuse.ncbi.nlm.nih.gov</dd>\n<dt>Client</dt><dd>12.34.567.890</dd>\n<dt>Time</dt><dd>Tuesday, 03-Apr-2018 13:30:17 EDT</dd>\n\n</dl>\n \n\n<p id='footer'>\n<span id='rev'>Rev. 05/18/15</span>\n</p>\n\n</body>\n</html>\n\n";
size_t s = string_size ( c );KDataBufferMakeBytes ( buffer, s + 9 );
memmove(buffer->base,c,s);base = buffer -> base;
buffer->elem_count=s;
}
else
#endif
    while ( rc == 0 )
    {
        size_t num_read = 0;

        uint64_t avail = buffer -> elem_count - total;
        if ( avail == 0 ) {
            rc = KDataBufferResize ( buffer, buffer -> elem_count + 1024 );
            if ( rc != 0 )
                break;
        }

        base = buffer -> base;
        rc = KStreamRead ( stream, & base [ total ],
            ( size_t ) buffer -> elem_count - total, & num_read );
        if ( num_read == 0 ) {
            buffer -> elem_count = total;
            break;
        }
        if ( rc != 0 ) /* TBD - look more closely at rc */
            rc = 0;

        total += num_read;
    }

    if ( rc == 0 ) {
        p -> begin = find ( base + i, buffer -> elem_count - i, & open );
        if ( p -> begin != NULL ) {
            i = p -> begin - base;
            p -> end = find ( base + i, buffer -> elem_count - i, & close );
            if ( p -> end != NULL )
                * ( p -> end ) = '\0';
        }
    }

    if ( p -> end != NULL ) {
        CONST_STRING ( & needle, "<a href=\"mailto:" );
        test -> mailto . begin = find ( p -> begin, p -> end - p -> begin,
                                        & needle );
        if ( test -> mailto . begin != NULL ) {
            CONST_STRING ( & needle, "</a>" );
            test -> mailto . end = find ( test -> mailto . begin,
                p -> end - test -> mailto . begin, & needle );
        }
    }

    if ( test -> mailto . end != NULL ) {
        CONST_STRING ( & needle, "Client%3D" );
        test -> ip . addr = find ( test -> mailto . begin,
            test -> mailto . end - test -> mailto . begin, & needle );
        if ( test -> ip . addr == NULL )
            test -> ip . addr = "";
        else {
            test -> ip . addr += needle . size;
            c = string_chr ( test -> ip . addr,
                             test -> mailto . end - test -> ip . addr, '%' );
            if ( c != NULL )
                test -> ip . len = test -> ip . size = c - test -> ip . addr;
        }
    }

    if ( test -> mailto . end != NULL ) {
        CONST_STRING ( & needle, "Time%3D" );
        test -> date . addr = find ( test -> mailto . begin,
            test -> mailto . end - test -> mailto . begin, & needle );
        if ( test -> date . addr == NULL )
            test -> date . addr = "";
        else {
            test -> date . addr += needle . size;
            c = string_chr ( test -> date . addr,
                             test -> mailto . end - test -> date . addr, '%' );
            if ( c != NULL )
                test -> date . len = test -> date . size
                                   = c - test -> date . addr;
        }
    }

    * abuse = true;

    RELEASE ( KStream, stream );
    RELEASE ( KHttpResult, rslt );
    RELEASE ( KHttpRequest, req );

    return rc;
}

static rc_t STestAbuse ( STest * self, Abuse * test,
                         bool * ok, bool * abuse )
{
    size_t i = 0;
    const char * s = NULL;
    const char * h;

    String misuse;
    CONST_STRING ( & misuse,
        "https://misuse.ncbi.nlm.nih.gov/error/abuse.shtml" );

    assert ( test && ok && abuse );

    * ok = * abuse = false;

    if ( test -> code == 200 ) {
        * ok = true;
        return 0;
    }

    if ( test -> code != 302 )
        return 0;

    if ( test -> location != NULL )
        return STestAbuseRedirect ( self, test, abuse );

    s = test -> response . base;
    h = find ( s + i, test -> response . elem_count - i,
                            & misuse );
    if ( h != NULL ) {
        rc_t rc = AbuseSetLocation ( test, misuse . addr, misuse . size );
        return rc == 0 ? STestAbuseRedirect ( self, test, abuse ) : rc;
    }

    return 0;
}

static char * processResponse ( char * response, size_t size ) {
    int n = 0;

    size_t i = 0;
    for ( i = 0; i < size; ++ n ) {
        const char * p = string_chr ( response + i, size - i, '|' );
        if ( p == NULL )
            return NULL;

        i = p - response + 1;

        if ( n == 5 ) {
            for ( ; response [ i ] != '|' && i < size; ++i )
                response [ i ] = 'x';

            return response [ i ] == '|' ? response + i + 1 : NULL;
        }
    }

    return NULL;
}

static rc_t STestCallCgi ( STest * self, uint64_t atest, const String * acc,
    char * response, size_t response_sz, size_t * resp_read,
    const char ** url, bool http )
{
    rc_t rc = 0;

    rc_t rs = 0;
    KHttpRequest * req = NULL;
    const String * cgi = NULL;
    KHttpResult * rslt = NULL;
    KStream * stream = NULL;
    Abuse * test = NULL;

    assert ( url && self && self -> dad );

    test = & self -> dad -> report . abuse;

    STestStart ( self, true, atest,
                 "Resolving of %s path to '%S'", http ? "HTTPS": "FASP", acc );

    * url = NULL;

    cgi = KConfig_Resolver ( self -> kfg );
    AbuseSetCgi ( test, cgi );
    STestStart ( self, false, 0,
        "KHttpRequest = KNSManagerMakeReliableClientRequest(%S):", cgi );
    rc = KNSManagerMakeReliableClientRequest ( self -> kmgr, & req,
        HTTP_VERSION, NULL, "%S", cgi);
    if ( rc == 0 )
        STestEndOr ( self, & rc, eEndOK, "OK"  );
    if ( rc != 0 ) {
        if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "CANCELED" );
        else {
            self -> dad -> report . firewall = true;
            STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        const char param [] = "accept-proto";
        const char * v = "http,fasp";
        if ( http )
            v = "http,https";
        rc = KHttpRequestAddPostParam ( req, "%s=%s", param, v );
        if ( rc != 0 )
            STestFail ( self, rc, 0,
                "KHttpRequestAddPostParam(%s=%s)", param, v );
    }
    if ( rc == 0 ) {
        const char param [] = "object";
        rc = KHttpRequestAddPostParam ( req, "%s=0||%S", param, acc );
        if ( rc != 0 )
            STestFail ( self, rc, 0,
                        "KHttpRequestAddPostParam(%s=0||%S)", param, acc );
    }
    if ( rc == 0 ) {
        const char param [] = "version";
        rc = KHttpRequestAddPostParam ( req, "%s=3.0", param );
        if ( rc != 0 )
            STestFail ( self, rc, 0,
                        "KHttpRequestAddPostParam(%s=3.0)", param );
    }
    if ( rc == 0 ) {
        rc_t r1 = 0;
        const KConfigNode * nProtected = NULL;
        KNamelist * names = NULL;
        uint32_t count = 0;
        const char path [] = "/repository/user/protected";
        if ( KConfigOpenNodeRead ( self -> kfg, & nProtected, path ) != 0 )
            return 0;
        r1 = KConfigNodeListChildren ( nProtected, & names );
        if ( r1 == 0 )
            r1 = KNamelistCount ( names, & count );
        if ( r1 == 0 ) {
            uint32_t i = 0;
            for ( i = 0; i < count; ++ i ) {
                const KConfigNode * node = NULL;
                String * tic = NULL;
                const char * name = NULL;
                r1 = KNamelistGet ( names, i, & name );
                if ( r1 != 0 )
                    continue;
                r1 = KConfigNodeOpenNodeRead ( nProtected, & node,
                                               "%s/download-ticket", name );
                if ( r1 != 0 )
                    continue;
                r1 = KConfigNodeReadString ( node, & tic );
                if ( r1 == 0 ) {
                    const char param[] = "tic";
                    rc = KHttpRequestAddPostParam ( req, "%s=%S", param, tic );
                    if ( rc != 0 )
                        STestFail ( self, rc, 0,
                            "KHttpRequestAddPostParam(%s)", param );
                    free ( tic );
                    tic = NULL;
                }
                RELEASE ( KConfigNode, node  );
            }
        }
        RELEASE ( KConfigNode, nProtected  );
    }
    if ( rc == 0 ) {
        STestStart ( self, false, 0,
                     "KHttpRequestPOST(KHttpRequest(%S)):", cgi );
        rc = KHttpRequestPOST ( req, & rslt );
        if ( rc == 0 )
            STestEndOr ( self, & rc, eEndOK, "OK"  );
        if ( rc != 0 ) {
            if ( _RcCanceled ( rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        uint32_t code = 0;
        STestStart ( self, false, 0,
                     "KHttpResultStatus(KHttpResult(%S)) =", cgi );
        rc = KHttpResultStatus ( rslt, & code, NULL, 0, NULL );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, "%R", rc );
        else {
            rc = STestEnd ( self, eMSG, "%u: ", code );
            if ( rc == 0 ) {
                if ( code == 200 )
                    STestEnd ( self, eEndOK, "OK" );
                else {
                    STestEnd ( self, eEndFAIL, "bad status" );
                    rs = RC ( rcRuntime, rcFile, rcReading, rcFile, rcInvalid );
                }
                AbuseSetStatus ( test, code );
            }
            else  if ( STestCanceled ( self, rc ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "%R", rc );
        }
    }
    if ( rc == 0 ) {
        const char name [] = "Location";
        rc_t r2 = 0;
        char buffer [ PATH_MAX ] = "";
        size_t num_read = 0;
        STestStart ( self, false, 0, "KClientHttpResultGetHeader(%s)", name );
        r2 = KClientHttpResultGetHeader ( rslt, name,
                                          buffer, sizeof buffer, & num_read );
        if ( r2 != 0 ) {
            if ( r2 == SILENT_RC ( rcNS,rcTree,rcSearching,rcName,rcNotFound ) )
                rc = STestEnd ( self, eEndOK, ": not found: OK" );
            else
                STestEnd ( self, eEndFAIL, "%R", r2 );
        }
        else {
            STestEnd ( self, eEndFAIL, "'%.*s'", ( int ) num_read, buffer );
            r2 = AbuseSetLocation ( test, buffer, num_read );
            if ( r2 != 0 && rc == 0)
                rc = r2;
        }
    }
    if ( rc == 0 ) {
        rc = KHttpResultGetInputStream ( rslt, & stream );
        if ( rc != 0 )
            STestFail ( self, rc, 0, "KHttpResultGetInputStream" );
    }
    if ( rc == 0 ) {
        assert ( resp_read );
        STestStart ( self, false, 0, "KStreamRead(KHttpResult(%S)) =", cgi );
        rc = KStreamRead ( stream, response, response_sz, resp_read );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, "%R", rc );
        else {
            if ( * resp_read > response_sz - 4 ) {
                response [ response_sz - 4 ] = '.';
                response [ response_sz - 3 ] = '.';
                response [ response_sz - 2 ] = '.';
                response [ response_sz - 1 ] = '\0';
            }
            else {
                response [ * resp_read + 1 ] = '\0';
                for ( ; * resp_read > 0 && ( response [ * resp_read ] == '\n' ||
                                             response [ * resp_read ] == '\0' );
                      --  ( * resp_read ) )
                {
                    response [ * resp_read ] = '\0';
                }
            }
            * url = processResponse ( response, * resp_read );
            if ( rs != 0 )
                * url = NULL;
            rc = STestEnd ( self, eEndOK, "'%s': OK", response );
            if ( true )
                AbuseAdd ( test, response, * resp_read );
else      AbuseAdd(test,"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
"<html><head>\n"
"<title>302 Found</title>\n"
"</head><body>\n"
"<h1>Found</h1>\n"
"<p>The document has moved <a href=\"https://misuse.ncbi.nlm.nih.gov/error/abuse.shtml\">here</a>.</p>\n"
"</body></html>",0);
        }
    }
    {
        bool ok = false;
        bool abuse = true;
        STestAbuse ( self, test, & ok, & abuse );
        if ( abuse )
            self -> dad -> report . blocked = true;
    }
    KStreamRelease ( stream );
    stream = NULL;
    KHttpResultRelease ( rslt );
    rslt = NULL;
    KHttpRequestRelease ( req );
    req = NULL;
    free ( ( void * ) cgi );
    cgi = NULL;
/*AbuseAdd(test,"<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n"
"<html><head>\n"
"<title>302 Found</title>\n"
"</head><body>\n"
"<h1>Found</h1>\n"
"<p>The document has moved <a href=\"https://misuse.ncbi.nlm.nih.gov/error/abuse.shtml\">here</a>.</p>\n"
"</body></html>",0);*/
    if ( rc == 0 ) {
        const char * server = test -> server;
        if ( server == NULL )
            server = "www.ncbi.nlm.nih.gov ";
        if ( rs == 0 )
            rc = STestEnd ( self, eOK,  "Resolving of %s path to '%S'",
                                        http ? "HTTPS": "FASP", acc );
        else {
            rc = rs;
            STestEnd ( self, eFAIL, "Resolving of %s path to '%S'",
                                    http ? "HTTPS": "FASP", acc );
        }
    }
    else if ( _RcCanceled ( rc ) )
        STestEnd ( self, eCANCELED, "Resolving of %s path to '%S': CANCELED",
                                    http ? "HTTPS": "FASP", acc );
    else
        STestEnd ( self, eFAIL,     "Resolving of %s path to '%S'",
                                    http ? "HTTPS": "FASP", acc );
    return rc;
}

static rc_t STestCheckFaspDownload ( STest * self, const char * url,
                             const char * cache, uint64_t * cacheSz )
{
    rc_t rc = 0;

    uint32_t m = 0;
    String fasp;
    String schema;

    assert ( self );

    if ( ! self -> ascpChecked ) {
        ascp_locate ( & self -> ascp, & self -> asperaKey, true, true);
        self -> ascpChecked = true;

        if ( self -> ascp == NULL ) {
            STestStart ( self, false, 0, "ascp download test:" );
            rc = STestEnd ( self, eEndOK, "skipped: ascp not found" );
        }
    }

    if ( self -> ascp == NULL )
        return rc;

    STestStart ( self, false, KDIAGN_ASCP_DOWNLOAD, "ascp download test:" );

    CONST_STRING ( & fasp, "fasp://" );

    m = string_measure ( url, NULL );
    if ( m < fasp . size ) {
        LogOut ( KVERBOSITY_ERROR, 0, "UNEXPECTED SCHEMA IN '%s'", url );
        return 0;
    }

    StringInit( & schema, url, fasp . size, fasp . len );
    if ( ! StringEqual ( & schema, & fasp ) ) {
        LogOut ( KVERBOSITY_ERROR, 0, "UNEXPECTED SCHEMA IN '%s'", url );
        return 0;
    }

    if ( rc == 0 ) {
        STestRemoveCache ( self, cache );
        rc = aspera_get ( self -> ascp, self -> asperaKey,
                          url + fasp . size, cache, 0 );
    }

    if ( rc == 0 )
        rc = KDirectoryFileSize ( self -> dir, cacheSz, cache );
    if ( rc == 0 )
        STestEndOr ( self, & rc, eEndOK, "OK" );
    if ( rc != 0 ) {
        if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "CANCELED" );
        else
            STestEnd ( self, eEndFAIL, "%R", rc );
    }

    return rc;
}

/******************************************************************************/

/*
static rc_t _STestCheckAcc ( STest * self, const Data * data, bool print,
                            const char * exp, size_t esz )
{
    rc_t rc = 0;
    char response [ 4096 ] = "";
    size_t resp_len = 0;
    const char * url = NULL;
    String acc;
    bool checked = false;

    const VPath * vcache = NULL;
    char faspCache [ PATH_MAX ] = "";
    uint64_t faspCacheSize = 0;
    char httpCache [ PATH_MAX ] = "";
    uint64_t httpCacheSize = 0;

    assert ( self && data );

    memset ( & acc, 0, sizeof acc );
    if ( DataIsAccession ( data ) ) {
        Abuse test;
        AbuseInit ( & test );
        acc = * data -> acc;
        rc = STestCallCgi ( self, & acc, response, sizeof response,
                            & resp_len, & url, & test, true | false );
        AbuseFini ( & test );
    }
    if ( acc . size != 0 ) {
        String cache;
        VPath * path = NULL;
        rc_t r2 = VFSManagerMakePath ( self -> vmgr, & path,
                                       "%S", data -> acc );
        if ( r2 == 0 )
            r2 = VResolverQuery ( self -> resolver, eProtocolFasp,
                                  path, NULL, NULL, & vcache);
// TODO: find another cache location if r2 != 0
        if ( r2 == 0 )
            r2 = VPathGetPath ( vcache, & cache );
        if ( r2 == 0 ) {
            rc_t r1 = string_printf ( faspCache, sizeof faspCache, NULL,
                                      "%S.fasp", & cache );
            r2      = string_printf ( httpCache, sizeof httpCache, NULL,
                                      "%S.http", & cache );
            if ( r2 == 0 )
                r2 = r1;
        }
        RELEASE ( VPath, path );
        if ( rc == 0 && r2 != 0 )
            rc = r2;
    }
    if ( url != NULL ) {
        char * p = string_chr ( url, resp_len - ( url - response ), '|' );
        if ( p == NULL ) {
            rc = RC ( rcRuntime, rcString ,rcParsing, rcString, rcIncorrect );
            STestFail ( self, rc, "UNEXPECTED RESOLVER RESPONSE" );
        }
        else {
            const String * full = NULL;
            rc_t r2 = VPathMakeString ( data -> vpath, & full );
            char * d = NULL;
            if ( r2 != 0 )
                LogOut ( KVERBOSITY_ERROR, 0,
                         "CANNOT VPathMakeString: %R\n", r2 );
            d = string_chr ( url, resp_len - ( url - response ), '$' );
            if ( d == NULL )
                d = p;
            while ( d != NULL && d <= p ) {
                if ( ! checked && full != NULL && string_cmp ( full -> addr,
                        full -> size, url, d - url, d - url ) == 0 )
                {
                    checked = true;
                }
                * d = '\0';
                switch ( * url ) {
                    case 'h': {
                        Data dt;
                        if ( rc == 0 )
                            rc = DataInit ( & dt, self -> vmgr, url );
                        if ( rc == 0 ) {
                            rc_t r1 = STestCheckUrl ( self, & dt,
                                httpCache, & httpCacheSize, print, exp, esz );
                            if ( rc == 0 && r1 != 0 )
                                rc = r1;
                        }
                        DataFini ( & dt );
                        break;
                    }
                    case 'f': {
                        rc_t r1 = STestCheckFaspDownload ( self, url,
                                                   faspCache, & faspCacheSize );
                        if ( rc == 0 && r1 != 0 )
                            rc = r1;
                        break;
                    }
                    default:
                        break;
                }
                if ( d == p )
                    break;
                url = d + 1;
                d = string_chr ( d, resp_len - ( d - response ), '$' );
                if ( d > p )
                    d = p;
            }
            free ( ( void * ) full );
            full = NULL;
        }
    }
    if ( ! checked ) {
        rc_t r1 = STestCheckUrl ( self, data, httpCache, & httpCacheSize,
                                  print, exp, esz );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }
    if ( faspCacheSize != 0 && httpCacheSize != 0 ) {
        uint64_t pos = 0;
        rc_t r1 = 0;
        STestStart ( self, false, "HTTP vs ASCP download:" );
        if ( faspCacheSize != httpCacheSize ) {
            r1 = RC ( rcRuntime, rcFile, rcComparing, rcSize, rcUnequal );
            STestEnd ( self, eEndFAIL, "FAILURE: size does not match: "
                       "ascp(%lu)/http(%lu)", faspCacheSize, httpCacheSize );
        }
        else {
            const KFile * ascp = NULL;
            const KFile * http = NULL;
            rc_t r1 = KDirectoryOpenFileRead ( self -> dir, & ascp, faspCache );
            if ( r1 != 0 )
                LogOut ( KVERBOSITY_ERROR, 0,
                         "KDirectoryOpenFileRead(%s)=%R\n", faspCache, r1 );
            else {
                r1 = KDirectoryOpenFileRead ( self -> dir, & http, httpCache );
                if ( r1 != 0 )
                    LogOut ( KVERBOSITY_ERROR, 0,
                             "KDirectoryOpenFileRead(%s)=%R\n", httpCache, r1 );
            }
            if ( r1 == 0 ) {
                char bAscp [ 1024 ] = "";
                char bHttp [ 1024 ] = "";
                size_t ascp_read = 0;
                size_t http_read = 0;
                while ( r1 == 0 ) {
                    r1 = KFileReadAll ( ascp, pos, bAscp, sizeof bAscp,
                                        & ascp_read );
                    if ( r1 != 0 ) {
                        STestEnd ( self, eEndFAIL, "FAILURE to read '%s': %R",
                                                   faspCache, r1 );
                        break;
                    }
                    r1 = KFileReadAll ( http, pos, bHttp, sizeof bHttp,
                                        & http_read );
                    if ( r1 != 0 ) {
                        STestEnd ( self, eEndFAIL, "FAILURE to read '%s': %R",
                                                   httpCache, r1 );
                        break;
                    }
                    else if ( ascp_read != http_read ) {
                        r1 = RC (
                            rcRuntime, rcFile, rcComparing, rcSize, rcUnequal );
                        STestEnd ( self, eEndFAIL,
                            "FAILURE to read the same amount from files" );
                        break;
                    }
                    else if ( ascp_read == 0 )
                        break;
                    else {
                        pos += ascp_read;
                        if ( string_cmp ( bAscp, ascp_read,
                                          bHttp, http_read, ascp_read ) != 0 )
                        {
                            r1 = RC ( rcRuntime,
                                      rcFile, rcComparing, rcData, rcUnequal );
                            STestEnd ( self, eEndFAIL,
                                       "FAILURE: files are different" );
                            break;
                        }
                    }
                }
            }
            RELEASE ( KFile, ascp );
            RELEASE ( KFile, http );
        }
        if ( r1 == 0 ) {
            rc_t r2 = 0;
            r1 = KDirectoryRemove ( self -> dir, false, faspCache );
            if ( r1 != 0 )
                STestEnd ( self, eEndFAIL, "FAILURE: cannot remove '%s': %R",
                                           faspCache, r1 );
            r2 = KDirectoryRemove ( self -> dir, false, httpCache );
            if ( r2 != 0 ) {
                if ( r1 == 0 ) {
                    r1 = r2;
                    STestEnd ( self, eEndFAIL,
                        "FAILURE: cannot remove '%s': %R", httpCache, r1 );
                }
                else
                    LogOut ( KVERBOSITY_ERROR, 0,
                             "Cannot remove '%s': %R\n", httpCache, r2 );
            }
            if ( r1 == 0 )
                rc = STestEnd ( self, eEndOK, "%lu bytes compared: OK", pos );
            else if ( rc == 0 )
                rc = r1;
        }
    }

    if ( acc . size != 0 ) {
        if ( rc == 0 )
            rc = STestEnd ( self, eOK, "Access to '%S'", & acc );
        else if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "Access to '%S': CANCELED", & acc );
        else
            STestEnd ( self, eFAIL, "Access to '%S'", & acc );
    }

    RELEASE ( VPath, vcache );
    return rc;
}
*/
# if 0
static rc_t _STestCheckNetwork ( STest * self, const Data * data,
    const char * exp, size_t esz, const Data * data2,
    const char * fmt, ... )
{
    rc_t rc = 0;
    KEndPoint ep;
    char b [ 512 ] = "";
    String host;

    va_list args;
    va_start ( args, fmt );
    rc = string_vprintf ( b, sizeof b, NULL, fmt, args );
    if ( rc != 0 )
        STestFail ( self, rc, "CANNOT PREPARE MESSAGE" );
    va_end ( args );

    assert ( self && data );

    STestStart ( self, true, b );
    rc = VPathGetHost ( data -> vpath, & host );
    if ( rc != 0 )
        STestFail ( self, rc, "VPathGetHost" );
    else {
        rc_t r1 = 0;
        uint16_t port = 443;
        STestStart ( self, false, "KNSManagerInitDNSEndpoint(%S:%hu)",
                                  & host, port );
        rc = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep, & host, port );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, ": FAILURE: %R", rc );
        else {
            char endpoint [ 1024 ] = "";
            rc_t rx = endpoint_to_string ( endpoint, sizeof endpoint, & ep );
            if ( rx == 0 )
                STestEndOr ( self, & rx, eEndOK, "= '%s': OK", endpoint );
            if ( rx != 0 ) {
                if ( _RcCanceled ( rx ) )
                    STestEnd ( self, eCANCELED, "CANCELED" );
                else
                    STestEnd ( self, eEndFAIL,
                               "CANNOT CONVERT TO STRING: %R", rx );
            }
        }
        port = 80;
        STestStart ( self, false, "KNSManagerInitDNSEndpoint(%S:%hu)",
                                  & host, port );
        r1 = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep,
                                              & host, port );
        if ( r1 != 0 )
            STestEnd ( self, eEndFAIL, "FAILURE: %R", r1 );
        else {
            char endpoint [ 1024 ] = "";
            rc_t rx = endpoint_to_string ( endpoint, sizeof endpoint, & ep );
            if ( rx == 0 )
                STestEndOr ( self, & rx, eEndOK, "= '%s': OK", endpoint );
            if ( rx != 0 ) {
                if ( _RcCanceled ( rx ) )
                    STestEnd ( self, eCANCELED, "CANCELED" );
                else
                    STestEnd ( self, eEndFAIL,
                               "CANNOT CONVERT TO STRING: %R", rx );
            }
        }
        rc = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep, & host, port );
        if ( rc == 0 ) {
            rc = _STestCheckAcc ( self, data, false, exp, esz );
            if ( data2 != NULL ) {
                rc_t r2 = _STestCheckAcc ( self, data2, true, 0, 0 );
                if ( rc == 0 && r2 != 0 )
                    rc = r2;
            }
        }
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }

    if ( rc == 0 )
        rc = STestEnd ( self, eOK, b );
    else  if ( _RcCanceled ( rc ) )
        STestEnd ( self, eCANCELED, "%s: CANCELED", b );
    else
        STestEnd ( self, eFAIL, b );
    return rc;
}
#endif

struct KDiagnoseTestDesc {
    const char * name;
    const char * desc;
    uint64_t code;
    uint32_t level;
    KDiagnoseTestDesc * next;
    KDiagnoseTestDesc * child;
    const KDiagnoseTestDesc * depends;
};

static rc_t KDiagnoseTestDescRelease ( KDiagnoseTestDesc * self ) {
    if ( self != NULL ) {
        if ( self -> child )
            KDiagnoseTestDescRelease ( self -> child );

        if ( self -> next )
            KDiagnoseTestDescRelease ( self -> next );

        memset ( self, 0, sizeof * self );

        free ( self );
    }

    return 0;
}

static rc_t KDiagnoseTestDescMake ( KDiagnoseTestDesc ** self,
    uint32_t level, const char * name, uint64_t code )
{
    assert ( self );

    * self = calloc ( 1, sizeof ** self );

    if ( * self == NULL )
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );
    else {
        ( * self ) -> name  = name;
        ( * self ) -> desc  = "";
        ( * self ) -> code  = code;
        ( * self ) -> level = level;

        return 0;
    }
}

static rc_t KDiagnoseMakeDesc ( KDiagnose * self ) {
  rc_t rc = 0;

  KDiagnoseTestDesc * root = NULL;
  KDiagnoseTestDesc * kfg = NULL;

  KDiagnoseTestDesc * net = NULL;
  KDiagnoseTestDesc * netNcbi = NULL;
  KDiagnoseTestDesc * netHttp = NULL;
  KDiagnoseTestDesc * netAscp = NULL;
  KDiagnoseTestDesc * netHttpVsAscp = NULL;

  assert ( self );

  if ( rc == 0 )
    rc = KDiagnoseTestDescMake ( & root, 0, "System", KDIAGN_ALL );
  {
    KDiagnoseTestDesc * kfgRemote = NULL;
    KDiagnoseTestDesc * kfgSite = NULL;
    KDiagnoseTestDesc * kfgUser = NULL;
    KDiagnoseTestDesc * kfgAscp = NULL;
    KDiagnoseTestDesc * kfgGap = NULL;

    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfg, 1, "Configuration", KDIAGN_CONFIG );
        if ( rc == 0 )
            root -> child = kfg;
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfgRemote, 2, "Remote repository",
                                     KDIAGN_REPO_REMOTE );
        if ( rc == 0 )
            kfg -> child = kfgRemote;
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfgSite, 2, "Site repository",
                                     KDIAGN_REPO_SITE );
        if ( rc == 0 )
            kfgRemote -> next = kfgSite;
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfgUser, 2, "Public user repository",
                                     KDIAGN_REPO_USER_PUBLIC );
        if ( rc == 0 )
            kfgSite -> next = kfgUser;
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfgAscp, 2, "ascp transfer rate",
                                     KDIAGN_KFG_ASCP );
        if ( rc == 0 )
            kfgUser -> next = kfgAscp;
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & kfgGap, 2, "DbGaP configuration",
                                     KDIAGN_REPO_GAP );
        if ( rc == 0 )
            kfgAscp -> next = kfgGap;
    }
  }

  if ( rc == 0 ) {
    rc = KDiagnoseTestDescMake ( & net, 1, "Network", KDIAGN_NETWORK );
    if ( rc == 0 )
        kfg -> next = net;
  }

  {
    KDiagnoseTestDesc * netNcbiHttp = NULL;
    KDiagnoseTestDesc * netNcbiHttps = NULL;
    KDiagnoseTestDesc * netNcbiFtp = NULL;
    KDiagnoseTestDesc * netNcbiVers = NULL;
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netNcbi, 2, "Access to NCBI",
                                     KDIAGN_ACCESS_NCBI );
        if ( rc == 0 ) {
            net -> child = netNcbi;
            //netNcbi -> depends = kfgCommon;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netNcbiHttp, 3,
            "KNSManagerInitDNSEndpoint(www.ncbi.nlm.nih.gov:80)",
            KDIAGN_ACCESS_NCBI_HTTP );
        if ( rc == 0 ) {
            netNcbi -> child = netNcbiHttp;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netNcbiHttps, 3,
            "KNSManagerInitDNSEndpoint(www.ncbi.nlm.nih.gov:443)",
            KDIAGN_ACCESS_NCBI_HTTPS );
        if ( rc == 0 ) {
            netNcbiHttp -> next = netNcbiHttps;
        }
    }
#define FTP "ftp-trace.ncbi.nlm.nih.gov"
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netNcbiFtp, 3,
            "KNSManagerInitDNSEndpoint(" FTP ":443)", KDIAGN_ACCESS_NCBI_FTP );
        if ( rc == 0 ) {
            netNcbiHttps -> next = netNcbiFtp;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netNcbiVers, 3, "Access to "
            "'https://" FTP "/sra/sdk/current/sratoolkit.current.version'",
            KDIAGN_ACCESS_NCBI_VERSION );
        if ( rc == 0 ) {
            netNcbiFtp -> next = netNcbiVers;
        }
    }
  }

  if ( rc == 0 ) {
    rc = KDiagnoseTestDescMake ( & netHttp, 2, "HTTPS download",
                                     KDIAGN_HTTP );
    if ( rc == 0 ) {
        netNcbi -> next = netHttp;
//      netHttp -> depends = netNcbi;
    }
  }
  {
    KDiagnoseTestDesc * netHttpRun = NULL;
    KDiagnoseTestDesc * netHttpCgi = NULL;
    KDiagnoseTestDesc * netHttpSmall = NULL;
    KDiagnoseTestDesc * netVfsSmall = NULL;
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netHttpRun, 3, "HTTPS access to a run",
                                     KDIAGN_HTTP_RUN );
        if ( rc == 0 ) {
            netHttp -> child = netHttpRun;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netHttpCgi, 4, "Resolving of HTTPS path",
                                     KDIAGN_HTTP_CGI );
        if ( rc == 0 ) {
            netHttpRun -> child = netHttpCgi;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netHttpSmall, 4, "Access to a small run",
                                     KDIAGN_HTTP_SMALL_ACCESS );
        if ( rc == 0 ) {
            netHttpCgi -> next = netHttpSmall;
//          netAscp -> depends = netNcbi;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netVfsSmall, 4,
            "VFSManagerOpenDirectoryRead(a small run)", KDIAGN_HTTP_SMALL_VFS );
        if ( rc == 0 ) {
            netHttpSmall -> next = netVfsSmall;
//          netAscp -> depends = netNcbi;
        }
    }
  }
  if ( rc == 0 ) {
    rc = KDiagnoseTestDescMake ( & netAscp, 2, "Aspera download",
                                     KDIAGN_ASCP );
    if ( rc == 0 ) {
        netHttp -> next = netAscp;
//      netAscp -> depends = netNcbi;
    }
  }
  {
    KDiagnoseTestDesc * netRun = NULL;
    KDiagnoseTestDesc * netCgi = NULL;
    KDiagnoseTestDesc * download = NULL;

    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netRun, 3, "Aspera access to a run",
                                     KDIAGN_ASCP_RUN );
        if ( rc == 0 ) {
            netAscp -> child = netRun;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & netCgi, 4, "Resolving of FASP path",
                                     KDIAGN_ASCP_CGI );
        if ( rc == 0 ) {
            netRun -> child = netCgi;
        }
    }
    if ( rc == 0 ) {
        rc = KDiagnoseTestDescMake ( & download, 4, "ascp download test",
                                     KDIAGN_ASCP_DOWNLOAD );
        if ( rc == 0 ) {
            netCgi -> next = download;
        }
    }
  }

  if ( rc == 0 ) {
    rc = KDiagnoseTestDescMake ( & netHttpVsAscp, 2,
        "HTTP vs ASCP download", KDIAGN_HTTP_VS_ASCP );
    if ( rc == 0 ) {
        netAscp -> next = netHttpVsAscp;
    }
  }

  if ( rc != 0 )
    KDiagnoseTestDescRelease ( root );
  else
    self -> desc = root;

  return rc;
}

static const char DIAGNOSE_CLSNAME [] = "KDiagnose";

LIB_EXPORT rc_t CC KDiagnoseMakeExt ( KDiagnose ** test, KConfig * kfg,
    KNSManager * kmgr, VFSManager * vmgr, rc_t (CC *quitting)(void) )
{
    rc_t rc = 0;

    KDiagnose * p = NULL;

    if ( test == NULL )
        return  RC ( rcRuntime, rcData, rcCreating, rcParam, rcNull );

    p = calloc ( 1, sizeof * p );
    if ( p == NULL )
        return RC ( rcRuntime, rcData, rcAllocating, rcMemory, rcExhausted );

    if ( kfg == NULL ) {
        rc_t r2 = KConfigMake ( & p -> kfg, NULL );
        if ( rc == 0 && r2 != 0 )
            rc = r2;
    }
    else {
        rc_t r2 = KConfigAddRef ( kfg );
        if ( r2 == 0 )
            p -> kfg = kfg;
        else if ( rc == 0 )
            rc = r2;
    }

    if ( kmgr == NULL ) {
        rc_t r2 = KNSManagerMake ( & p -> kmgr );
        if ( rc == 0 && r2 != 0 )
            rc = r2;
    }
    else {
        rc_t r2 = KNSManagerAddRef ( kmgr );
        if ( r2 == 0 )
            p -> kmgr = kmgr;
        else if ( rc == 0 )
            rc = r2;
    }

    if ( vmgr == NULL ) {
        rc_t r2 = VFSManagerMake ( & p -> vmgr );
        if ( rc == 0 && r2 != 0 )
            rc = r2;
    }
    else {
        rc_t r2 = VFSManagerAddRef ( vmgr );
        if ( r2 == 0 )
            p -> vmgr = vmgr;
        else if ( rc == 0 )
            rc = r2;
    }

    if ( rc == 0 )
        rc = KLockMake ( & p -> lock );
    if ( rc == 0 )
        rc = KConditionMake ( & p -> condition );

    if ( rc == 0 )
        rc = KDiagnoseMakeDesc ( p );

    if ( rc == 0 ) {
        p -> verbosity = KConfig_Verbosity ( p -> kfg );
        KRefcountInit ( & p -> refcount, 1, DIAGNOSE_CLSNAME, "init", "" );
        p -> quitting = quitting;
        AbuseInit ( & p -> report . abuse );
        * test = p;
    }
    else
        KDiagnoseRelease ( p );

    return rc;
}

LIB_EXPORT rc_t CC KDiagnoseAddRef ( const KDiagnose * self ) {
    if ( self != NULL )
        switch ( KRefcountAdd ( & self -> refcount, DIAGNOSE_CLSNAME ) ) {
            case krefLimit:
                return RC ( rcRuntime,
                            rcData, rcAttaching, rcRange, rcExcessive );
        }

    return 0;
}

static void CC errorWhack ( void * item, void * data )
{   KDiagnoseErrorWhack ( item ); }
static void CC testsWhack ( void * item, void * data )
{   KDiagnoseTestWhack ( item ); }

LIB_EXPORT rc_t CC KDiagnoseRelease ( const KDiagnose * cself ) {
    rc_t rc = 0;

    KDiagnose * self = ( KDiagnose * ) cself;

    if ( self != NULL )
        switch ( KRefcountDrop ( & self -> refcount, DIAGNOSE_CLSNAME ) ) {
            case krefWhack:
                RELEASE ( KConfig   , self -> kfg );
                RELEASE ( KNSManager, self -> kmgr );
                RELEASE ( VFSManager, self -> vmgr );
                RELEASE ( KLock     , self -> lock );
                RELEASE ( KCondition, self -> condition );
                VectorWhack ( & self -> tests , & testsWhack, NULL );
                VectorWhack ( & self -> errors, & errorWhack, NULL );

                RELEASE ( KDiagnoseTestDesc, self -> desc  );

                AbuseFini ( & self -> report . abuse );

                free ( self );
                break;
            case krefNegative:
                return RC ( rcRuntime,
                            rcData, rcReleasing, rcRange, rcExcessive );
        }

    return rc;
}

static rc_t _KDiagnoseSetState ( KDiagnose * self, enum EState state ) {
    rc_t rc = 0;
    rc_t r2 = 0;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    rc = KLockAcquire ( self -> lock );

    if ( rc == 0 ) {
        self -> state = state;

        rc = KConditionSignal ( self -> condition );
    }

    r2 = KLockUnlock ( self -> lock );
    if ( rc == 0 && r2 != 0 )
        rc = r2;

    return rc;
}

LIB_EXPORT rc_t CC KDiagnosePause  ( KDiagnose * self ) {
    return _KDiagnoseSetState ( self, ePaused );
}

LIB_EXPORT rc_t CC KDiagnoseResume ( KDiagnose * self ) {
    return _KDiagnoseSetState ( self, eRunning );
}

LIB_EXPORT rc_t CC KDiagnoseCancel ( KDiagnose * self ) {
    return _KDiagnoseSetState ( self, eCanceled );
}

LIB_EXPORT rc_t CC KDiagnoseGetDesc ( const KDiagnose * self,
    const KDiagnoseTestDesc ** desc )
{
    if ( desc == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcParam, rcNull );

    * desc = NULL;

    if ( self == NULL )
        return RC ( rcRuntime, rcData, rcAccessing, rcSelf, rcNull );

    * desc = self -> desc;

    return 0;
}

static rc_t STestKNSManagerInitDNSEndpoint ( STest * self, uint64_t tests,
    const String * host, uint16_t port, bool warn )
{
    rc_t rc = 0;

    KEndPoint ep;

    assert ( self && self -> dad );

    STestStart ( self, false, tests, "KNSManagerInitDNSEndpoint(%S:%hu)",
                                                          host, port );
    rc = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep, host, port );
    if ( rc != 0 ) {
        self -> dad -> report . firewall = true;
        STestEnd ( self, warn ? eWarning : eEndFAIL, "%R", rc );
    }
    else {
        char endpoint [ 1024 ] = "";
        rc_t rx = endpoint_to_string ( endpoint, sizeof endpoint, & ep );
        if ( rx == 0 )
            STestEndOr ( self, & rx, eEndOK, "= '%s': OK", endpoint );
        if ( rx != 0 ) {
            if ( _RcCanceled ( rx ) )
                STestEnd ( self, eCANCELED, "CANCELED" );
            else
                STestEnd ( self, eEndFAIL, "CANNOT CONVERT TO STRING: %R", rx );
        }
    }

    return rc;
}

static rc_t STestCheckNcbiAccess ( STest * self, uint64_t tests ) {
    rc_t rc = 0;

    String www;
    CONST_STRING ( & www, "www.ncbi.nlm.nih.gov" );

    if ( tests & KDIAGN_ACCESS_NCBI_HTTP ) {
        rc_t r1 = STestKNSManagerInitDNSEndpoint ( self,
            KDIAGN_ACCESS_NCBI_HTTP, & www, 80, false );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }

    if ( tests & KDIAGN_ACCESS_NCBI_HTTPS ) {
        rc_t r1 = STestKNSManagerInitDNSEndpoint ( self,
            KDIAGN_ACCESS_NCBI_HTTPS, & www, 443, false );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }

    if ( tests & KDIAGN_ACCESS_NCBI_FTP ) {
        rc_t r1 = 0;
        String ftp;
#define FTP "ftp-trace.ncbi.nlm.nih.gov"
        CONST_STRING ( & ftp, FTP );
        r1 = STestKNSManagerInitDNSEndpoint ( self,
            KDIAGN_ACCESS_NCBI_FTP, & ftp, 443, true );
        if ( r1 == 0 ) {
            Data v;
            r1 = DataInit ( & v, self -> vmgr, "https://" FTP
                            "/sra/sdk/current/sratoolkit.current.version" );
            if ( r1 == 0 ) {
                uint64_t s = 0;
                r1 = STestCheckUrl ( self, tests, KDIAGN_ACCESS_NCBI_VERSION,
                                     0, & v, "", & s, true, 0, 0, NULL );
            }
            DataFini ( & v );
        }
        /* ignore result: failure to access current.version is not an error */
    }

    return rc;
}

static bool STestWritableImpl ( const STest * self,
                                const char * path, bool dir )
{
    char tmp [ PATH_MAX ] = "";

    int i = 0;

    if ( path == NULL )
        return false;

    assert ( self );

    if ( ! dir ) {
        rc_t rc = 0;
        KFile * f = NULL;

        if ( KDirectoryPathType ( self -> dir, path ) != kptNotFound ) {
            rc = KDirectoryRemove ( self -> dir, false, path );
            if ( rc != 0 )
                return false;
        }

        rc = KDirectoryCreateFile ( self -> dir, & f, false,
                                    0775, kcmCreate | kcmParents, path);
        if ( rc != 0 )
            return false;

        RELEASE ( KFile, f );

        rc = KDirectoryRemove ( self -> dir, false, path );
        return rc == 0;
    }

    for ( i = 0; i <= 0; ++i ) {
        rc_t rc = string_printf ( tmp, sizeof tmp, NULL, "%s/tmp-ncbi-vdb%d",
                                                         path, i );
        if ( rc != 0 )
            return false;

        if ( KDirectoryPathType ( self -> dir, tmp ) != kptNotFound )
            continue;

        rc = KDirectoryCreateDir ( self -> dir,
                                   0775, kcmCreate | kcmParents, tmp );
        if ( rc == 0 ) {
            rc = KDirectoryRemove ( self -> dir, false, tmp );
            return rc == 0;
        }
        else
            return false;
    }

    return false;
}

static bool STestWritable ( const STest * self, const char * path )
{   return STestWritableImpl ( self, path, true ); }

static bool STestCanCreate ( const STest * self, const char * path )
{   return STestWritableImpl ( self, path, false ); }

static rc_t STestCache ( const STest * self, const String * acc,
    char * cache, size_t sCache, const char * suffix )
{
    String rCache;
    VPath * path = NULL;
    rc_t rc = VFSManagerMakePath ( self -> vmgr, & path, "%S", acc );

    const VPath * vcache = NULL;
    if ( rc == 0 )
        rc = VResolverQuery ( self -> resolver, eProtocolHttps, path,
                              NULL, NULL, & vcache );
    if ( rc == 0 )
        rc = VPathGetPath ( vcache, & rCache );
    if ( rc == 0 ) {
        rc = string_printf ( cache, sCache, NULL, "%S.%s", & rCache, suffix );
        RELEASE ( VPath, vcache );
        if ( rc != 0 || ! STestCanCreate ( self, cache ) )
            cache [ 0 ] = '\0';
    }
    RELEASE ( VPath, path );

    if ( cache [ 0 ] == '\0' ) {
        const char * p = getenv ( "TMPDIR" );
        while ( cache [ 0 ] == '\0' ) {
            if ( STestWritable ( self, p ) )
                break;
            p          = getenv ( "TEMP" );
            if ( STestWritable ( self, p ) )
                break;
            p          = getenv ( "TMP" );
            if ( STestWritable ( self, p ) )
                break;
            p          = getenv ( "TEMPDIR" );
            if ( STestWritable ( self, p ) )
                break;
            p = NULL;
            break;
        }
        if ( p != NULL ) {
            rc = string_printf ( cache, sCache, NULL, "%s/%S.%s",
                                                      p, acc, suffix );
            if ( rc != 0 )
                cache [ 0 ] = '\0';
        }
    }

    if ( cache [ 0 ] == '\0' ) {
        String * p = NULL;
        rc = KConfigReadString ( self -> kfg,
                                 "/repository/user/default-path", & p );
        if ( rc == 0 && STestWritable ( self, p -> addr ) ) {
            rc = string_printf ( cache, sCache, NULL, "%S/%S.%s",
                                                      p, acc, suffix );
            free ( p );
        }
        if ( rc != 0 )
            cache [ 0 ] = '\0';
    }

    if ( cache [ 0 ] == '\0' ) {
        String * p = NULL;
        rc = KConfigReadString ( self -> kfg, "NCBI_HOME", & p );
        if ( rc == 0 && STestWritable ( self, p -> addr ) ) {
            rc = string_printf ( cache, sCache, NULL, "%S/%S.%s",
                                                      p, acc, suffix );
            free ( p );
        }
        if ( rc != 0 )
            cache [ 0 ] = '\0';
    }

    if ( cache [ 0 ] == '\0' ) {
        String * p = NULL;
        rc = KConfigReadString ( self -> kfg, "HOME", & p );
        if ( rc == 0 && STestWritable ( self, p -> addr ) ) {
            rc = string_printf ( cache, sCache, NULL, "%S/%S.%s",
                                                      p, acc, suffix );
            free ( p );
        }
        if ( rc != 0 )
            cache [ 0 ] = '\0';
    }

    if ( cache [ 0 ] == '\0' ) {
        rc = string_printf ( cache, sCache, NULL, "%S/%s", acc, suffix );
        if ( rc != 0 )
            cache [ 0 ] = '\0';
        else if ( ! STestWritable ( self, "." ) ) {
            cache [ 0 ] = '\0';
            rc = RC ( rcRuntime,
                      rcDirectory, rcAccessing, rcDirectory, rcUnauthorized );
        }
    }

    return rc;
}

static rc_t STestCheckHttp ( STest * self, uint64_t tests, const String * acc,
    bool print,
    char * downloaded, size_t sDownloaded, uint64_t * downloadedSize,
    const char ** exp, size_t esz, bool * tooBig )
{
    rc_t rc = 0;
    char response [ 4096 ] = "";
    size_t resp_len = 0;
    const char * url = NULL;
    bool failed = false;
    uint64_t atest = KDIAGN_HTTP_RUN;
    STestStart ( self, true, atest,
                 "HTTPS access to '%S'", acc );
    if ( tests & KDIAGN_HTTP_CGI )
        rc = STestCallCgi ( self, KDIAGN_HTTP_CGI, acc,
            response, sizeof response, & resp_len, & url, true );

    if ( rc == 0 ) {
        rc = STestCache ( self, acc, downloaded, sDownloaded, "http" );
        if ( rc != 0 )
            STestFail ( self, rc, 0, "Cannot find cache location" );
    }
    if ( rc == 0 && url != NULL ) {
        char * p = string_chr ( url, resp_len - ( url - response ), '|' );
        if ( p == NULL ) {
            rc = RC ( rcRuntime,
                        rcString ,rcParsing, rcString, rcIncorrect );
            STestFail ( self, rc, 0, "UNEXPECTED RESOLVER RESPONSE" );
            failed = true;
        }
        else {
            Data dt;
            * p = '\0';
            rc = DataInit ( & dt, self -> vmgr, url );
            if ( rc == 0 ) {
                rc_t r1 = STestCheckUrl ( self,
                    tests, KDIAGN_HTTP_SMALL_ACCESS, KDIAGN_HTTP_SMALL_VFS,
                    & dt, downloaded, downloadedSize, print, exp, esz, tooBig );
                if ( rc == 0 && r1 != 0 ) {
                    assert ( downloaded );
                    * downloaded = '\0';
                    rc = r1;
                }
            }
            DataFini ( & dt );
        }
    }

    if ( ! failed ) {
        if ( rc == 0 )
            rc = STestEnd ( self, eOK,  "HTTPS access to '%S'", acc );
        else if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "HTTPS access to '%S': CANCELED", acc );
        else
            STestEnd ( self, eFAIL,     "HTTPS access to '%S'", acc );
    }
    return rc;
}

static rc_t STestCheckFasp ( STest * self, uint64_t tests, const String * acc,
    bool print, char * downloaded, size_t sDownloaded,
    uint64_t * downloadedSize )
{
    rc_t rc = 0;
    char response [ 4096 ] = "";
    size_t resp_len = 0;
    const char * url = NULL;
    bool failed = false;
    uint64_t atest = KDIAGN_ASCP_RUN;
    STestStart ( self, true, atest, "Aspera access to '%S'", acc );
    if ( tests & KDIAGN_ASCP_CGI )
        rc = STestCallCgi ( self, KDIAGN_ASCP_CGI, acc,
            response, sizeof response, & resp_len, & url, false );

    if ( tests & KDIAGN_DOWNLOAD_ASCP ) {
        if ( rc == 0 ) {
            rc = STestCache ( self, acc, downloaded, sDownloaded, "fasp" );
            if ( rc != 0 )
                STestFail ( self, rc, 0, "Cannot find cache location" );
        }
        if ( rc == 0 && url != NULL ) {
            char * p = string_chr ( url, resp_len - ( url - response ), '|' );
            if ( p == NULL ) {
                rc = RC ( rcRuntime,
                          rcString ,rcParsing, rcString, rcIncorrect );
                STestFail ( self, rc, 0, "UNEXPECTED RESOLVER RESPONSE" );
                failed = true;
            }
            else {
                Data dt;
                * p = '\0';
                rc = DataInit ( & dt, self -> vmgr, url );
                if ( rc == 0 ) {
                    rc_t r1 = STestCheckFaspDownload ( self, url, downloaded,
                                                       downloadedSize );
                    if ( rc == 0 && r1 != 0 ) {
                        assert ( downloaded );
                        * downloaded = '\0';
                        rc = r1;
                    }
                }
                DataFini ( & dt );
            }
        }
    }

    if ( ! failed ) {
        if ( rc == 0 )
            rc = STestEnd ( self, eOK,  "Aspera access to '%S'", acc );
        else if ( _RcCanceled ( rc ) )
            STestEnd ( self, eCANCELED, "Aspera access to '%S': CANCELED",
                                        acc );
        else
            STestEnd ( self, eFAIL,     "Aspera access to '%S'", acc );
    }
    return rc;
}

static
rc_t STestHttpVsFasp ( STest * self, const char * http, uint64_t httpSize,
                       const char * fasp, uint64_t faspSize )
{
    rc_t rc = 0;
    uint64_t pos = 0;
    const KFile * ascpF = NULL;
    const KFile * httpF = NULL;
    STestStart ( self, false, KDIAGN_HTTP_VS_ASCP, "HTTP vs ASCP download:" );
    if ( httpSize != faspSize ) {
        rc = RC ( rcRuntime, rcFile, rcComparing, rcSize, rcUnequal );
        STestEnd ( self, eEndFAIL, "size does not match: "
                   "http(%lu)/ascp(%lu)", httpSize, faspSize );
    }
    else {
        rc = KDirectoryOpenFileRead ( self -> dir, & httpF, http );
        if ( rc != 0 )
            STestEnd ( self, eEndFAIL, "cannot open '%s'; %R", http, rc );
        else {
            rc = KDirectoryOpenFileRead ( self -> dir, & ascpF, fasp );
            if ( rc != 0 )
                STestEnd ( self, eEndFAIL, "cannot open '%s'; %R", fasp, rc );
        }
    }
    if ( rc == 0 ) {
        char bAscp [ 1024 ] = "";
        char bHttp [ 1024 ] = "";
        size_t ascp_read = 0;
        size_t http_read = 0;
        while ( rc == 0 ) {
            rc = KFileReadAll ( ascpF, pos, bAscp, sizeof bAscp, & ascp_read );
            if ( rc != 0 ) {
                STestEnd ( self, eEndFAIL, "cannot read '%s': %R",
                                          fasp, rc );
                break;
            }
            rc = KFileReadAll ( httpF, pos, bHttp, sizeof bHttp, & http_read );
            if ( rc != 0 ) {
                STestEnd ( self, eEndFAIL, "cannot read '%s': %R",
                                          http, rc );
                break;
            }
            else if ( ascp_read != http_read ) {
                rc = RC ( rcRuntime, rcFile, rcComparing, rcSize, rcUnequal );
                STestEnd ( self, eEndFAIL,
                           "cannot read the same amount from files" );
                break;
            }
            else if ( ascp_read == 0 )
                break;
            else {
                pos += ascp_read;
                if ( string_cmp ( bAscp, ascp_read,
                                  bHttp, http_read, (uint32_t)ascp_read ) != 0 )
                {
                    rc = RC ( rcRuntime,
                              rcFile, rcComparing, rcData, rcUnequal );
                    STestEnd ( self, eEndFAIL, "files are different" );
                    break;
                }
            }
        }
    }
    RELEASE ( KFile, ascpF );
    RELEASE ( KFile, httpF );
    if ( rc == 0 )
        rc = STestEnd ( self, eEndOK, "%lu bytes compared: OK", pos );
    return rc;
}

static rc_t StringRelease ( String * self ) {
    if ( self != NULL )
        free ( self );
    return 0;
}

/*static rc_t STestWarning ( STest * self,
                           const char * msg, const char * fmt, ... )
{
    rc_t rc = 0;

    va_list args;
    va_start ( args, fmt );

    rc = STestVStart ( self, false, fmt, args );
    STestEnd ( self, eMSG, "WARNING: %s", msg );
    if ( CALL_BACK )
        CALL_BACK ( eKDTS_Warning, self -> crnt );
    rc = STestEnd ( self, eWarning, "" );

    va_end ( args );

    return rc;
}*/

static rc_t STestCheckNodeExists ( STest * self, uint64_t code,
    const char * path, const char * msg, const char * fmt, String ** value )
{
    String * p = NULL;
    rc_t rc = KConfigReadString ( self -> kfg, path, & p );

    STestStart ( self, false, code, fmt );

    if ( rc != 0 ) {
        if ( rc != SILENT_RC ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
            STestEnd ( self, eEndFAIL, "cannot read '%s': %R", path, rc );
        else {
            STestEnd ( self, eWarning, msg );
            rc = 0;
        }

        p = NULL;
    }
    else {
        if ( value != NULL )
            * value = p;

        STestEnd ( self, eEndOK, "'%S': OK", p );
        RELEASE ( String, p );
    }

    return rc;
}

static rc_t STestCheckRemoteRepoKfg ( STest * self, bool * exists ) {
    rc_t rc = 0;
    rc_t r1 = 0;
    String * p = NULL;
    assert ( exists );
    * exists = false;
    STestStart ( self, true, KDIAGN_REPO_REMOTE, "Remote repository" );
    {
        bool printed = false;
        const char * path = "/repository/remote/disabled";
        rc_t r1 = KConfigReadString ( self -> kfg, path, & p );
        STestStart ( self, false, 0, "Remote repository disabled:" );
        if ( r1 != 0 ) {
            if ( r1 !=
                 SILENT_RC ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
            {
                STestEnd ( self, eEndFAIL, "cannot read '%s': %R", path, r1 );
                printed = true;
                if ( rc == 0 )
                    rc = r1;
            }
        }
        else {
            String sTrue;
            CONST_STRING ( & sTrue, "true" );
            if ( StringEqual ( p, & sTrue ) ) {
                STestEnd ( self, eWarning, "Remote repository is disabled" );
                printed = true;
            }
            RELEASE ( String, p );
        }
        if ( ! printed )
            STestEnd ( self, eEndOK, "false: OK" );
    }
    {
        bool printed = false;
        const char * path = "/repository/remote/main/CGI/resolver-cgi";
        rc_t r1 = KConfigReadString ( self -> kfg, path, & p );
        STestStart ( self, false, 0, "Main resolver-cgi:" );
        if ( r1 != 0 ) {
            if ( r1 !=
                 SILENT_RC ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
            {
                STestEnd ( self, eEndFAIL, "cannot read '%s': %R", path, r1 );
                printed = true;
                if ( rc == 0 )
                    rc = r1;
            }
            else {
                STestEnd ( self, eWarning, "Main resolver-cgi is not set" );
                printed = true;
            }
        }
        else {
            String c, f, t;
            CONST_STRING ( & c,
                "https://www.ncbi.nlm.nih.gov/Traces/names/names.cgi" );
            CONST_STRING ( & f,
                "https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi" );
            CONST_STRING ( & t,
                "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi" );
            if ( ! StringEqual ( p, & f ) && ! StringEqual ( p, & c ) &&
                 ! StringEqual ( p, & t ))
            {
                STestEnd ( self, eWarning,
                    "Main resolver-cgi is not standard: '%S'", p );
                printed = true;
            }
            RELEASE ( String, p );

            * exists = true;
        }
        if ( ! printed )
            STestEnd ( self, eEndOK, "OK" );
    }
    r1 = STestEnd ( self, rc == 0 ? eOK : eFAIL, "Remote repository" );
    if ( rc == 0 && r1 != 0 )
        rc = r1;
    return rc;
}

static rc_t STestCheckSiteRepoKfg ( STest * self, bool * exists ) {
    bool printed = false;
    bool hasNode = true;
#define SITE "/repository/site"
    const char * path = SITE;
    const KConfigNode * node = NULL;
    String * p = NULL;
    rc_t rc = KConfigOpenNodeRead ( self -> kfg, & node, path );
    assert ( exists );
    * exists = false;
    STestStart ( self, false, KDIAGN_REPO_SITE, "Site repository:" );
    if ( rc != 0 ) {
        if ( rc != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                rcPath, rcNotFound ) )
        {
            STestEnd ( self, eEndFAIL, "cannot read '%s': %R", path, rc );
            printed = true;
        }
        else {
            hasNode = false;
            rc = 0;
            STestEnd ( self, eEndOK, "not found: OK" );
            printed = true;
        }
    }
    if ( hasNode ) {
        KNamelist * names = NULL;
        uint32_t count = 0;
        if ( rc == 0 ) {
            rc = KConfigNodeListChildren ( node, & names );
            if ( rc != 0 ) {
                STestEnd ( self, eEndFAIL,
                           "cannot list children of '%s': %R", path, rc );
                printed = true;
            }
        }
        if ( rc == 0 ) {
            rc = KNamelistCount ( names, & count );
            if ( rc != 0 ) {
                STestEnd ( self, eEndFAIL,
                           "cannot count children of '%s': %R", path, rc );
                printed = true;
            }
            else if ( count > 0 )
                * exists = true;
        }
        if ( rc == 0 ) {
            const char * path = SITE "/disabled";
            rc = KConfigReadString ( self -> kfg, path, & p );
            if ( rc != 0 ) {
                if ( rc != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                       rcPath, rcNotFound ) )
                {
                    STestEnd ( self, eEndFAIL, "cannot read '%s': %R",
                                                            path, rc );
                    printed = true;
                }
                else
                    rc = 0;
            }
            else {
                String sTrue;
                CONST_STRING ( & sTrue, "true" );
                if ( StringEqual ( p, & sTrue ) ) {
                    * exists = false;
                    if ( count > 1 ) {
                        STestEnd ( self, eWarning,
                                   "Site repository is disabled" );
                        printed = true;
                    }
                }
                RELEASE ( String, p );
            }
        }
        RELEASE ( KNamelist, names );
    }
    RELEASE ( KConfigNode, node );
    if ( ! printed )
        STestEnd ( self, eEndOK, "OK" );
    return rc;
}

static rc_t STestCheckUserRepoKfg ( STest * self, bool * exists ) {
    rc_t rc = 0;
    rc_t r1 = 0;
    String * p = NULL;
    assert ( exists );
    * exists = false;
    STestStart ( self, true, KDIAGN_REPO_USER_PUBLIC,
                 "Public user repository" );
    {
        bool printed = false;
        const char * path = "/repository/user/cache-disabled";
        rc_t r1 = KConfigReadString ( self -> kfg, path, & p );
        STestStart ( self, false, 0, "User repository caching:" );
        if ( r1 != 0 ) {
            if ( r1 !=
                 SILENT_RC ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
            {
                STestEnd ( self, eEndFAIL, "cannot  read '%s': %R", path, r1 );
                printed = true;
                if ( rc == 0 )
                    rc = r1;
            }
        }
        else {
            String sTrue;
            CONST_STRING ( & sTrue, "true" );
            if ( StringEqual ( p, & sTrue ) ) {
                STestEnd ( self, eWarning,
                           "User repository caching is disabled" );
                printed = true;
            }
            RELEASE ( String, p );
        }
        if ( ! printed )
            STestEnd ( self, eEndOK, "enabled: OK" );
    }
    {
        const char * path = "/repository/user/main/public";
        const KConfigNode * node = NULL;
        rc_t r1 = KConfigOpenNodeRead ( self -> kfg, & node, path );
        if ( r1 == 0 )
            * exists = true;
        else if ( r1 != SILENT_RC
                        ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
        {
            STestFail ( self, r1, 0, "Failed to read '%s'", path );
            if ( rc == 0 )
                rc = r1;
        }
        RELEASE ( KConfigNode, node );
    }
    r1 = STestCheckNodeExists ( self, 0,
        "/repository/user/main/public/apps/file/volumes/flat",
        "User repository is incomplete", "User repository file app:", NULL );
    if ( r1 != 0 && rc == 0 )
        rc = r1;

    r1 = STestCheckNodeExists ( self, 0,
        "/repository/user/main/public/apps/nakmer/volumes/nakmerFlat",
        "User repository is incomplete", "User repository nakmer app:", NULL );
    if ( r1 != 0 && rc == 0 )
        rc = r1;

    r1 = STestCheckNodeExists ( self, 0,
        "/repository/user/main/public/apps/nannot/volumes/nannotFlat",
        "User repository is incomplete", "User repository nannot app:", NULL );
    if ( r1 != 0 && rc == 0 )
        rc = r1;

    {
        r1 = STestCheckNodeExists ( self, 0,
            "/repository/user/main/public/apps/refseq/volumes/refseq",
            "User repository is incomplete", "User repository refseq app:",
            & p );
        if ( r1 != 0 ) {
            if ( rc == 0 )
                rc = r1;
        }
        else if ( p == NULL )
            * exists = false;
        else
            STestEnd ( self, eEndOK, "'%S': OK", p );
        RELEASE ( String, p );
    }
    {
        r1 = STestCheckNodeExists ( self, 0,
            "/repository/user/main/public/apps/sra/volumes/sraFlat",
            "User repository is incomplete", "User repository sra app:", & p );
        if ( r1 != 0 ) {
            if ( rc == 0 )
                rc = r1;
        }
        else if ( p == NULL )
            * exists = false;
        else
            STestEnd ( self, eEndOK, "'%S': OK", p );
        RELEASE ( String, p );
    }

    r1 = STestCheckNodeExists ( self, 0,
        "/repository/user/main/public/apps/wgs/volumes/wgsFlat",
        "User repository is incomplete", "User repository wgs app:", NULL );
    if ( r1 != 0 && rc == 0 )
        rc = r1;

    {
        bool user = false;
        r1 = STestCheckNodeExists ( self, 0,
            "/repository/user/main/public/root",
            "User repository's root path is not set",
            "User repository root path:",
            & p );
        if ( r1 != 0 ) {
            if ( rc == 0 )
                rc = r1;
        }
        else if ( p != NULL ) {
            if ( p -> size == 0 )
                STestEnd ( self, eWarning,
                           "User repository's root path is empty" );
            else {
                KPathType type = kptFirstDefined;
                type = KDirectoryPathType ( self -> dir, p -> addr )
                        & ~ kptAlias;
                if ( type == kptNotFound )
                    STestEnd ( self, eWarning,
                        "User repository's root path does not exist: '%S'", p );
                else if ( type != kptDir )
                    STestEnd ( self, eWarning,
                        "User repository's root path is not a directory: '%S'",
                        p );
                else {
                    STestEnd ( self, eEndOK, "'%S': OK", p );
                    user = true;
                }
            }
            RELEASE ( String, p );
        }
        if ( ! user )
            * exists  = false;
    }
    r1 = STestEnd ( self, rc == 0 ? eOK : eFAIL, "Public user repository" );
    if ( rc == 0 && r1 != 0 )
        rc = r1;
    return rc;
}

static rc_t STestCheckNoGapKfg ( STest * self, uint64_t tests ) {
    rc_t rc = 0;
    rc_t r1 = 0;
    bool rRemote = false;
    bool rSite  = false;
    bool rUser  = false;
    if ( tests & KDIAGN_REPO_REMOTE ) {
        rc_t r1 = STestCheckRemoteRepoKfg ( self, & rRemote );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }
    if ( tests & KDIAGN_REPO_SITE ) {
        rc_t r1 = STestCheckSiteRepoKfg ( self, & rSite );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }
    if ( tests & KDIAGN_REPO_USER_PUBLIC ) {
        rc_t r1 = STestCheckUserRepoKfg ( self, & rUser );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }
    if ( tests & KDIAGN_KFG_ASCP ) {
        r1 = STestCheckNodeExists ( self, KDIAGN_KFG_ASCP,
            "/tools/ascp/max_rate",
            "ascp max transfer rate is not set", "ascp transfer rate:", NULL );
        if ( r1 != 0 && rc == 0 )
            rc = r1;
    }

    if ( ! rRemote && ! rSite && ! rUser ) {
        if ( rc == 0 )
            rc = RC ( rcRuntime, rcNode, rcValidating, rcNode, rcInsufficient );
        STestFail ( self, rc, 0, "No repositories in configuration: " );
    }

    return rc;
}

static rc_t STestCheckGapKfg ( STest * self, uint64_t tests ) {
    rc_t rc = 0;
    String * p = NULL;
    {
        bool printed = false;
        const char * path = "/repository/remote/protected/CGI/resolver-cgi";
        rc_t r1 = KConfigReadString ( self -> kfg, path, & p );
        STestStart ( self, false, 0, "Protected resolver-cgi:" );
        if ( r1 != 0 ) {
            if ( r1 !=
                 SILENT_RC ( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
            {
                STestEnd ( self, eEndFAIL, "cannot  read '%s': %R", path, r1 );
                printed = true;
                if ( rc == 0 )
                    rc = r1;
            }
            else {
                STestEnd ( self, eEndOK, "not set: OK" );
                printed = true;
            }
        }
        else {
            String c, f, t;
            CONST_STRING ( & c,
                "https://www.ncbi.nlm.nih.gov/Traces/names/names.cgi" );
            CONST_STRING ( & f,
                "https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi" );
            CONST_STRING ( & t,
                "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi" );
            if ( ! StringEqual ( p, & f ) && ! StringEqual ( p, & c ) &&
                 ! StringEqual ( p, & t ))
            {
                STestEnd ( self, eWarning,
                           "Protected resolver-cgi is not standard: '%S'", p );
                printed = true;
            }
            RELEASE ( String, p );
        }
        if ( ! printed )
            STestEnd ( self, eEndOK, "OK" );
    }
    {
        const char * path = "/repository/user/protected";
        const KConfigNode * nProtected = NULL;
        rc_t r1 = KConfigOpenNodeRead ( self -> kfg, & nProtected, path );
        if ( r1 != 0 ) {
            if ( r1 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                   rcPath, rcNotFound ) )
            {
                STestFail ( self, r1, 0,
                    "Protected repositories: failed to read '%s'", path );
                if ( rc == 0 )
                    rc = r1;
            }
            else {
                STestStart ( self, false, 0, "Protected repositories:" );
                STestEnd ( self, eEndOK, "not found: OK" );
            }
        }
        else {
            bool printed = false;
            rc_t r1 = 0;
            KNamelist * names = NULL;
            uint32_t count = 0;
            STestStart ( self, true, 0, "Protected repositories" );
            if ( r1 == 0 ) {
                r1 = KConfigNodeListChildren ( nProtected, & names );
                if ( r1 != 0 ) {
                    STestEnd ( self, eEndFAIL,
                        "cannot list children of '%s': %R", path, r1 );
                    printed = true;
                }
            }
            if ( r1 == 0 ) {
                r1 = KNamelistCount ( names, & count );
                if ( r1 != 0 ) {
                    STestEnd ( self, eEndFAIL,
                        "cannot count children of '%s': %R", path, r1 );
                    printed = true;
                }
            }
            if ( r1 == 0 ) {
                uint32_t i = 0;
                for ( i = 0; i < count; ++ i ) {
                    const KConfigNode * node = NULL;
                    const char * name = NULL;
                    r1 = KNamelistGet ( names, i, & name );
                    if ( r1 != 0 ) {
                        STestEnd ( self, eEndFAIL,
                            "cannot get child of '%s': %R", path, r1 );
                        printed = true;
                        break;
                    }
                    STestStart ( self, true, 0, name );
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/apps/file/volumes/flat", path, name );
                        STestStart ( self, false, 0, "%s file app:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL, "cannot read "
                                    "'%s/%s/apps/file/volumes/flat': %R",
                                    path, name, r2 );
                            else {
                                self -> dad -> report . incompleteGapKfg = true;
                                STestEnd ( self, tests & KDIAGN_TRY_TO_WARN
                                                    ? eWarning : eEndFAIL,
                                           "not found" );
                            }
                        }
                        else
                            STestEnd ( self, eEndOK, "OK" );
                        RELEASE ( KConfigNode, node );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/apps/sra/volumes/sraFlat", path, name );
                        STestStart ( self, false, 0, "%s sra app:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL, "cannot read "
                                    "'%s/%s/apps/sra/volumes/sraFlat': %R",
                                    path, name, r2 );
                            else {
                                self -> dad -> report . incompleteGapKfg = true;
                                STestEnd ( self, tests & KDIAGN_TRY_TO_WARN
                                                    ? eWarning : eEndFAIL,
                                           "not found" );
                            }
                        }
                        else
                            STestEnd ( self, eEndOK, "OK" );
                        RELEASE ( KConfigNode, node );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/cache-enabled", path, name );
                        STestStart ( self, false, 0, "%s caching:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL, "cannot open "
                                    "'%s/%s/cache-enabled': %R",
                                    path, name, r2 );
                            else {
                                STestEnd ( self, eEndOK, "not found: OK" );
                                r2 = 0;
                            }
                        }
                        else {
                            String sFalse;
                            CONST_STRING ( & sFalse, "false" );
                            r2 = KConfigNodeReadString ( node, & p );
                            if ( r2 != 0 )
                                STestEnd ( self, eEndFAIL, "cannot read "
                                    "'%s/%s/cache-enabled': %R",
                                    path, name, r2 );
                            else if ( StringEqual ( p, & sFalse ) )
                                STestEnd ( self, eWarning,
                                    "caching is disabled" );
                            else
                                STestEnd ( self, eEndOK, "OK" );
                            RELEASE ( String, p );
                        }
                        RELEASE ( KConfigNode, node );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/download-ticket", path, name );
                        STestStart ( self, false, 0,
                                     "%s download-ticket:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL, "cannot read "
                                    "'%s/%s/download-ticket': %R",
                                    path, name, r2 );
                            else {
                                self -> dad -> report . incompleteGapKfg = true;
                                STestEnd ( self, tests & KDIAGN_TRY_TO_WARN
                                                    ? eWarning : eEndFAIL,
                                           "not found" );
                            }
                        }
                        else
                            STestEnd ( self, eEndOK, "OK" );
                        RELEASE ( KConfigNode, node );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/encryption-key", path, name );
                        STestStart ( self, false, 0,
                                     "%s encryption-key:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL, "cannot read "
                                    "'%s/%s/encryption-key': %R",
                                    path, name, r2 );
                            else {
                                r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                                    "%s/%s/encryption-key-path", path, name );
                                if ( r2 != 0 ) {
                                    if ( r2 != SILENT_RC ( rcKFG, rcNode,
                                        rcOpening, rcPath, rcNotFound ) )
                                    {
                                        STestEnd ( self, eEndFAIL,
                                            "cannot read "
                                            "'%s/%s/encryption-key-path': %R",
                                            path, name, r2 );
                                    }
                                    else {
                                        self -> dad -> report . incompleteGapKfg
                                            = true;
                                        STestEnd ( self,
                                            tests & KDIAGN_TRY_TO_WARN
                                                ? eWarning : eEndFAIL,
                                            "not found" );
                                    }
                                }
                            }
                        }
                        if ( r2 == 0 )
                            STestEnd ( self, eEndOK, "OK" );
                        RELEASE ( KConfigNode, node );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    {
                        rc_t r2 = KConfigOpenNodeRead ( self -> kfg, & node,
                            "%s/%s/root", path, name );
                        STestStart ( self, false, 0, "%s root:", name );
                        if ( r2 != 0 ) {
                            if ( r2 != SILENT_RC ( rcKFG, rcNode, rcOpening,
                                                   rcPath, rcNotFound ) )
                                STestEnd ( self, eEndFAIL,
                                           "cannot open '%s/%s/root': %R",
                                           path, name, r2 );
                            else
                                STestEnd ( self, tests & KDIAGN_TRY_TO_WARN
                                                    ? eWarning : eEndFAIL,
                                           "not found" );
                        }
                        else {
                            r2 = KConfigNodeReadString ( node, & p );
                            if ( r2 != 0 )
                                STestEnd ( self, eEndFAIL,
                                           "cannot read '%s/%s/root': %R",
                                           path, name, r2 );
                            else if ( p -> size == 0 )
                                STestEnd ( self, tests & KDIAGN_TRY_TO_WARN
                                                    ? eWarning : eEndFAIL,
                                    "'%s' root path is empty", name );
                            else {
                                KPathType type = kptFirstDefined;
                                type = KDirectoryPathType
                                    ( self -> dir, p -> addr ) & ~ kptAlias;
                                if ( type == kptNotFound )
                                    STestEnd ( self, eWarning, "'%s' root path "
                                        "does not exist: '%S'", name, p );
                                else if ( type != kptDir )
                                    STestEnd ( self,
                                        tests & KDIAGN_TRY_TO_WARN
                                            ? eWarning : eEndFAIL,
                                        "'%s' root path "
                                        "is not a directory: '%S'", name, p );
                                else
                                    STestEnd ( self, eEndOK, "'%S': OK", p );
                            }
                            RELEASE ( String, p );
                            RELEASE ( KConfigNode, node );
                        }
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    if ( r1 == 0 ) {
                        rc_t r2 = STestEnd ( self, eOK, name );
                        if ( r2 != 0 && r1 == 0 )
                            r1 = r2;
                    }
                    else if ( _RcCanceled ( r1 ) )
                        STestEnd ( self, eCANCELED, name );
                    else {
                        self -> dad -> report . incompleteGapKfg = true;
                        STestEnd ( self, eFAIL, name );
                    }
                }
            }
            RELEASE ( KNamelist, names  );
            RELEASE ( KConfigNode, nProtected  );
            if ( ! printed ) {
                printed = true;
                if ( r1 == 0 ) {
                    rc_t r2 = STestEnd ( self, eOK, "Protected repositories" );
                    if ( r2 != 0 && r1 == 0 )
                        r1 = r2;
                }
                else if ( _RcCanceled ( r1 ) )
                    STestEnd ( self, eCANCELED,
                                         "Protected repositories: CANCELED" );
                else
                    STestEnd ( self, eFAIL, "Protected repositories" );
            }
            if ( r1 != 0 && rc == 0 )
                rc = r1;
        }
    }
    return rc;
}

static rc_t CC STestRun ( STest * self, uint64_t tests,
    const KFile * kart, uint32_t numberOfKartItemsToCheck,
    bool checkHttp, bool checkAspera, bool checkDownload, const char * acc,
    uint32_t projectId, va_list args )
{
    rc_t rc = 0;

    assert ( self );

    if ( tests & KDIAGN_CONFIG ) {
        rc_t r1 = 0;
        STestStart ( self, true, KDIAGN_CONFIG, "Configuration" );
        if ( tests & KDIAGN_KFG_NO_GAP && ! _RcCanceled ( r1 ) ) {
            rc_t r2 = STestCheckNoGapKfg ( self, tests );
            if ( r1 == 0 && r2 != 0 )
                r1 = r2;
        }
        if ( tests & KDIAGN_REPO_GAP && ! _RcCanceled ( r1 ) ) {
            rc_t r2 = 0;
            STestStart ( self, true, KDIAGN_REPO_GAP, "DbGaP configuration" );
            r2 = STestCheckGapKfg ( self, tests );
            if ( r2 == 0 )
                r2 = STestEnd ( self, eOK,  "DbGaP configuration" );
            else {
                if ( _RcCanceled ( r2 ) )
                    STestEnd ( self, eCANCELED,
                                            "DbGaP configuration: CANCELED" );
                else
                    STestEnd ( self, eFAIL, "DbGaP configuration" );
            }
            if ( r1 == 0 && r2 != 0 )
                r1 = r2;
        }
        if ( r1 == 0 )
            r1 = STestEnd ( self, eOK,      "Configuration" );
        else {
            if ( _RcCanceled ( r1 ) )
                STestEnd ( self, eCANCELED, "Configuration: CANCELED" );
            else
                STestEnd ( self, eFAIL,     "Configuration" );
        }
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }

    if ( tests & KDIAGN_NETWORK && ! _RcCanceled ( rc ) ) {
        rc_t r1 = 0;
        bool tooBig = false;
        String run;
        char http [ PATH_MAX ] = "";
        uint64_t httpSize = 0;
        if ( acc != NULL )
            StringInitCString ( & run, acc );
        else
            CONST_STRING ( & run, "SRR029074" );
        STestStart ( self, true, KDIAGN_NETWORK, "Network" );
        if ( tests & KDIAGN_ACCESS_NCBI && ! _RcCanceled ( r1 ) ) {
            rc_t r2 = 0;
            STestStart ( self, true, KDIAGN_ACCESS_NCBI, "Access to NCBI" );
            r2 = STestCheckNcbiAccess ( self, tests );
            if ( r2 == 0 )
                r2 = STestEnd ( self, eOK,      "Access to NCBI" );
            else {
                if ( _RcCanceled ( r2 ) )
                    STestEnd ( self, eCANCELED, "Access to NCBI: CANCELED" );
                else {
                    STestEnd ( self, eFAIL,     "Access to NCBI" );
                    self -> dad -> report . firewall = true;
                }
            }
            if ( r1 == 0 && r2 != 0 )
                r1 = r2;
        }
        if ( tests & KDIAGN_HTTP && ! _RcCanceled ( r1 ) ) {
            rc_t r2 = 0;
            STestStart ( self, true, KDIAGN_HTTP, "HTTPS download" );
            if ( tests & KDIAGN_HTTP_RUN ) {
                const char sra  [] = "NCBI.sra\210\031\003\005\001\0\0\0";
                const char nenc [] = "NCBInenc\210\031\003\005\002\0\0\0";
                const char * exp [] = { sra, nenc };
                size_t s = sizeof sra - 1;
                r2 = STestCheckHttp ( self, tests, & run, false,
                    http, sizeof http, & httpSize, exp, s, & tooBig );
            }
            if ( r2 == 0 )
                r2 = STestEnd ( self, eOK,      "HTTPS download" );
            else {
                if ( _RcCanceled ( r2 ) )
                    STestEnd ( self, eCANCELED, "HTTPS download: CANCELED" );
                else
                    STestEnd ( self, eFAIL,     "HTTPS download" );
            }
            if ( r1 == 0 && r2 != 0 )
                r1 = r2;
        }
        if ( tests & KDIAGN_ASCP && ! _RcCanceled ( r1 ) ) {
            rc_t r2 = 0;
            char fasp [ PATH_MAX ] = "";
            uint64_t faspSize = 0;
            String smallrun;
            const String * frun = & run;
            STestStart ( self, true, KDIAGN_ASCP, "Aspera download" );
            if ( tooBig ) {
                CONST_STRING ( & smallrun, "SRR029074" );
                frun = & smallrun;
            }

            if ( tests & KDIAGN_ASCP_RUN )
                r2 = STestCheckFasp ( self, tests,
                    frun, false, fasp, sizeof fasp, & faspSize );
            if ( r2 == 0 )
                r2 = STestEnd ( self, eOK,      "Aspera download" );
            else {
                if ( _RcCanceled ( r2 ) )
                    STestEnd ( self, eCANCELED, "Aspera download: CANCELED" );
                else
                    STestEnd ( self, eFAIL,     "Aspera download" );
            }
            if ( r1 == 0 && r2 != 0 )
                r1 = r2;
            if ( tests & KDIAGN_HTTP_VS_ASCP && ! tooBig &&
                 r2 == 0 && httpSize != 0 && faspSize != 0 )
            {
                r2 = STestHttpVsFasp ( self, http, httpSize, fasp, faspSize );
                if ( r1 == 0 && r2 != 0 )
                    r1 = r2;
            }
            if ( * fasp != '\0' ) {
                rc_t r2 = KDirectoryRemove ( self-> dir, false, fasp );
                if ( r2 != 0 ) {
                    STestFail ( self, r2, 0, "FAILURE: cannot remove '%s': %R",
                                          fasp, r2 );
                    if ( r1 == 0 && r2 != 0 )
                        r1 = r2;
                }
                else
                    * fasp = '\0';
            }
        }
        if ( * http != '\0' ) {
            rc_t r2 = KDirectoryRemove ( self-> dir, false, http );
            if ( r2 != 0 ) {
                STestFail ( self, r2, 0, "FAILURE: cannot remove '%s': %R",
                                      http, r2 );
                if ( r1 == 0 && r2 != 0 )
                    r1 = r2;
            }
            else
                * http = '\0';
        }
        if ( r1 == 0)
            r1 = STestEnd ( self, eOK,  "Network" );
        else  if ( _RcCanceled ( r1 ) )
            STestEnd ( self, eCANCELED, "Network: CANCELED" );
        else
            STestEnd ( self, eFAIL,     "Network" );
        if ( rc == 0 && r1 != 0 )
            rc = r1;
    }

    if ( rc == 0 && tests & KDIAGN_FAIL )
      rc = 1;

  /*if ( rc == 0)
        STestEnd ( & t, eOK,       "System" );
    else  if ( _RcCanceled ( rc ) )
        STestEnd ( & t, eCANCELED, "System: CANCELED" );
    else
        STestEnd ( & t, eFAIL,     "System" );

    STestFini ( & t );
    KDiagnoseRelease ( self );*/
    return rc;
}

static rc_t CC KDiagnoseRunImpl ( KDiagnose * self, const char * name,
    uint64_t tests, const KFile * kart, uint32_t numberOfKartItemsToCheck,
    bool checkHttp, bool checkAspera, bool checkDownload,
    const char * acc, uint32_t projectId, ... )
{
    rc_t rc = 0;
    STest t;
    va_list args;
    va_start ( args, projectId );
    if ( self == NULL )
        rc = KDiagnoseMakeExt ( & self, NULL, NULL, NULL, NULL );
    else
        rc = KDiagnoseAddRef ( self );
    if ( rc != 0 )
        return rc;

    assert ( self );

    STestInit ( & t, self );

    STestStart ( & t, true, KDIAGN_ALL, name );

    rc = STestRun ( & t, tests, kart, numberOfKartItemsToCheck,
        checkHttp, checkAspera, checkDownload, acc, projectId, args );
    va_end ( args );
    if ( rc == 0 && tests & KDIAGN_FAIL )
      rc = 1;

    if ( rc == 0)
        STestEnd ( & t, eOK,       name );
    else  if ( _RcCanceled ( rc ) )
        STestEnd ( & t, eCANCELED, "%s: CANCELED", name );
    else
        STestEnd ( & t, eFAIL,     "%s", name );

    STestFini ( & t );
    KDiagnoseRelease ( self );
    return rc;
}

LIB_EXPORT rc_t CC KDiagnoseAll ( KDiagnose * self, uint64_t tests ) {
    return KDiagnoseRunImpl ( self, "System", KDIAGN_ALL, NULL, 0,
                              true, true, true, NULL, 0 );
}

LIB_EXPORT
rc_t CC KDiagnoseAdvanced ( KDiagnose * self, uint64_t tests )
{
    return KDiagnoseRunImpl ( self, "System", tests, NULL, 0,
                              true, true, true, NULL, 0 );
}

LIB_EXPORT rc_t CC KDiagnoseAcc ( KDiagnose * self,  const char * acc,
    uint32_t projectId, bool checkHttp, bool checkAspera, bool checkDownload,
    uint64_t tests )
{
    return KDiagnoseRunImpl ( self, "System", tests, NULL, 0,
                              checkHttp, checkAspera, checkDownload, acc, 0 );
}

/*DIAGNOSE_EXTERN rc_t CC KDiagnoseDbGap ( KDiagnose * self, uint64_t tests,
    uint32_t projectId, ... )
{
    rc_t rc = 0;
    va_list args;
    va_start ( args, projectId );
    rc = KDiagnoseRunImpl ( self, tests, NULL, 0, NULL, projectId, args );
    va_end ( args );
    return rc;
}*/

DIAGNOSE_EXTERN rc_t CC KDiagnoseKart ( KDiagnose * self,
    const struct KFile * kart, uint32_t numberOfKartItemsToCheck,
    bool checkHttp, bool checkAspera, uint64_t tests )
{
    return KDiagnoseRunImpl ( self, "Kart file", KDIAGN_ALL, kart,
        numberOfKartItemsToCheck, checkHttp, checkAspera, true, NULL, 0 );
}
