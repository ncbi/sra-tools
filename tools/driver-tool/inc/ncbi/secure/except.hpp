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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#pragma once

#include <ncbi/secure/types.hpp>

#include <string>
#include <ostream>

/**
 * @file ncbi/secure/except.hpp
 * @brief Exception classes
 *
 *  A common exception family for this library's exception types.
 */

namespace ncbi
{

    /*=====================================================*
     *                      FORWARDS                       *
     *=====================================================*/

    class XP;
    class XBackTrace;


    /*=====================================================*
     *                     ReturnCodes                     *
     *=====================================================*/

    enum ReturnCodes
    {
        rc_okay,
        rc_param_err,
        rc_init_err,
        rc_logic_err,
        rc_runtime_err,
        rc_input_err,
        rc_protocol_err,
        rc_test_err
    };
    
    /*=====================================================*
     *                        XMsg                         *
     *=====================================================*/
    
    /**
     * @struct XMsg
     * @brief an object to hold a well-defined block of memory for msg responses
     *
     * Used as a return value to avoid pointers
     */
    struct XMsg
    {
        size_t msg_size;            //!< size of "zmsg" excluding NUL termination
        UTF8 zmsg [ 512 ];          //!< NUL-terminated string of UTF-8 characters
    };


    /*=====================================================*
     *                      Exception                      *
     *=====================================================*/
    
    /**
     * @class Exception
     * @brief base class for errors
     */
    class Exception
    {
    public:

        /*=================================================*
         *                     GETTERS                     *
         *=================================================*/

        /**
         * what
         * @return const XMsg with formatted explanation of "what went wrong"
         */
        const XMsg what () const noexcept;

        /**
         * problem
         * @return const XMsg of "what is the problem"
         */
        const XMsg problem () const noexcept;

        /**
         * context
         * @return const XMsg of "in what context did it occur"
         */
        const XMsg context () const noexcept;

        /**
         * cause
         * @return const XMsg of "what may have caused the problem"
         */
        const XMsg cause () const noexcept;

        /**
         * suggestion
         * @return const XMsg of "how can I fix/avoid the problem"
         */
        const XMsg suggestion () const noexcept;

        /**
         * file
         * @return const XMsg of file name
         */
        const XMsg file () const noexcept;

        /**
         * line
         * @return line number where object was constructed
         */
        unsigned int line () const noexcept;

        /**
         * function
         * @return const XMsg of simple function name
         */
        const XMsg function () const noexcept;

        /**
         * status
         * @return ReturnCodes for process exit status
         */
        ReturnCodes status () const noexcept;


        /*=================================================*
         *                    C++ STUFF                    *
         *=================================================*/
        
        /**
         * Exception
         * @brief constructor
         * @param XP with all parameters
         */
        explicit Exception ( const XP & params );

        /**
         * operator=
         * @overload copy operator
         */
        Exception & operator = ( const Exception & x );

        /**
         * operator=
         * @overload move operator
         */
        Exception & operator = ( const Exception && x ) = delete;

        /**
         * Exception
         * @overload copy constructor
         */
        Exception ( const Exception & x );

        /**
         * Exception
         * @overload move constructor
         */
        Exception ( const Exception && x ) = delete;

        /**
         * Exception
         * @overload default constructor
         */
        Exception () = delete;

        /**
         * ~Exception
         * @brief destroys message storage strings
         */
        virtual ~ Exception () noexcept;
        
    private:

        void * callstack [ 128 ];
        std :: string prob_msg;
        std :: string ctx_msg;
        std :: string cause_msg;
        std :: string suggest_msg;
        const UTF8 * file_name;
        const ASCII * func_name;
        unsigned int lineno;
        int stack_frames;
        ReturnCodes rc;

        friend class XBackTrace;
    };


    /*=====================================================*
     *                         XP                          *
     *=====================================================*/
    
    /**
     * @class XP
     * @brief a parameter block for constructing an Exception
     *
     * The class name is purposely kept short for convenience
     * of implementation notation.
     */
    class XP
    {
    public:

