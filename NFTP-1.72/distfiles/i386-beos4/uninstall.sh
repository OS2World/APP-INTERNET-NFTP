#!/bin/sh

# This shell script removes NFTP binaries and support files

TARGETBIN="/boot/beos/bin"
TARGETLIB="/boot/beos/etc/nftp"
TARGETDOC="/boot/beos/documentation/nftp"

# Issue a message for user and wait for decision

echo
echo "NFTP uninstallation."
echo
echo "This script will REMOVE the following files:"
echo
echo "NFTP executable:                  " $TARGETBIN/nftp
echo "Directory                         " $TARGETLIB
echo "Documentation:                    " $TARGETDOC"/*"
echo
echo "Your personal files in ~/.nftp will be kept intact."

echo "Press ENTER to continue, or Ctrl-C to cancel"
read x

# Remove everything

echo "Deleting executable "
rm -f $TARGETBIN/nftp 

echo "Removing " $TARGETLIB " directory"
rm -rf $TARGETLIB

echo "Deleting documentation"
rm -rf $TARGETDOC

echo "Deinstallation is complete."

