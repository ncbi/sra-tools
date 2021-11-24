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

import ngs.ErrorMsg;

/** The Logger is used to print log messages to stderr.
 An associated "Level" reflects a minimum Level that this logger cares about. */
class Logger {

    enum Level {
        OFF    (0), // OFF that can be used to turn off logging
        WARNING(1), // WARNING is a message level indicating a potential problem
        INFO   (2), // INFO is a message level for informational messages.
        FINE   (3), // FINE is a message level providing tracing information
        FINER  (4), // FINER indicates a fairly detailed tracing message
        FINEST (5); // FINEST indicates a highly detailed tracing message

        private Level(int id) { this.id = id; }
        private int id() { return id; }
        private final int id;
    }

    /** Default logging Level is WARNING.
        It could be changed by setting "vdb.log" java system property */
    private Logger() {
        String s = System.getProperty("vdb.log");
        if (s == null || s.equals("WARNING")) {
            level = Level.WARNING;
        } else if (s.equals("OFF")) {
            level = Level.OFF;
        } else if (s.equals("INFO")) {
            level = Level.INFO;
        } else if (s.equals("FINE")) {
            level = Level.FINE;
        } else if (s.equals("FINER")) {
            level = Level.FINER;
        } else if (s.equals("FINEST")) {
            level = Level.FINEST;
        } else {
            level = Level.FINE;
        }
    }

    static void warning(String msg) { log(Level.WARNING, msg); }
    static void info   (String msg) { log(Level.INFO   , msg); }
    static void fine   (String msg) { log(Level.FINE   , msg); }
    static void finer  (String msg) { log(Level.FINER  , msg); }
    static void finest (String msg) { log(Level.FINEST , msg); }

    static void fine   (Throwable e) { fine(e.toString()); }
    static void finest (Throwable e) { finest(e.toString()); }

    static void finest (String[] msgs) {
        if (msgs == null) {
            return;
        }
        String msg = null;
        for (String s : msgs) {
            if (msg == null) {
                msg = "";
            } else {
                msg += " ";
            }
            msg += s;
        }
        finest(msg);
    }

    static void log(Level level, String msg) { logger.go(level, msg); }

    static Level getLevel() { return logger.level; }
    static void setLevel(Level newLevel) { logger.level = newLevel; }

    private void go(Level level, String msg) {
        if (this.level.id() < level.id()) {
            return;
        }
        String v = getVersion();

        String formatted = "ngs-java" + (v != null ? "." + v : "") + ": " + msg;
        System.err.println(formatted);
    }

    private String getVersion() {
        if (version == null) {
            try {
                version = ngs.Package.getPackageVersion();
            } catch (Throwable e) {
                version = null;
            }
        }
        return version;
    }

    private String version;
    private Level level;
    private static Logger logger = new Logger();
}