        XP & operator << ( bool val )
        { putUTF8 ( val ? "true" : "false" ); return * this; }
        XP & operator << ( short int val )
        { return operator << ( ( long long int ) val ); }
        XP & operator << ( unsigned short int val )
        { return operator << ( ( unsigned long long int ) val ); }
        XP & operator << ( int val )
        { return operator << ( ( long long int ) val ); }
        XP & operator << ( unsigned int val )
        { return operator << ( ( unsigned long long int ) val ); }
        XP & operator << ( long int val )
        { return operator << ( ( long long int ) val ); }
        XP & operator << ( unsigned long int val )
        { return operator << ( ( unsigned long long int ) val ); }
        XP & operator << ( long long int val );
        XP & operator << ( unsigned long long int val );
        XP & operator << ( float val )
        { return operator << ( ( long double ) val ); }
        XP & operator << ( double val )
        { return operator << ( ( long double ) val ); }
        XP & operator << ( long double val );
        XP & operator << ( XP & ( * f ) ( XP & f ) )
        { return f ( * this ); }
        XP & operator << ( const UTF8 * s )
        { putUTF8 ( s ); return * this; }
        XP & operator << ( const std :: string & s );
        XP & operator << ( const XMsg & s );

        void putChar ( UTF32 ch );
        void putUTF8 ( const UTF8 * utf8 );
        void putUTF8 ( const UTF8 * utf8, size_t bytes );
        void putPtr ( const void * ptr );
        void setRadix ( unsigned int radix );

        void sysError ( int status );
        void cryptError ( int status );
        void setStatus ( ReturnCodes status );

        void useProblem ();
        void useContext ();
        void useCause ();
        void useSuggestion ();

        XP ( const UTF8 * file_name,
             const ASCII * func_name,
             unsigned int lineno,
             ReturnCodes status = rc_runtime_err );

        XP & operator = ( const XP & xp ) = delete;
        XP & operator = ( const XP && xp ) = delete;

        XP ( const XP & xp ) = delete;
        XP ( const XP && xp ) = delete;

        ~ XP ();

    private:

        void * callstack [ 128 ];
        std :: string problem;
        std :: string context;
        std :: string cause;
        std :: string suggestion;
        std :: string * which;
        const UTF8 * file_name;
        const ASCII * func_name;
        unsigned int lineno;
        unsigned int radix;
        int stack_frames;
        ReturnCodes rc;

        friend class Exception;
    };


    /*=====================================================*
     *                     XBackTrace                      *
     *=====================================================*/
    
    /**
     * @class XBackTrace
     * @brief a C++ style callstack that attempts to show a backtrace
     *
     * NB - may not be available on all platforms,
     * and may be disabled due to policies.
     *
     * Intended for use in debugging.
     */
    class XBackTrace
    {
    public:

        bool isValid () const noexcept;

        const XMsg getName () const noexcept;

        bool up () const noexcept;

        void operator = ( const XBackTrace & bt ) noexcept;
        XBackTrace ( const XBackTrace & bt ) noexcept;

        void operator = ( const XBackTrace && bt ) noexcept;
        XBackTrace ( const XBackTrace && bt ) noexcept;

        XBackTrace ( const Exception & x ) noexcept;

        ~ XBackTrace () noexcept;

    private:

        mutable char ** frames;
        mutable int num_frames;
        mutable int cur_frame;
    };


    /*=====================================================*
     *                  C++ INLINES, ETC.                  *
     *=====================================================*/

    //!< select which message is being populated
    inline XP & xprob ( XP & xp ) { xp . useProblem (); return xp; }
    inline XP & xctx ( XP & xp ) { xp . useContext (); return xp; }
    inline XP & xcause ( XP & xp ) { xp . useCause (); return xp; }
    inline XP & xsuggest ( XP & xp ) { xp . useSuggestion (); return xp; }

    //!< handle an ASCII character
    inline XP & operator << ( XP & xp, char c )
    { xp . putChar ( c ); return xp; }

    //!< handle a UTF32 character
    struct XPChar { UTF32 ch; };
    inline XPChar putc ( UTF32 ch ) { XPChar c; c . ch = ch; return c; }
    inline XP & operator << ( XP & xp, const XPChar & c )
    { xp . putChar ( c . ch ); return xp; }

    //!< handle an arbitrary pointer
    struct XPPtr { const void * ptr; };
    inline XPPtr ptr ( const void * ptr ) { XPPtr p; p . ptr = ptr; return p; }
    inline XP & operator << ( XP & xp, const XPPtr & p )
    { xp . putPtr ( p . ptr ); return xp; }

