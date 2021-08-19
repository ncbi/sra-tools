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


/******************************************************************************/


class Version implements Comparable<Version> {

    Version(String version) {
        stage = Stage.FINAL;
        this.version = version;
        if (version == null) {
            return;
        }

        int value = 0;
        int part = MAJOR;

        for (int i = 0; i < version.length(); ++i) {
            char c = version.charAt(i);
            if (part == STAGE) {
                if (c == '.' || c == '-') {
                    part = REVISION;
                    continue;
                }
                else if (Character.isDigit(c)) {
                    part = REVISION;
                    value = Character.getNumericValue(c);
                    continue;
                }
            }
            if (c == '.' || c == '-') {
                add(part, value);
                value = 0;
                ++part;
                if (part > REVISION) {
                    break;
                }
                
            } else if (c == 'a' || c == 'b' || c == 'r') {
                if (part != STAGE) {
                    add(part, value);
                }
                value = 0;
                if (stage == Stage.FINAL) {
                    switch (c) {
                        case 'a': stage = Stage.ALPHA; break;
                        case 'b': stage = Stage.BETA ; break;
                        case 'r': stage = Stage.RC   ; break;
                    }
                }
                part = STAGE;
            } else if (Character.isDigit(c)) {
                value = value * 10 + Character.getNumericValue(c);
            }
        }
        add(part, value);
    }

    public String toString() {
        return "{ " + s(version) + " = "
            + major + "." + minor + "." + release + stage + revision + " }";
    }

    public String toSimpleVersion() {
        return version == null ? "null" : version;
    }

    public boolean isCompatible(Version other) {
        return major == other.major;
    }

    public int getMajor() {
        return major;
    }

    /** Compares two Version, testing whether one comes before or after the
        other, or whether they're equal. The Version parts are compared.

        Syntax        version.compareTo(version2)

        Parameters    version: a variable of type Version
                      version2: another variable of type Version

        Returns       a negative number: if version comes before version2
                      0                : if version equals version2
                      a positive number: if version comes after version2
     */
    public int compareTo(Version o) { /* if this < o return -1 */
        int r = 1;
        if (o == null) {
            v(o, r, 88);
            return r;
        }
        
        else if (major > o.major) {
            v(o, r, 93);
            return r;
        } else if (major < o.major) {
            r = -1;
            v(o, r, 97);
            return r;
        }
        
        else if (minor > o.minor) {
            v(o, r, 102);
            return r;
        } else if (minor < o.minor) {
            r = -1;
            v(o, r, 106);
            return r;
        }

        else if (release > o.release) {
            v(o, r, 112);
            return r;
        } else if (release < o.release) {
            r = -1;
            v(o, r, 116);
            return r;
        }

        else if (stage.compareTo(o.stage) > 0) {
            v(o, r, 121);
            return r;
        } else if (stage.compareTo(o.stage) < 0) {
            r = -1;
            v(o, r, 125);
            return r;
        }

        else if (revision > o.revision) {
            v(o, r, 130);
            return r;
        } else if (revision < o.revision) {
            r = -1;
            v(o, r, 134);
            return r;
        }

        else {
            r = 0;
            v(o, r, 140);
            return r;
        }
    }

    private void add(int part, int value) {
        switch (part) {
            case MAJOR   : major    = value; break;
            case MINOR   : minor    = value; break;
            case RELEASE : release  = value; break;
            case STAGE   :
            case REVISION: revision = value; break;
            default      :                   break;
        }
    }

    private enum Stage {
        ALPHA(0),
        BETA(1),
        RC(2),
        FINAL(3);

        private Stage(int id) { this.id = id; }

        private final int id;

        public String toString() {
            switch (id) {
                case 0 : return "-a";
                case 1 : return "-b";
                case 2 : return "-rc";
                default: return "-";
            }
        }
    }

    final private static int MAJOR    = 0;
    final private static int MINOR    = 1;
    final private static int RELEASE  = 2;
    final private static int STAGE    = 3;
    final private static int REVISION = 4;

    private int major;
    private int minor;
    private int release;
    private int revision;
    private Stage stage;
    private String version;

    private void v(Version o, int r, int i) {
        if (verbose > 1)
        {   l(this + ".compareTo(" + o + ") == " + r + " #" + i); }
    }

    private static int verbose = 0;

    private static String s(String s)
    {   if (s == null) { return "null"; } return "'" + s + "'"; }

    private static void l(int    s) { System.out.println(s); }
    private static void l(String s) { System.out.println(s); }

    private static void ok(String s)
    {   if (verbose == 0) { return; } l("ok: " + s); }

    private static void ko(String s){l("KO: "+s+" ==========================");}

    private static boolean test(String a, String b, boolean equals) {
        boolean ok = true;
        Version va = new Version(a);
        Version vb = new Version(b);
        int r = va.compareTo(vb);
        if (equals) {
            if (r == 0) {
                ok(s(a) + " == " + s(b));
            } else {
                ko(s(a) + " != " + s(b));
                ok = false;
            }
            if (vb.compareTo(va) != 0) {
                ko(s(b) + " != " + s(a));
                ok = false;
            }
        } else {
            if (r < 0) {
                ok(s(a) + " < " + s(b));
            } else {
                ko(s(a) + " >= " + s(b));
                ok = false;
            }
            if (vb.compareTo(va) <= 0) {
                ko(s(b) + " <= " + s(a));
                ok = false;
            }
        }
        if (verbose > 1) {
            l("test(" + a + ", " + b + ") = " + ok);
        }
        return ok;
    }

    private static boolean testeq(String a, String b)
    {   return test(a, b, true ); }

    private static boolean testlt(String a, String b)
    {   return test(a, b, false); }

    public static void main(String[] args) {
        verbose = args.length;
        boolean ok = true;

        /*if (Version.parse(testFW).compareTo(Version.parse(baseFW)) < 0) {
            // Version is newer!
        }*/

        ok &= testeq(null, null);
        ok &= testeq(null, "");
        ok &= testeq("", "");
        ok &= testeq("0", "");
        ok &= testeq("0", "0");

        ok &= testlt("0", "1");
        ok &= testlt("2", "9");
        ok &= testlt("9", "11");
        ok &= testlt("08", "9");
        ok &= testeq("080", "80");

        ok &= testeq("0.5", ".5.0");
        ok &= testeq("0.5", ".5.0.");
        ok &= testeq("2.", "2");

        ok &= testlt("1.2", "1.2.1");
        ok &= testlt("1", "1.0.1");
        ok &= testlt("1.0.1", "1.1");
        ok &= testlt("1.2.9", "1.3");
        ok &= testlt("1.2.9", "1.3.");
        ok &= testlt("1.2.9", "1.3.0");

        ok &= testlt("1.1.4a", "1.1.4");
        ok &= testeq("2.3.4-a0", "2.3.4.a0");
        ok &= testeq("2.3.4a", "2.3.4.a0");
        ok &= testlt("2.3.4-a0", "2.3.4b");
        ok &= testlt("2.3.4-a1", "2.3.4b1");
        ok &= testlt("2.3.4a2", "2.3.4-b1");
        ok &= testlt("2.3.4-a0", "2.3.4-b3");
        ok &= testlt("2.3.4-a0", "2.3.4-rc1");
        ok &= testlt("2.3.4-rc1", "2.3.4");
        ok &= testlt("2.3.4", "2.3.4-2");

        if (!ok) {
            System.exit(1);
        }
    }
}

/******************************************************************************/
