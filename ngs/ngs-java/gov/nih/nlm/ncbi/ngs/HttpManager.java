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
* ==============================================================================
*
*/


package gov.nih.nlm.ncbi.ngs;


import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;


/** Helper class responsible for HTTP/HTTPS-related activities:
    download files using GET/POST */
class HttpManager
{
    /** POST implementation. Returns response text */
    static String post(String spec,
            String request)
        throws HttpException
    {
        Logger.fine(spec + "?" + request + "...");
        
        ByteArrayOutputStream result = new ByteArrayOutputStream();

        InputStream is = getPostInputStream(spec, request);
        byte[] buffer = new byte[1024];
        int l = 0;
        try {
            while ((l = is.read(buffer)) != -1) {
                result.write(buffer, 0, l);
            }
            is.close();
        } catch (IOException e) {
            throw new HttpException(-3);
        }

        try {
            return result.toString("UTF-8");
        } catch (java.io.UnsupportedEncodingException e) {
            System.err.println(e);
            throw new HttpException(-4);
        }
    }


    /** POST implementation. Calls download() to process the response. */
    static int post(String spec,
        String request,
        FileCreator creator,
        String libname)
    {
        Logger.fine(spec + "?" + request + " -> " + libname + "...");

        try {
            InputStream in = getPostInputStream(spec, request);

            if (download(in, creator, libname)) {
                return 200;
            } else {
                return -3;
            }
        } catch (HttpException e) {
            return e.getResponseCode();
        }
    }


    /** GET implementation. Calls download() to process the response. */
    private static boolean get(String spec,
        FileCreator creator,
        String libname)
    {
        System.err.println(spec + " ->" + libname + "...");

        URL url = null;
        try {
            url = new URL(spec);
        } catch (java.net.MalformedURLException e) {
            System.err.println("Bad URL: " + spec + ": " + e);
            return false;
        }

        InputStream in = null;
        try {
            in = url.openStream();
        } catch (IOException e) {
            System.err.println("Cannot download " + spec + ": " + e);
            return false;
        }

        return download(in, creator, libname);
    }


    private static InputStream getPostInputStream(String spec,
            String request)
        throws HttpException
    {
        URL url = null;
        try {
            url = new URL(spec);
        } catch (java.net.MalformedURLException e) {
            System.err.println("Bad URL: " + spec + ": " + e);
            throw new HttpException(-1);
        }

        InputStream in = null;
        try {
            java.net.URLConnection urlConn = url.openConnection();
            urlConn.setDoInput(true);
            urlConn.setDoOutput (true);
            urlConn.setUseCaches(false);
            urlConn.setRequestProperty("Content-Type",
                "application/x-www-form-urlencoded");

            java.io.OutputStream out = urlConn.getOutputStream();
            DataOutputStream printout = new DataOutputStream(out);
            printout.writeBytes(request);
            printout.flush();
            printout.close();

            in = urlConn.getInputStream();

            HttpURLConnection httpConnection = (HttpURLConnection) urlConn;
            int status = httpConnection.getResponseCode();
            if (status != 200) {
                throw new HttpException(status);
            }
        } catch (IOException e) {
            System.err.println(e);
            throw new HttpException(-2);
        }

        return in;
    }


    /** Download InputStream, use FileCreator to create output file */
    private static boolean download(InputStream in,
         FileCreator creator, String libname)
    {
        boolean success = false;
        java.io.BufferedOutputStream bout = creator.create( libname );
        if (bout == null) {
            System.err.println("Not possible to create a file for downloading");
            creator.done(success);
            return success;
        }

        BufferedInputStream bin = new BufferedInputStream(in);
        byte data[] = new byte[BUF_SZ];

        while (true) {
            int count = 0;
            try {
                count = bin.read(data, 0, BUF_SZ);
            } catch (IOException e) {
                System.err.println(e);
                creator.done(success);
                return success;
            }
            if (count == -1) {
                break;
            }

            try {
                bout.write(data, 0, count);
            } catch (IOException e) {
                System.err.println(e);
                creator.done(success);
                return success;
            }
//          break;
        }

        try {
            bout.close();
        } catch (IOException e) {
            System.err.println(e);
            creator.done(success);
            return success;
        }

        success = true;
        creator.done(success);
        return success;
    }


    /** Size of buffer for HTTP/HTTPS-related buffered IO operations */
    static final int BUF_SZ = 128 * 1024;
}
