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

#ifndef _h_outpost_args_
#define _h_outpost_args_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <vector>
#include <map>

#include <ngs/ErrorMsg.hpp>
#include <ngs/StringRef.hpp>

/* ##########################################################
   # Big WARNING : those aren't thread safe classes
   ########################################################## */

/*))
 // Right namespace?
((*/
namespace ngs {

/*)))
 ///    These are simple adapters for methods and structures introduced
 \\\    in kapp/args.h. I agree, these are looks lame, but I will use
 ///    them ... 
(((*/

/*))    Adapter for OptDef ... 
 ((*/
class AOptDef {
private :
    typedef struct OptDef OptDef;

public :
        /*) Various constructors and destructors 
         (*/
    AOptDef ();
    AOptDef ( const AOptDef & Opt );
    ~AOptDef ();

    bool good () const;

    AOptDef & operator = ( const AOptDef & Opt );

    void reset ( const AOptDef & Opt );
    void reset ();

        /*) Various setters/getters
         (*/
    inline const char * getName ( ) const
                {
                    return _M_name . empty ()
                                    ? NULL
                                    : _M_name . c_str ()
                                    ;
                };
    inline void setName ( const char * Name = NULL )
                {
                    if ( Name == NULL )
                        _M_name . clear ();
                    else 
                        _M_name = Name;
                };

    inline const char * getAliases ( ) const
                {
                    return _M_aliases . empty ()
                                    ? NULL
                                    : _M_aliases . c_str ()
                                    ;
                };
    inline void setAliases ( const char * Aliases = NULL )
                {
                    if ( Aliases == NULL )
                        _M_aliases . clear ();
                    else 
                        _M_aliases = Aliases;
                };

    inline const char * getParam ( ) const
                {
                    return _M_param . empty ()
                                    ? NULL
                                    : _M_param . c_str ()
                                    ;
                };
    inline void setParam ( const char * Param = NULL )
                {
                    if ( Param == NULL )
                        _M_param . clear ();
                    else 
                        _M_param = Param;
                };

    inline uint16_t maxCount () const
                {
                    return _M_max_count;
                };
    inline void setMaxCount ( uint16_t MaxCount = 0 )
                {
                    _M_max_count = MaxCount;
                };

    inline bool needValue () const
                {
                    return _M_need_value;
                };
    inline void setNeedValue ( bool NeedValue = true )
                {
                    _M_need_value = NeedValue;
                };

    inline bool required () const
                {
                    return _M_required;
                };
    inline void setRequired ( bool Required = true )
                {
                    _M_required = Required;
                 };

    inline const char * getHlp () const
                {
                    return _M_hlp.c_str ();
                };

    inline void setHlp ( const String & HlpStr = "" )
                {
                    _M_hlp = HlpStr;
                };
private :

    String _M_name;
    String _M_aliases;
    String _M_param;
    String _M_hlp;
    uint16_t _M_max_count;
    bool _M_need_value;
    bool _M_required;
};  /* class AOptDef */

/*))    Something extra for the same money
 ((*/
class AOPBase {
public :
    typedef struct Args Args;

    typedef std :: vector < String > VVal;
    typedef VVal :: const_iterator VValI;

public :
    AOPBase ();
    virtual ~AOPBase ();

    virtual void reset (
                    const String & Name,
                    Args * TheArgs,
                    bool needValue
                    ) = 0;

    inline bool exist () const
            {
                return _M_exist;
            };

    inline const String & name () const
            { 
                return _M_name;
            };

    inline bool hasVal() const
            {
                return valCount () != 0;
            };

    uint32_t valCount () const;
    const String & val ( uint32_t idx = 0 ) const;
    uint32_t uint32Val ( uint32_t idx = 0 ) const;
    int32_t int32Val ( uint32_t idx = 0 ) const;
    uint64_t uint64Val ( uint32_t idx = 0 ) const;
    int64_t int64Val ( uint32_t idx = 0 ) const;

protected :
    void reset ();
    void reset ( const AOPBase & Bse );

    bool _M_exist;

    String _M_name;

    VVal _M_val;
};  /* class AOPBase */

class AOptVal : public AOPBase {
public :
    AOptVal ();
    AOptVal ( const AOptVal & Val );

    AOptVal & operator = ( const AOptVal & Val );

    void reset ( const String & Name, Args * TheArgs, bool needValue );
};  /* class AOptVal */

class AParVal : public AOPBase {
public :
    AParVal ();
    AParVal ( const AParVal & Val );

    AParVal & operator = ( const AParVal & Val );

    void reset ( const String & Name, Args * TheArgs, bool needValue );
};  /* class AParVal */

/*))))  Adapter for Args structure.
 ////
 \\\\   Nothing new, it fully repeats behaviour of Args** methods.
 ////   There is pattern how to use it :
 \\\\       a.init (); // initializes Args
 ////       for ( .... ) {
 \\\\           a.addOpt ( Opt );
 ////       }
 \\\\       a.parseArgs ( argc, argv );
 ////       a.dispose ();
((((*/
class AArgs {
private :
    typedef struct Args Args;

    typedef std :: map < Args * , AArgs * > MArgs;
    typedef MArgs :: iterator MArgsI;

    typedef std :: vector < AOptDef > VOpt;
    typedef VOpt :: const_iterator VOptI;

    typedef std :: vector < String > VVal;
    typedef VVal :: const_iterator VValI;

public :
    AArgs ();
    virtual ~AArgs ();

        /* Three general stepd to perform : init/parse/dispose
         */
    void init ( bool AddStandardOrguments = false );
    void parse ( int argc, char ** argv );
    void dispose ();

        /*  Are we good ? virtual ... ouch
         */
    inline bool good () const { return _M_args != NULL; };

        /*  Options handling
         */
    void addStdOpts ();
    void addOpt ( const AOptDef & Opt );

    inline size_t optDefCount () const
            { return _M_optDefs . size (); };
    inline const AOptDef & optDef ( size_t i )
            { return _M_optDefs [ i ]; };

        /*  Here is program name
         */
    inline const String & prog () const
            { return _M_progName; };

        /* Here are getters for option and paramter values 
         */
    AOptVal optVal ( const char * name ) const;
    AParVal parVal () const;

    void usage () const;

protected :
    virtual void __customInit ();
    virtual void __customParse ();
    virtual void __customDispose ();

private :
    static MArgs _sM_args;

public :
    static void __regArgs ( Args * TheArgs, AArgs * TheAArgs );
    static AArgs * __getArgs ( Args * TheArgs ); 
    static void __deregArgs ( Args * TheArgs ); 

private :
    const AOptDef & optDef ( const String & name ) const;

    void __disposeArgs ( Args * TheArgs );
    void __addStdOpts ( Args * TheArgs );

    Args * _M_args;
    bool _M_standards_added;

    VOpt _M_optDefs;

    String _M_progName;
};  /* class AArgs */

};  /* namespace ngs */

#endif /* _h_outpost_args_ */
