#!/bin/sh

# This shell script installs NFTP binaries and support files

# Issue a message for user

echo
echo "NFTP installation"

TARGETBIN="/boot/beos/bin"
TARGETLIB="/boot/beos/etc/nftp"
TARGETDOC="/boot/beos/documentation/nftp"

# Issue a message for user and wait for decision

echo "The following files are to be installed:"
echo
echo "NFTP executable:                  " $TARGETBIN/nftp
echo "National language support files:  " $TARGETLIB/nls/"*"
echo "Template for initialization file: " $TARGETLIB/nftp.i
echo "script for uninstalling NFTP:     " $TARGETLIB/uninstall.sh
echo "Documentation:                    " $TARGETDOC/"*"
echo

echo "Press ENTER to continue, or Ctrl-C to cancel"
read x

# making directories

echo -n "Making directories: "

echo -n $TARGETBIN " "
mkdir -m 755 -p $TARGETBIN

echo -n $TARGETLIB " "
mkdir -m 755 -p $TARGETLIB

echo -n $TARGETLIB/nls " "
mkdir -m 755 -p $TARGETLIB/nls

echo -n $TARGETDOC " "
mkdir -m 755 -p $TARGETDOC

echo

# Install everything

BINFILES=nftp
LIBFILES="uninstall.sh nftp.i nftp.bm"
NLSFILES=`echo *.nls *.mnu`
DOCFILES=`echo *.html *.txt LICENSE readme.1st`

echo "Copying base files ("$BINFILES $LIBFILES")..."
cp $BINFILES  $TARGETBIN
cp $LIBFILES  $TARGETLIB

echo "Copying NLS files ("$NLSFILES")"
cp $NLSFILES $TARGETLIB/nls

echo "Copying documentation ("$DOCFILES")..."
cp $DOCFILES  $TARGETDOC

echo "Setting menu entries..."
cp NFTP /boot/beos/apps
ln -sf /boot/beos/apps/NFTP /boot/apps/NFTP

echo
echo "Installation complete."
