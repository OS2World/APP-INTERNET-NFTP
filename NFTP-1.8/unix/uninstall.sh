#!/bin/sh

# This shell script removes NFTP binaries and support files

# look up installation

IS_ROOT=`id | grep "uid=0(root)" | wc -l`

if [ $IS_ROOT -eq 1 ]
then
   if [ -d /usr/lib/nftp ]
   then
      TARGETBIN="/usr/bin"
      TARGETLIB="/usr/lib/nftp"
   else
      TARGETBIN="/usr/local/bin"
      TARGETLIB="/usr/local/lib/nftp"
   fi
   TARGETDOC="/usr/doc/nftp"
   TARGETMAN="/usr/man/man1"
else
   TARGETBIN=$HOME/bin
   TARGETLIB=$HOME/.nftp
   TARGETDOC=
   TARGETMAN=
fi

# Issue a message for user and wait for decision

clear
echo
echo "NFTP uninstallation."
echo
echo "This script will REMOVE the following files:"
echo
echo "NFTP executable:                  " $TARGETBIN/nftp
if  [ $IS_ROOT -eq 1 ]
then
   echo "Directory                         " $TARGETLIB
   echo "Man page for NFTP:                " $TARGETMAN/nftp.1
   echo "Documentation:                    " $TARGETDOC"/*"
else
   echo "National language support files:  " $TARGETLIB"/nls/*"
   echo "Template for initialization file: " $TARGETLIB/nftp.i
   echo "Base bookmark list:               " $TARGETLIB/nftp.bm
   echo "Keynames executable:              " $TARGETLIB/keynames
   echo "Script for uninstalling NFTP:     " $TARGETLIB/uninstall.sh
fi
echo
if [ $IS_ROOT -eq 1 ]
then
   echo "Entire subdirectory " $TARGETLIB " will be removed." 
else
   echo "Subdirectory " $TARGETLIB " will not be deleted."
fi
echo "Your personal files in ~/.nftp will be kept intact."

echo "Press ENTER to continue, or Ctrl-C to cancel"
read x

# Remove everything

echo -n "Deleting executables: "
echo $TARGETBIN/nftp
rm -f $TARGETBIN/nftp

if [ $IS_ROOT -eq 1 ]
then
   echo "Removing " $TARGETLIB " directory"
   rm -rf $TARGETLIB
else
   echo -n "Deleting support files: "
   echo $TARGETLIB/keynames $TARGETLIB/nftp.i $TARGETLIB/nftp.bm $TARGETLIB/uninstall.sh
   rm -f $TARGETLIB/keynames $TARGETLIB/nftp.i $TARGETLIB/nftp.bm $TARGETLIB/uninstall.sh

   echo -n "Deleting NLS files: "
   echo $TARGETLIB/nls/*.nls $TARGETLIB/nls/*.dos
   rm -f $TARGETLIB/nls/*.nls $TARGETLIB/nls/*.dos
fi

if  [ $IS_ROOT -eq 1 ]
then
   echo -n "Deleting manual page and HTML documentation: "
   echo $TARGETMAN/nftp.1 $TARGETDOC/*.html $TARGETDOC/readme.*
   rm -f $TARGETMAN/nftp.1 $TARGETDOC/*.html $TARGETDOC/readme.*
fi

echo "Deinstallation is complete."
