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

import java.io.File;


/** Library Path Iterator is used to find location of JNI DLL.
  It iterates over all file names generated using LibManager.Location [array] */
class LibPathIterator {

    private static final String NCBI_NGS_JAR_NAME = "ngs-java.jar";


    /** Creates an iterator that will go through all LibManager's location-s */
    LibPathIterator(LibManager mgr,
        String filename[])
    {
        this(mgr, null, filename, false);
    }


    /** Creates an iterator that will go through all LibManager's location-s;
        if parents == true then create file's parent directory */
    LibPathIterator(LibManager mgr,
        String filename[],
        boolean parents)
    {
        this(mgr, null, filename, parents);
    }


    /** Creates an iterator that will go through just a single location */
    LibPathIterator(LibManager.Location location,
        String filename[])
    {
        this(null, location, filename, false);
    }


    LibPathIterator(LibManager mgr,
        LibManager.Location location,
        String filename[],
        boolean parents)
    {
//      Logger.finer("\n");
        Logger.finer("Creating LibPathIterator(" + toString(filename) + ", "
            + location + ")...");

        if (mgr == null && location == null) {
            throw new NullPointerException();
        }

        if (filename == null) {
            throw new NullPointerException();
        }

        this.mgr = mgr;
        this.location = location;
        this.filenames = filename;

        this.parents = parents;
        this.separator = fileSeparator();

        reset();
    }


    /** Get next file name-candidate */
    String nextName()
    {
        while (true) {
            if ((paths == null || iPaths >= paths.length) // done iterating
                                                          // current location 
                && !reset())    // done iterating all locations
            {
                return null;
            }

            String path = paths[iPaths];

            String filename = null;
            if (filenames != null) {
                filename = filenames[iFilename];
            }

            if (filenames == null) {
                ++iPaths;
            } else if (++iFilename >= filenames.length) {
                iFilename = 0;
                ++iPaths;
            }
            if (path.length() == 0) { // ignore empty PATH elements
                continue;
            }

            LibManager.Location crn = location;
            if (crn == null) {
                crn = crnLocation;
            }

            if (separator == null) {
                throw new NullPointerException();
            }
            return path += separator + filename;
        }
    }


    /** Iterate over all locations and print them: debug/test */
    static boolean list( String libname )
    {
        return iterate(new LibManager(new String[] {}, new String[] {}), libname, false);
    }


    /** delete all libraries found by the LibManager */
    static boolean deleteLibraries(LibManager mgr, String libname)
    {
        return iterate(mgr, libname, true);
    }


    /** Resets the LibPathIterator to use the next (first) location */
    private boolean reset()
    {
        if (location != null) { // this object iterates just over one location
            if (crnLocation != null) {
                return false; // iterated over the requested location
            } else {
                crnLocation = location;
            }
        } else { // this object iterates over all locations
            if (crnLocation == null) {
                iLocation = -1; // constructor call : use the first location
            }
        }

        while (true) {
            // TODO(andrii): check if that will work with new changes
            ++iLocation;
            Logger.finest("LibPathIterator.reset() " + iLocation);
            if (location == null) {
                if (iLocation >= mgr.locations().length) {
                    return false; // iterated over all locations
                }
                crnLocation = mgr.locations()[iLocation];
                Logger.finest("LibPathIterator.reset(" + crnLocation + ")");
            } else if (iLocation > 1) {
                return false; /* A single location was requested;
                                 already processed it: break this loop */
            }
            iPaths = 0;
            String pathSeparator = pathSeparator();
            String path = null;
            switch (crnLocation) {
                case CLASSPATH:
                    path = System.getProperty("java.class.path");
                    Logger.finest("java.class.path = " + path);
                    if (!resetPaths(path, pathSeparator)) {
                        continue;
                    }
                    return true;
                case CWD:
                    if (!resetPaths(System.getProperty("user.dir"),
                        pathSeparator))
                    {
                        continue;
                    }
                    return true;
                case LIBPATH:
                    path = System.getProperty("java.library.path");
                    Logger.finest("java.library.path = " + path);
                    if (!resetPaths(path, pathSeparator)) {
                        continue;
                    }
                    return true;
                case NCBI_HOME:
                    if (!resetPaths(ncbiLibHome())) {
                        continue;
                    }
                    return true;
                case NCBI_NGS_JAR_DIR:
                    path = findJarDir(pathSeparator);
                    if (path == null) {
                        Logger.finest(NCBI_NGS_JAR_NAME + " was not found");
                        continue;
                    } else {
                        Logger.finest("jar directory is " + path);
                        resetPaths(path);
                        return true;
                    }
                case TMP:
                    path = findTmpDirEnv();
                    if (path != null) {
                        Logger.fine("Use " + path + " as temporary directory");
                        resetPaths(path);
                        return true;
                    }
                    Logger.finest(
                        "Temporary directory was not found in environment");
                    if (resetPathsToTmpDirs()) {
                        Logger.finest("Found a temporary directory");
                        return true;
                    } else {
                        Logger.fine("Could not find any temporary directory");
                        continue;
                    }
            }
        }
    }


