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


import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.Vector;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LibVersionChecker {
    static Version getVersion(String libname, String libpath, boolean useLoadLibrary) {
        Vector<String> cmdarray = new Vector<String>();
        String property = System.getProperty("java.home");
        if (property != null) {
            cmdarray.add(property + LibPathIterator.fileSeparator()
                    + "bin" + LibPathIterator.fileSeparator() + "java");
            if (!tryJava(cmdarray)) {
                cmdarray.remove(0);
            }
        }
        if (cmdarray.size() == 0) {
            cmdarray.add("java");
            if (!tryJava(cmdarray)) {
                // come up with exception class
                throw new RuntimeException("Failed to check library " + libpath + " version: failed to execute java");
            }
        }

        String classpath = System.getProperty("java.class.path");
        if (classpath != null) {
            cmdarray.add("-cp");
            cmdarray.add(classpath);
        }
        cmdarray.add(createPropertyString("java.library.path"));
        if (System.getProperty("vdb.log") != null) {
            cmdarray.add(createPropertyString("vdb.log"));
        }
        cmdarray.add("gov.nih.nlm.ncbi.ngs.LibVersionChecker");
        cmdarray.add(libname);
        cmdarray.add(libpath);
        if (useLoadLibrary) {
            cmdarray.add("true");
        }

        Logger.finer(">>> RUNNING CHILD ...");
        String version = null;
        try {
            String cmd[] = new String[cmdarray.size()];
            for (int i = 0; i < cmdarray.size(); ++i) {
                cmd[i] = cmdarray.elementAt(i);
            }
            Logger.finest(cmd);
            Process p = Runtime.getRuntime().exec(cmd);
            BufferedReader bri =
                    new BufferedReader(new InputStreamReader(p.getInputStream()));
            BufferedReader bre =
                    new BufferedReader(new InputStreamReader(p.getErrorStream()));
            String line = null;
            while ((line = bre.readLine()) != null) {
                System.err.println(line);
            }
            bre.close();
            while ((line = bri.readLine()) != null) {
                Pattern pattern = Pattern.compile("^LibManager: version='(.*)'$");
                Matcher matcher = pattern.matcher(line);
                while (matcher.find()) {
                    version = matcher.group(1);
                    if (version != null) {
                        break;
                    }
                }
                if (version == null) {
                    System.out.println(line);
                }
            }
            bri.close();
            p.waitFor();
        } catch (Exception e) { Logger.finest(e); }
        Logger.finer("<<< Done CHILD");
        if (version != null) {
            return new Version(version);
        }
        return null;
    }

    /** Call checkLib for every argument to the version of local dll,
     compare it with the latest available
     and download the latest if it is more recent */
    public static void main(String[] args) {
        LibVersionChecker checker = new LibVersionChecker();

        if (args.length != 2 && args.length != 3) {
            throw new RuntimeException("Not enough arguments: should be 2 or 3");
        }

        String libname = args[0];
        String libpath = args[1];
        boolean useLoadLibrary = args.length == 3 && args[2].equals("true");
        String version = checker.checkLib(libname, libpath, useLoadLibrary);
        if (version != null) {
            System.out.println("LibManager: version='" + version + "'");
        }

    }

    /** Check the version of local dll,
     compare it with the latest available;
     download the latest if it is more recent */
    private String checkLib(String libname, String path, boolean useLoadLibrary) {
        Logger.finest("> Checking the version of " + path  + " library...");

        Logger.finest(">> Loading the library...");
        boolean loaded = false;
        try {
            if (useLoadLibrary) {
                System.loadLibrary(path);
            } else {
                System.load(path);
            }
            loaded = true;
        } catch (UnsatisfiedLinkError e) {
            Logger.finest("<< Failed to load library " + path);
            Logger.finest(e);
        }

        String version = null;
        if (loaded) {
            Logger.finest(">> Checking current version of the library...");
            version = getLoadedVersion(libname);

            Logger.finest("<< The current version of " + path + " = " + version);
        }

        Logger.finest("< Done checking version of the library");

        return version;
    }

    static String getLoadedVersion(String libname) {
        try {
            if (libname.equals("ncbi-vdb")) {
                return Manager.getPackageVersion();
            } else if (libname.equals("ngs-sdk")) {
                return ngs.Package.getPackageVersion();
            } else {
                Logger.warning("It is not known how to check "
                        + "the version of " + libname + " library");
                return null;
            }
        } catch (ngs.ErrorMsg e) {
            Logger.finest(e);
        } catch (UnsatisfiedLinkError e) {
            Logger.finest(e);
        }
        return null;
    }

    /** Make sure we can execute java */
    private static boolean tryJava(Vector<String> cmdarray) {
        try {
            Process p
                    = Runtime.getRuntime().exec(cmdarray.elementAt(0) + " -version");
            if (p.waitFor() == 0) {
                return true;
            }
        } catch (Exception e) {}
        return false;
    }

    /** Create java property option */
    private static String createPropertyString(String key) {
        String property = System.getProperty(key);
        if (property == null) {
            throw new RuntimeException("Property " + key + " is not defined");
        }
        return "-D" + key + "=" + property + "";
    }

}
