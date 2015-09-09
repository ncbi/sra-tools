# ===========================================================================
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
# ===========================================================================


The tool 'vdb-config' can be used to inspect or change the configuration
of the sra-toolkit.

When called without any parameters the tool reports the current configuration
in xml-format. No changes are made.

-----------------------------------------------------------------------------

vdb-config --restore-defaults

If called with this parameter the tool will bring the configuration into
default state.

-----------------------------------------------------------------------------

vdb-config -i

This will present the user with a colored configuration dialog.

The tab-key and the cursor-keys navigate the dialog. The item with the little
red square has the focus. A button or a checkbox can be 'pressed' with the
space or enter-key. To get out of the dialog without saving any changes
press the '6'-key or the 'q'-key or navigate to the 'exit'-button at the
bottom of the dialog and press the space or enter-key.


The "data source" part:

The "NCBI SRA" labeled checkbox enables/disables remote access to the SRA-
accession stored at NCBI. As long as the computer has internet-access and this
checkbox is enabled the user can access SRA-accessions directly without
downloading them.

A command like 'sra-pileup SRR341578' at the command-line will produce pileup
output of the given accession even if this accession has not been downloaded
before.
The tool will download the data on the fly from our servers.


There might be a checkbox labeled "site" below the "NCBI-SRA" one. If this
checkbox is not available you do not have a 'site'-installation of SRA-data.
If it is visible you do have such a site-installation and you can disable
access to this data.


The "local workspaces" part:

At the top are 2 buttons "import dbGaP-project" and "set default import path".

If you are not using dbGaP-projects (The database of Genotypes and Phenotypes)
you can ignore these 2 buttons.

The "import dbGaP-project" button presents you with another dialog to select
a ngc-file. You can navigate the directories of your computer to find and
select one of these files. By default the focus is in the files-list. It may
be empty.
Use the cursor-key: 'up' to focus the 'directories'-list. If you press enter
on any of the listed directory-names you change into this directory.
The '[ .. ]' entry brings you back into the parent directory. If you see
ngc-files in the lower 'files'-list press the tab-key to switch to the
'files'-list. Press enter on one of them to select this file for import. You
will see a success-message if the import was performed without errors.
On Windows you cannot switch from one drive-letter
to another when selecting.

The "set default import path" gives you the opportunity to specify a different
default location for dbGaP-projects - for instance if your home directory is
not big enough. You can always change the location for your dbGaP-project
after the import.


Below the 2 buttons is a list of local repositories. If there are no
dbGaP-projects this list has only one entry "Open Access Data". This is the
location where accessions get downloaded and cached. You can change these
locations if for instance your home directory where they are created by
default does not have enough space.
The change button brings up a directory-select dialog.

If you made any changes like enabling/disabling or changed a location, the
change is only written to the configuration if you exit the dialog via the
'save and exit' button.

-----------------------------------------------------------------------------


vdb-config -i --interactive-mode textual

This will present the user with a purely textual and sequential dialog. It is
intended to be used if the colored mode does not work, maybe because of
console issues.
