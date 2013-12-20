/* ADDICON.CMD Version 1.1 */
/* by Timothy F. Sipples */
/* July 11, 1992 */

/* Modified by Michael J. Strasser to allow multiple filespecs, all */
/* of which can have wildcards. */
/* July 27, 1992 */

/* Copyright 1992 by Timothy F. Sipples. */
/* This utility may be freely distributed with all included files. */
/* For-profit (commercial) users: please see the accompanying */
/* documentation for license terms. */

/* ADDICON is a REXX utility to attach an icon file to any file */
/* on the system.  It uses the SysSetIcon function in REXX, which */
/* places the icon file in the extended attributes of any given */
/* file. */

/* Retrieve command line arguments passed to program. */
/* Also converts to uppercase. */
ARG Arg1 Arg2

/* Print title banner. */
/*SAY ''
SAY 'AddIcon Version 1.1 for OS/2 Version 2.0 and Above'
SAY 'by Timothy F. Sipples (Internet: sip1@ellis.uchicago.edu)'
SAY 'and Michael J. Strasser (Internet: M.Strasser@edn.gu.edu.au)'
SAY ''*/

/* Check for null arguments. */
if Arg1 \= '' & Arg2 \= '' then SIGNAL attach

/* If one or both arguments not specified, print usage information. */
SAY 'USAGE:    ADDICON Iconfile Filespec ...'
SAY ''
SAY "Iconfile, an icon file created by OS/2's Icon Editor,"
SAY "will be added to the extended attributes of the specified files."
SAY ''
SAY 'Example:  ADDICON \ICONS\DOC.ICO C:\DATA\TEST.TXT'
SAY ''
SAY 'Wildcards are permitted, as well as multiple filespecs.'
SAY ''
SAY 'Example: ADDICON \ICONS\DOC.ICO C:\DATA\*.TXT C:\WORK\*.TXT'
SAY ''
SAY 'to attach the same icon to all those files.'
SIGNAL end

attach:

/* Load SysSetIcon function from standard OS/2 REXX library. */
call RxFuncAdd 'SysSetIcon','RexxUtil','SysSetIcon'
/* Ditto for SysFileTree */
CALL RxFuncAdd 'SysFileTree', 'RexxUtil', 'SysFileTree'

/* Set the error flag */
GotError = 0

/* Arg2 is one or more filespecs, which may include wildcard expressions */
DO UNTIL Arg2 = ''
     /* Get the first filespec into Filespec; remainder back into Arg2 */
     PARSE VAR Arg2 Filespec Arg2
     /* Clear the stem variable */
     Files. = ''
     /* Get files & directories, names only */
     call SysFileTree Filespec, 'Files', 'O'
     /* For each filespec, try to attach the icon */
     DO i = 1 TO Files.0
          IF SysSetIcon(Files.i, Arg1) THEN
               NOP/*SAY 'Icon' Arg1 'successfully attached to file' Files.i'.'*/
          ELSE DO
               SAY 'ERROR: unable to attach icon' Arg1 'to file' Files.i'.'
               GotError = 1
          END
     END
END

IF GotError THEN
     SIGNAL failed

end:
EXIT

failed:
SAY ''
SAY 'Some errors occurred in attaching icons.'
SAY 'Verify that all files exist, that the icon file is in the'
SAY 'proper format, and that files are not in use by another process.'
SIGNAL end
