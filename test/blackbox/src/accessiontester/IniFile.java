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

import java.util.*;
import java.io.*;

public class IniFile
{
    private String filename;
    private final String hint;
    private final Properties props;
    private boolean valid;

    private int to_int( String s, int dflt )
    {
        int res;
        try { res = Integer.parseInt( s ); }
        catch ( Exception e ) { res = dflt; }
        return res;
    }

    private long to_long( String s, long dflt )
    {
        long res;
        try { res = Long.parseLong( s ); }
        catch ( Exception e ) { res = dflt; }
        return res;
    }

    private boolean to_bool( String s, boolean dflt )
    {
        boolean res;
        try { res = Boolean.parseBoolean( s ); }
        catch ( Exception e ) { res = dflt; }
        return res;
    }

    /**
     *
     * @param key  ... key to identify the key/value pair
     * @param dflt ... default value
     * @return ....... value if found / dflt if not found
     */
    public final String get_str( String key, String dflt )
    {
        String res = props.getProperty( key );
        if ( res == null )
            res = dflt;
        else if ( res.isEmpty() )
            res = dflt;
        return res;
    }

    /**
     *
     * @param key ... key to identify the key/value pair
     * @return ...... value if found / empty string if not found
     */
    public final String get_str( String key )
    {
        String res = props.getProperty( key );
        if ( res == null ) res = "";
        return res;
    }
    
    /**
     *
     * @param key  ... key to identify the key/value pair
     * @param dflt ... default value
     * @return ....... value as integer if found/converted else dflt
     */
    public final int get_int( String key, int dflt ) { return to_int( props.getProperty( key ), dflt ); }
    
    /**
     *
     * @param key ... key to identify the key/value pair
     * @return ...... value as integer if found/converted else 0
     */
    public final int get_int( String key ) { return get_int( key, 0 ); }
    
    /**
     *
     * @param key  ... key to identify the key/value pair
     * @param dflt ... default value
     * @return ....... value as long if found/converted else dflt
     */
    public final long get_long( String key, long dflt ) { return to_long( props.getProperty( key ), dflt ); }
    
    /**
     *
     * @param key ... key to identify the key/value pair
     * @return ...... value as long if found/converted else 0
     */
    public final long get_long( String key ) { return get_long( key, 0 ); }
    
    /**
     *
     * @param key  ... key to identify the key/value pair
     * @param dflt ... default value
     * @return ....... value as boolean if found/converted else dflt
     */
    public final boolean get_bool( String key, boolean dflt ) { return to_bool( props.getProperty( key ), dflt ); }
    
    /**
     *
     * @param key ... key to identify the key/value pair
     * @return ...... value as boolean if found/converted else false
     */
    public final boolean get_bool( String key ) { return get_bool( key, false ); }
    
    /**
     *
     * @param key   ... key to identify the key/value pair
     * @param value ... string-value of the key/value pair
     */
    public final void set_str( String key, String value ) { props.setProperty( key, value ); }

    /**
     *
     * @param key   ... key to identify the key/value pair
     * @param value ... integer-value of the key/value pair
     */
    public final void set_int( String key, int value ) { props.setProperty( key, Integer.toString( value ) ); }

    /**
     *
     * @param key   ... key to identify the key/value pair
     * @param value ... long-value of the key/value pair
     */
    public final void set_long( String key, long value ) { props.setProperty( key, Long.toString( value ) ); }

    /**
     *
     * @param key   ... key to identify the key/value pair
     * @param value ... boolean-value of the key/value pair
     */
    public final void set_bool( String key, boolean value ) { props.setProperty( key, Boolean.toString( value ) ); }
    
    /**
     * remove all stored key/value pairs
     */
    public void clear()
    {
        filename = "";
        props.clear();
    }
    
    /**
     *
     * @return success of storing
     */
    public final boolean store()
    {
        boolean res = false;
        if ( !filename.isEmpty() )
        {
            try
            {
                FileOutputStream s = new FileOutputStream( filename );
                props.store( s, hint );
                s.close();
                res = true;
            }
            catch ( Exception e ) { }
        }
        return res;
    }

    /**
     *
     * @param new_filename ... file to be stored into
     * @return success
     */
    public final boolean store_to( String new_filename )
    {
        boolean res = false;
        if ( !new_filename.isEmpty() )
        {
            filename = new_filename;
            res = store();
        }
        return res;
    }
            
    /**
     *
     * @return success of loading
     */
    public final boolean load()
    {
        boolean res = false;
        if ( !filename.isEmpty() )
        {
            try
            {
                FileInputStream s = new FileInputStream( filename );
                props.load( s );
                s.close();
                res = true;
            }
            catch ( Exception e ) { }
        }
        return res;
    }
    
    /**
     *
     * @param new_filename ... file to load from
     * @return ... success of loading
     */
    public final boolean load_from( String new_filename )
    {
        boolean res = false;
        if ( !new_filename.isEmpty() )
        {
            filename = new_filename;
            res = load();
            valid = res;
        }
        return res;
    }
    
    /**
     *
     * @return returns true if ini-file could be loaded form given filename
     *  or state was set via set_valid()
     */
    public boolean is_valid() { return valid; }
    
    /**
     *
     * @param value ... flag to set if the ini-file is considered valid
     */
    public void set_valid( boolean value ) { valid = value; }
    
    /**
     *
     * @return the name of the backing file
     */
    public String get_filename() { return filename; }
    
    /**
     * deletes the file that is backing the ini-file
     * 
     * @return success of deletion
     */
    public boolean delete_file()
    {
        boolean res = false;
        if ( !filename.isEmpty() )
        {
            File f = new File( filename );
            res = f.delete();
        }
        return res;
    }
    
    /**
     * returns a list of string-values for the given key
     * 
     * @param key ... ini-key
     * @return generic list of values for the given key
     */
    public List <String> get_list( String key )
    {
        List< String > res = new ArrayList<>();
        String v = get_str( key );
        if ( !v.isEmpty() )
        {
            String a[] = v.split( " " );
            for ( int i = 0; i < a.length; ++i )
                res.add( a[ i ] );
        }
        return res;
    }

    /**
     * writes a list or String-values into key-value entry
     * 
     * @param key ...... ini - key
     * @param values ... list of values for key
     */
    public void set_list( String key, List< String > values )
    {
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        for ( String item : values )
        {
            if ( !first )
                sb.append( " " );
            else
                first = false;
            sb.append( item );
        }
        set_str( key, sb.toString() );
    }
    
    /**
     * creates an IniFile from a file and loads all the values
     *
     * @param filename ... full path to ini-file 
     * @param hint ....... hint to be written into the head of the ini-file
     */
    public IniFile( String filename, String hint )
    {
        this.filename = filename;
        this.hint = hint;
        props = new Properties();
        valid = load();
    }

    /**
     * creates an IniFile without a file backing it ( yet )
     * 
     * @param hint ... hint to be written into the head of the ini-file
     */
    public IniFile( String hint )
    {
        this.filename = "";
        this.hint = hint;
        props = new Properties();
        valid = false;
    }
    
}
