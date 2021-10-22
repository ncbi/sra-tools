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


package gov.nih.nlm.ncbi.ngs;


import ngs.ErrorMsg;
import ngs.ReadCollection;
import ngs.ReferenceSequence;


/*==========================================================================
 * NGS
 *  static implementation root
 */
public class NGS
{

    /**
     * Check to see if NGS SDK is supported by current environment
     * @return true if NGS SDK is supported
     */
    static public boolean isSupported ()
    {
        return mgr . isSupported ();
    }

    /**
     * Returns exception which occurred during initialization
     *  If the exception is a subclass of LibraryLoadError, then there was a problem
     *  with loading native libraries
     *
     * @return initialization error or null
     */
    static public ExceptionInInitializerError getInitializationError ()
    {
        return mgr . invalid;
    }


    /**
     * Updates User-Agent header in HTTP communications
     *
     * @param app_version gives app name and version, e.g. "pileup-stats.1.0.0"
     */
    static public void setAppVersionString ( String app_version )
    {
        mgr . setAppVersionString ( app_version );
    }


    /**
     * Create an object representing a named collection of reads
     *
     * @param spec may be a path to an object or may be an id, accession, or URL
     * @return the requested read-collection
     * @throws ErrorMsg if object cannot be located
     * @throws ErrorMsg if object cannot be converted to a ReadCollection
     * @throws ErrorMsg if an error occurs during construction
     */
    static public ReadCollection openReadCollection ( String spec )
        throws ErrorMsg
    {
        return mgr . openReadCollection ( spec );
    }


    /**
     * Create an object representing a named reference sequence
     *
     * @param spec may be a path to an object or may be an id, accession, or URL
     * @return the requested reference
     * @throws ErrorMsg if object cannot be located
     * @throws ErrorMsg if object cannot be converted to a ReadCollection
     * @throws ErrorMsg if an error occurs during construction
     */
    static public ReferenceSequence openReferenceSequence ( String spec )
        throws ErrorMsg
    {
        return mgr . openReferenceSequence ( spec );
    }


    /**
     * Check to see if spec string represents an SRA archive
     *
     * @param spec may be a path to an object or may be an id, accession, or URL
     * @return true spec represents an SRA archive
     */
    static public boolean isValid ( String spec )
    {
        return mgr . isValid ( spec );
    }


    private static Manager mgr = new Manager ();


    private static void test(String s, boolean expected) {
        if (isValid(s) != expected) {
            System.err.println("ERRRRRRROR isValid(" + s + ") = " + ! expected);
            System.exit(1);
        } else if (false) {
            System.err.println("isValid(" + s + ") = " + expected);
        }
    }


    public static void main(String[] args) {
        String h = System.getenv("HOME") + "/A/";
        try {
            isValid(null);
            System.exit(1);
        } catch (NullPointerException e) {}
        test( ""        , false);
        test( "SRR00000", false);
        test( "SRR000000", false);
        test( "SRR000001", true); // table
        test("https://sra-download.ncbi.nlm.nih.gov/srapub/SRR000001", true);
        test( "SRR499924", true); // db
        test("https://sra-download.ncbi.nlm.nih.gov/srapub/SRR499924", true);
        test("SRR9000000", false);
        test("ERR000000", false);
        test("ERR000002", true);
        test("ERR900000", false);
        test("DRR000000", false);
        test("DRR000003", true);
        test("DRR900000", false);
        test(h + "notExisting", false);
        test(h + "empty"      , false);
        test(h + "text"       , false);
        test(h + "SRR053325.f", true); // tbl; file
        test(h + "SRR053325"  , true); // tbl; dir
        test(h + "SRR600096.f", true); // db; file
        test(h + "SRR600096"  , true); // db; dir
        test("https://w.gov/", false); // bad host
        test("https://www.nih.gov/", false); // exists
        test("https://sra-download.ncbi.nlm.nih.gov/srapub/", false); // ! exists
    }

}
