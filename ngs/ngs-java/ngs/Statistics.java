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

package ngs;


/**
 * Statistical data container
 */
public interface Statistics
{

    /**
     * ValueType
     */
    static int none = 0;
    static int string = 1;
    static int int64 = 2;
    static int uint64 = 3;
    static int real = 4;
    
    /**
     * getValueType
     * @param path is hierarchical path to value node
     * @return one of { none, string, int64, uint64, real }
     */
    int getValueType ( String path );
    
    /**
     * getAsString
     * @param path is hierarchical path to value node
     * @return textual representation of value
     * @throws ErrorMsg if path not found or value cannot be converted
     */
    String getAsString ( String path )
        throws ErrorMsg;

    /**
     * getAsI64
     * @param path is hierarchical path to value node
     * @return a signed 64-bit integer
     * @throws ErrorMsg if path not found or value cannot be converted
     */
    long getAsI64 ( String path )
        throws ErrorMsg;
            
    /**
     * getAsU64
     * @param path is hierarchical path to value node
     * @return a non-negative 64-bit integer
     * @throws ErrorMsg if path not found or value cannot be converted
     */
    long getAsU64 ( String path )
        throws ErrorMsg;
        
    /**
     * getAsDouble
     * @param path is hierarchical path to value node
     * @return a 64-bit floating point
     * @throws ErrorMsg if path not found or value cannot be converted
     */
    double getAsDouble ( String path )
        throws ErrorMsg;
            
    /**
     * nextPath
     * advance to next path in container
     * @param path is null or empty to request first path, or a valid path string
     * @return null if no more paths, or a valid path string
     */
    String nextPath ( String path );
}