    /** Find directory where ncbi-ngs.jar is located */
    private String findJarDir(String pathSeparator)
    {
        String path = System.getProperty("java.class.path");
        Logger.finest("java.class.path = " + path);
        if (path == null) {
            return null;
        }

        String[] paths = path.split(pathSeparator);
        for (int i = 0; i < paths.length; ++i) {
            Logger.finest("findJarDir() " + i + " " + paths[i]);
            if (paths[i].endsWith(NCBI_NGS_JAR_NAME)) {
                File f = new File(paths[i]);
                if (!f.exists()) {
                    System.err.println(NCBI_NGS_JAR_NAME
                        + " from class path does not exist");
                    continue;
                }

                Logger.finest("Found NCBI jar file: " + paths[i]);
                String dir = null;
                if (NCBI_NGS_JAR_NAME.equals(paths[i])) {
                    dir = System.getProperty("user.dir");
                    if (dir == null) {
                        System.err.println("cannot get directory "
                            + "for " + NCBI_NGS_JAR_NAME);
                        continue;
                    }
                } else {
                    dir = paths[i].substring(0, paths[i].length()
                        - NCBI_NGS_JAR_NAME.length());
                }
                String fileSeparator = fileSeparator();
                if (fileSeparator != null && fileSeparator.length() == 1 &&
                    dir.length() > 0 &&
                    dir.charAt(dir.length() - 1) == fileSeparator.charAt(0))
                {   dir = dir.substring(0, dir.length() - 1); }
                return dir;
            }
        }

        return null;
    }


    /** Find a temporary directory */
    private String findTmpDirEnv() {
        String name = "TMPDIR";
        String path = System.getenv(name);
        if (path != null) {
            Logger.finest(name + " = " + path);
            return path;
        }

        name = "TEMP";
        path = System.getenv(name);
        if (path != null) {
            Logger.finest(name + " = " + path);
            return path;
        }

        name = "TMP";
        path = System.getenv(name);
        if (path != null) {
            Logger.finest(name + " = " + path);
            return path;
        }

        name = "TEMPDIR";
        path = System.getenv(name);
        if (path != null) {
            Logger.finest(name + " = " + path);
            return path;
        }

        return null;
    }


    private boolean resetPathsToTmpDirs() {
        int cnt = 0;

        String var = "/var/tmp";
        File fVar = new File(var);
        if (fVar.exists()) {
            ++cnt;
        } else {
            fVar = null;
        }

        String tmp = "/tmp";
        File fTmp = new File(tmp);
        if (fTmp.exists()) {
            ++cnt;
        } else {
            fTmp = null;
        }

        if (cnt == 0) {
            return false;
        }

        if (paths == null || paths.length != cnt) {
            paths = new String[cnt];
        }

        int i = 0;
        if (fVar != null) {
            paths[i++] = var;
        }
        if (fTmp != null) {
            paths[i++] = tmp;
        }

        return true;
    }


    /** Sets paths = path */
    private boolean resetPaths(String path) { return resetPaths(path, null); }


    /** Resets paths by splitting path using regex */
    private boolean resetPaths(String path,
        String regex)
    {
        if (path == null) {
            return false;
        }

        if (regex != null) {
            paths = path.split(regex);

// In the classpath there could be file names and/or relative elements.
// TODO what is there is a relative directory in the classpath?
            if (crnLocation == LibManager.Location.CLASSPATH) {
                for (int i = 0; i < paths.length; ++i) {
                    File elm = new File(paths[i]);
                    if (elm.isFile()) { /* Replace file path by its parent
TODO should it be absolute? System.loadLibrary uses it property directly.
Here we use it just to find where to write the downoaded file. */
                        String p = elm.getParent();
                        if (p == null) { // parent is null when elm is just a 
                          // leaf file name. I.e. it is in the current directory
                            p = System.getProperty("user.dir");
                        }

                        if (p == null) {
                            System.err.println(
                                "cannot get parent directory for " + paths[i]);
                            paths[i] = "."; // TODO test use . during f.download
                        } else {
                            paths[i] = p;
                        }
                    }
                }
            }
        }
        else {
            if (paths == null || paths.length != 1) {
                paths = new String[1];
            }

            paths[0] = path;
        }

        return true;
    }

