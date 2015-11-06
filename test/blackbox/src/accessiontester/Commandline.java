/* ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
=========================================================================== */

package accessiontester;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Commandline_rule
{
    public final boolean has_value;
    public final List< String > values;
    public final String dflt;
            
    public String[] get_values()
    {
        String[] res = null;
        if ( values.size() > 0 )
        {
            res = new String[ values.size() ];
            values.toArray( res );
        }
        else if ( dflt != null && !dflt.isEmpty() )
        {
            res = new String[ 1 ];
            res[ 0 ] = dflt;
        }
        return res;
    }

    public String get_value( final int nr )
    {
        String res = null;
        if ( values.size() > 0 )
        {
            if ( values.size() >= ( nr + 1 ) )
                res = values.get( nr );
        }
        else if ( dflt != null && !dflt.isEmpty() )
        {
            res = dflt;
        }
        return res;
    }

    public void report( final String key )
    {
        System.out.print( String.format( "%s: ", key ) );
        for ( String s : values )
            System.out.print( String.format( "%s ", s ) );
        System.out.println();
    }
    
    public Commandline_rule( final boolean has_value, final String dflt )
    {
        this.has_value = has_value;
        this.dflt = dflt;
        values = new ArrayList<>();
    }
}

public class Commandline
{
    private final Map< String , Commandline_rule > rules;
    private final List< String > arguments;
    
    /**
     * adds a parsing rule for a value-option
     * 
     * @param dflt ... dflt value for option ( can be null )
     * @param keys ... list of option-keys like ( "-p", "--path", "/path" )
     */
    public void add_value_rule( final String dflt, final String... keys )
    {
        Commandline_rule rule = new Commandline_rule( true, dflt );
        for ( String key : keys )
            rules.put( key, rule );
    }

    /**
     * adds a parsing rule for a flag-option
     * 
     * @param keys ... list of option-keys like ( "-h", "--help", "/help" )
     */
    public void add_flag_rule( final String... keys )
    {
        Commandline_rule rule = new Commandline_rule( false, null );
        for ( String key : keys )
            rules.put( key, rule );
    }
    
    /**
     * 
     * @return ..... all arguments as String-array
     */
    public String[] get_arguments()
    {
        String[] res = new String[ arguments.size() ];
        arguments.toArray( res );
        return res;
    }

    /**
     *
     * @param nr ... which argument on the commandline
     * @return ..... requested arguments if available or null
     */
    public String get_argument( final int nr )
    {
        if ( arguments.size() >= ( nr + 1 ) )
            return arguments.get( nr );
        return null;
    }

    /**
     *
     * @return ...... how many arguments have been parsed
     */
    public int get_argument_count() { return arguments.size(); }
    
    /**
     *
     * @param key ... option to look for ( example "-t" or "--path" )
     * @return ...... all values as String array
     */
    public String[] get_options( final String key )
    {
        Commandline_rule rule = rules.get( key );
        if ( rule != null )
            return rule.get_values();
        return null;
    }
    
    /**
     *
     * @param key ... option to look for ( example "-t" or "--path" )
     * @param nr .... which value if key appeared multiple times on input
     * @return ...... value for key if available or null
     */
    public String get_option( final String key, final int nr )
    {
        Commandline_rule rule = rules.get( key );
        if ( rule != null )
            return rule.get_value( nr );
        return null;
    }

    /**
     *
     * @param key ... option to look for ( example "-t" or "--path" )
     * @return ...... first value if available or null
     */
    public String get_option( final String key )
    {
        return get_option( key, 0 );
    }
    
    /**
     *
     * @param key ... option to look for ( example "-t" or "--path" )
     * @return ...... number of values for the given key
     */
    public int get_option_count( final String key )
    {
        Commandline_rule rule = rules.get( key );
        if ( rule != null )
            return rule.values.size();
        return 0;
    }
    
    /**
     * parses the given args according to the specified rules
     * fills the internal structures with the results of the parsing
     * 
     * @param args ... commandline arguments
     */
    public void parse( final String[] args )
    {
        Commandline_rule rule = null;
        for ( String arg : args )
        {
            if ( rule == null )
            {
                rule = rules.get( arg );
                if ( rule == null )
                    arguments.add( arg );
                else if ( !rule.has_value )
                {
                    rule.values.add( arg );
                    rule = null;
                }
            }
            else
            {
                rule.values.add( arg );
                rule = null;
            }
        }
    }
    
    /**
     * reports the given commandline arguments/options for debugging
     */
    public void report()
    {
        for ( Map.Entry< String, Commandline_rule > entry : rules.entrySet() )
        {
            String key = entry.getKey();
            Commandline_rule rule = entry.getValue();
            rule.report( key );
        }
        System.out.print( "arguments: " );
        for ( String s : arguments )
            System.out.print( String.format( "%s ", s ) );
        System.out.println();
    }
    
    /**
     * creates an empty commandline-parser, rules have to be added
     * and parsing has to be done, then values can be retrieved...
     */
    public Commandline()
    {
        rules = new HashMap<>();
        arguments = new ArrayList<>();
    }
}