    //!< modify numeric radix
    inline XP & binary ( XP & xp ) { xp . setRadix ( 2 ); return xp; }
    inline XP & octal ( XP & xp ) { xp . setRadix ( 8 ); return xp; }
    inline XP & decimal ( XP & xp ) { xp . setRadix ( 10 ); return xp; }
    inline XP & hex ( XP & xp ) { xp . setRadix ( 16 ); return xp; }
    struct XPRadix { unsigned int radix; };
    inline XPRadix radix ( unsigned int val ) { XPRadix r; r . radix = val; return r; }
    inline XP & operator << ( XP & xp, const XPRadix & r )
    { xp . setRadix ( r . radix ); return xp; }

    //!< set exit status
    struct XPStatus { ReturnCodes rc; };
    inline XPStatus xstatus ( ReturnCodes rc ) { XPStatus s; s . rc = rc; return s; }
    inline XP & operator << ( XP & xp, const XPStatus & s )
    { xp . setStatus ( s . rc ); return xp; }

    //!< support for system errors
    struct XPSysErr { int status; };
    inline XPSysErr syserr ( int status )
    { XPSysErr e; e . status = status; return e; }
    inline XP & operator << ( XP & xp, const XPSysErr & e )
    { xp . sysError ( e . status ); return xp; }

    //!< support for crypto library
    struct XPCryptoErr { int status; };
    inline XPCryptoErr crypterr ( int status )
    { XPCryptoErr e; e . status = status; return e; }
    inline XP & operator << ( XP & xp, const XPCryptoErr & e )
    { xp . cryptError ( e . status ); return xp; }

    //!< support for XMsg and std::ostream
    inline std :: ostream & operator << ( std :: ostream & o, const XMsg & m )
    { return o << m . zmsg; }

    //!< support for XBackTrace and std::ostream
    std :: ostream & operator << ( std :: ostream & o, const XBackTrace & bt );

    /**
     * format
     * @param fmt a printf-compatible NUL-terminated constant format string
     * @return a std::string populated with printf-style format string
     *
     * This function can be used to stream a formatted std::string into fmt
     */
    inline std :: string format ( const UTF8 * fmt, ... )
        __attribute__ ( ( format ( printf, 1, 2 ) ) );


    /**
     * @def DECLARE_SEC_MSG_EXCEPTION
     * @brief macro for easy declaration of exception classes
     */
#define DECLARE_SEC_MSG_EXCEPTION( class_name, super_class )            \
    struct class_name : super_class                                     \
    {                                                                   \
        class_name ( const Exception & x )                              \
            : super_class ( x )                                         \
        {                                                               \
        }                                                               \
        class_name ( const XP & params )                                \
            : super_class ( params )                                    \
        {                                                               \
        }                                                               \
    }

    /**
     * @def XLOC
     * @brief list of macro args that supply current __FILE__, __func__, __LINE__
     */
#define XLOC __FILE__, __func__, __LINE__


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

    DECLARE_SEC_MSG_EXCEPTION ( LogicException, Exception );
    DECLARE_SEC_MSG_EXCEPTION ( RuntimeException, Exception );

    DECLARE_SEC_MSG_EXCEPTION ( InternalError, LogicException );
    DECLARE_SEC_MSG_EXCEPTION ( UnimplementedError, InternalError );
    DECLARE_SEC_MSG_EXCEPTION ( InsufficientBuffer, InternalError );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidArgument, LogicException );
    DECLARE_SEC_MSG_EXCEPTION ( NullArgumentException, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidStateException, LogicException );
    DECLARE_SEC_MSG_EXCEPTION ( IteratorInvalid, InvalidStateException );
    DECLARE_SEC_MSG_EXCEPTION ( IncompatibleTypeException, LogicException );
    DECLARE_SEC_MSG_EXCEPTION ( UnsupportedException, LogicException );
    DECLARE_SEC_MSG_EXCEPTION ( InternalPolicyViolation, InternalError );

    DECLARE_SEC_MSG_EXCEPTION ( MemoryExhausted, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( BoundsException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( OverflowException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( UnderflowException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( BadCastException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( ConstraintViolation, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( SizeViolation, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( LengthViolation, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( PermissionViolation, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( UniqueConstraintViolation, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( NotFoundException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidInputException, RuntimeException );
    DECLARE_SEC_MSG_EXCEPTION ( PolicyViolation, ConstraintViolation );
}