    private boolean mkdir(String path, boolean closed)
    {
        File f = new File(path);

        if (!f.exists()) {
            try {
                f.mkdirs();
            } catch (SecurityException e) {
                Logger.fine(e);
                return false;
            }

            if (closed) {
                try {
                    f.setExecutable(false, false);
                } catch (SecurityException e) {
                    Logger.fine(e);
                }
                try {
                    f.setReadable(false, false);
                } catch (SecurityException e) {
                    Logger.fine(e);
                }
                try {
                    f.setWritable(false, false);
                } catch (SecurityException e) {
                    Logger.fine(e);
                }
            }

            try {
                f.setExecutable(true, true);
            } catch (SecurityException e) {
                Logger.fine(e);
            }
            try {
                f.setReadable(true, true);
            } catch (SecurityException e) {
                Logger.fine(e);
            }
            try {
                f.setWritable(true, true);
            } catch (SecurityException e) {
                Logger.fine(e);
            }
        }

        return true;
    }


    /** [Configuration] /NCBI_HOME (~/.ncbi/). */
    static String ncbiHome() {
        String path = System.getProperty("user.home");
        if (path == null) {
            return null;
        }

        return path + fileSeparator() + ".ncbi";
    }


    /** [Libraries in] NCBI_HOME (~/.ncbi/libXX/). Create it if required. */
    private String ncbiLibHome()
    {
        String path = ncbiHome();
        if (path == null) {
            return null;
        }

        if (parents) {
            if (!mkdir(path, true)) {
                return null;
            }
        }

        path += fileSeparator() + "lib";
        switch (LibManager.detectJVM()) {
            case b64:
                path += "64";
                break;
            case b32:
                path += "32";
                break;
        }

        if (parents) {
            if (!mkdir(path, false)) {
                return null;
            }
        }

        return path;
    }


    /** Path separator character used in java.class.path */
    private static String pathSeparator()
    {
        String separator = System.getProperty("path.separator");
        if (separator == null) {
            String os = System.getProperty("os.name");
            if (os != null && os.indexOf("win") >= 0) {
                separator = ";";
            }
        }
        if (separator == null) {
            separator = ":";
        }
        return separator;
    }


    private String toString(String[] s) {
        if (s == null) {
            throw new NullPointerException();
        }
        String r = "[";
        for (int i = 0; i < s.length; ++i) {
            if (i > 0) {
                r += ", ";
            }
            r += s[i];
        }
        r += "]";
        return r;
    }

    /** Separates components of a file path. "/" on UNIX and "\" on Windows. */
    static String fileSeparator()
    {
        String separator = System.getProperty("file.separator");
        if (separator == null) {
            String os = System.getProperty("os.name");
            if (os != null && os.indexOf("win") >= 0) {
                separator = "\\";
            }
        }
        if (separator == null) {
            separator = "/";
        }
        return separator;
    }


    private static boolean iterate(LibManager mgr, String libname, boolean rm)
    {
        boolean removed = rm ? false : true;

//      LibManager mgr = new LibManager("ncbi-ngs-jni");
//      String filename[] = LibManager.mapLibraryName("ncbi-ngs-jni");
        LibPathIterator it =
            new LibPathIterator(mgr, LibManager.mapLibraryName( libname ));
        String s = null;

        int i = 0;
        while ((s = it.nextName()) != null) {
            File f = new File(s);
            boolean delete = f.exists();
            System.err.println("(" + ++i + ") " + s + " "
                + (delete ? "+" : "-"));
            if (rm && delete) {
                try {
                    delete = f.delete();
                    if (delete) {
                        removed = delete;
                    } else {
                        System.err.println("Cannot delete '" + s + "'");
                    }
                } catch (SecurityException e) {
                    System.err.println("Cannot delete '" + s + "': " + e);
                }
            }
        }

        return removed;
    }


    // mgr has list of library locations (not used if location is set below)
    private LibManager mgr;

    private LibManager.Location location; // requested location; all if null

    private LibManager.Location crnLocation; // current location
    private int iLocation; // current location idx in LibManager.location[]

    private String[] paths;         // list of paths (directories) to iterate
    private int iPaths;             // index in paths

    private String separator;       // path separator

    private String filenames[];     // file names
    private int iFilename;          // index in file name

    private String abspath;         // absolute file path

    // if is true and file's parent directory does not exist then create it
    private boolean parents;
}
