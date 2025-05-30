#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>

#define SRC_LOC_DEFINED 1

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/xcdefs.h>
#include <kfc/except.h>
#include <klib/printf.h>
#include <vdb/manager.h>
#include <vdb/vdb-priv.h>

#include <kapp/args.h>

#include <iostream>

#include <stdarg.h>

#define FAKE_CTX_RECOVER 0
#define FAKE_CTX_ERROR 0
#define FAKE_MAKE_VDATABASE 0
namespace ngs
{
    static
    void run ()
    {
#if 1
        ReadCollection run = ncbi :: NGS :: openReadCollection ( "SRR000123" );
#elif 1
        ReadCollection run = ncbi :: NGS :: openReadCollection ( "SRR1063272" );
#endif
        ReadIterator it = run . getReads ( Read :: all );

        String run_name = run . getName ();

        int i;
        for ( i = 0;
#if 0
              i < 5 &&
#endif
              it . nextRead (); ++ i )
        {
            StringRef read_name = it . getReadName ();
            StringRef bases = it . getReadBases ();
            StringRef qual = it . getReadQualities ();


            std :: cout
                << '@'
                << run_name
                << '.'
                << it . getReadId ()
                << ' '
                << read_name
                << " length="
                << bases . size ()
                << std :: endl
                << bases
                << std :: endl
                << '+'
                << run_name
                << '.'
                << it . getReadId ()
                << ' '
                << read_name
                << " length="
                << qual . size ()
                << std :: endl
                << qual
                << std :: endl;
        }

        std :: cerr
            << "Read "
            << i
            << " spots for "
            << run_name
            << std :: endl
            << "Written "
            << i
            << " spots for "
            << run_name
            << std :: endl;

    }
}

extern "C"
{
#if FAKE_CTX_RECOVER
    ctx_t ctx_recover ( KCtx * new_ctx, const KFuncLoc * func_loc, uint32_t rsrc_bits )
    {
        static KCtx fake_ctx;
        static KRsrc fake_rsrc;
        static bool initialized;

        if ( ! initialized )
        {
            VDBManagerMakeRead ( ( const VDBManager** ) & fake_rsrc . vdb, NULL );
            VDBManagerOpenKDBManagerRead ( fake_rsrc . vdb, ( const KDBManager** ) & fake_rsrc . kdb );
            fake_ctx . rsrc = & fake_rsrc;
            fake_ctx . loc = func_loc;
            initialized = true;
        }

        * new_ctx = fake_ctx;
        return new_ctx;
    }
#endif

#if FAKE_CTX_ERROR
    static
    void print_stack_trace ( ctx_t ctx )
    {
        if ( ctx != NULL )
        {
            print_stack_trace ( ctx -> caller );
            std :: cerr
                << ctx -> loc -> src -> mod
                << '/'
                << ctx -> loc -> src -> file
                << '.'
                << ctx -> loc -> src -> ext
                << ':'
                << ctx -> loc -> func
                << std :: endl
                ;
        }
    }

    static
    void print_xc ( xc_t xc )
    {
        const XCErr * err = ( const XCErr* ) xc;
        std :: cerr
            << err -> name
            << ": "
            ;

        while ( err -> dad != NULL )
            err = err -> dad;

        const XCObj * obj = err -> obj;
        while ( obj -> dad != NULL )
            obj = obj -> dad;

        const XCState * state = err -> state;
        while ( state -> dad != NULL )
            state = state -> dad;

        std :: cerr
            << obj -> desc
            << ' '
            << state -> desc
            << ": "
            ;
    }

    static
    rc_t make_rc ( rc_t rc_ctx, xc_t xc )
    {
        const XCErr * err = ( const XCErr* ) xc;
        while ( err -> dad != NULL )
            err = err -> dad;

        const XCObj * obj = err -> obj;
        while ( obj -> dad != NULL )
            obj = obj -> dad;

        const XCState * state = err -> state;
        while ( state -> dad != NULL )
            state = state -> dad;

        return rc_ctx | ( obj -> rc_obj << 6 ) | state -> rc_state;
    }

    void ctx_error ( ctx_t ctx, uint32_t lineno, xc_sev_t sev, xc_t xc, const char *msg, ... )
    {
        static const char * severity_strings [ 3 ] = { "SYSTEM", "INTERNAL", "USER" };
        char buffer [ 4096 ];
        va_list args;

        va_start ( args, msg );

        print_stack_trace ( ctx -> caller );
        std :: cerr
            << ctx -> loc -> src -> mod
            << '/'
            << ctx -> loc -> src -> file
            << '.'
            << ctx -> loc -> src -> ext
            << ':'
            << ctx -> loc -> func
            << ':'
            << lineno
            << ": "
            << severity_strings [ sev ]
            << " ERROR: "
            ;

        print_xc ( xc );

        string_vprintf ( buffer, sizeof buffer, NULL, msg, args );

        va_end ( args );

        std :: cerr
            << buffer
            << std :: endl
            ;

        KCtx * mctx = const_cast < KCtx* > ( ctx );
        mctx -> rc = make_rc ( ctx -> loc -> rc_context, xc );
        mctx -> annotated = mctx -> error = true;
        while ( 1 )
        {
            mctx = const_cast < KCtx* > ( mctx -> caller );
            if ( mctx == 0 )
                break;
            if ( mctx -> error )
            {
                if ( ! mctx -> clear_error_stop )
                    mctx -> clear_error_stop = true;
                break;
            }

            assert ( ! mctx -> clear_error_stop );
            mctx -> error = true;
        }
    }
#endif

#if FAKE_MAKE_VDATABASE
    NGS_ReadCollection * NGS_ReadCollectionMakeVDatabase ( ctx_t ctx, struct VDatabase const *db, const char * spec )
    {
        return 0;
    }
#endif

    int main ( int argc, char * argv [] )
    {
        try
        {
            ngs :: run ();
        }
        catch ( ngs :: ErrorMsg & x )
        {
            :: std :: cerr
                << "ngs :: Error - "
                << x . what ()
                << std :: endl;
        }
        catch ( ... )
        {
            :: std :: cerr
                << "unknown error"
                << std :: endl;
        }

        return 0;
    }
}

