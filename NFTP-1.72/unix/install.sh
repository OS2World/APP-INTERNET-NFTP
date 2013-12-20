#!/bin/sh

# This shell script installs NFTP binaries and support files

# Issue a message for user

clear
echo
echo -n "NFTP installation ("

# FreeBSD's sh does not export UID. Hmm...

IS_ROOT=`id | grep "uid=0(root)" | wc -l`

if [ $IS_ROOT -eq 1 ]
then
   echo "system-wide)"
else
   echo "private)"
fi

# determine where to install

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
   TARGETMAN="/usr/local/man/man1"
   TARGETDOC="/usr/local/doc/nftp"
#  silently try to kill old manpage   
   rm -f /usr/man/man1/nftp.1
else
   TARGETBIN=$HOME/bin
   TARGETLIB=$HOME/.nftp
   TARGETDOC=
   TARGETMAN=
fi

# Issue a message for user and wait for decision

echo "The following files are to be installed:"
echo
echo "NFTP executable:                  " $TARGETBIN/nftp
echo "X11 NFTP executable:              " $TARGETBIN/xnftp
echo "National language support files:  " $TARGETLIB/nls/"*"
echo "Template for initialization file: " $TARGETLIB/nftp.i
echo "Icon file (64x64):                " $TARGETLIB/nftp.xpm
echo "script for uninstalling NFTP:     " $TARGETLIB/uninstall.sh
if [ $IS_ROOT -eq 1 ]
then
echo "Manual page for NFTP:             " $TARGETMAN/nftp.1
echo "Documentation:                    " $TARGETDOC/"*"
fi
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

if [ $IS_ROOT -eq 1 ]
then
   echo -n $TARGETMAN " "
   mkdir -m 755 -p $TARGETMAN
   echo -n $TARGETDOC " "
   mkdir -m 755 -p $TARGETDOC
fi
echo

# Install everything

echo "Copying files and setting permissions..."

echo "Copying base files..."
rm -f         $TARGETBIN/nftp
cp nftp       $TARGETBIN
cp xnftp      $TARGETBIN
chmod 755     $TARGETBIN/nftp $TARGETBIN/xnftp

cp uninstall.sh $TARGETLIB
chmod 755     $TARGETLIB/uninstall.sh

cp nftp.i     $TARGETLIB
chmod 644     $TARGETLIB/nftp.i

cp nftp.xpm   $TARGETLIB
chmod 644     $TARGETLIB/nftp.xpm

cp nftp.bm    $TARGETLIB
chmod 644     $TARGETLIB/nftp.bm

echo "Copying NLS files..."
cp *.nls* *.mnu* $TARGETLIB/nls
chmod 644     $TARGETLIB/nls/*

if [ $IS_ROOT -eq 1 ]
then
   echo "Copying manual page and HTML documentation..."
   cp nftp.1     $TARGETMAN
   chmod 644     $TARGETMAN/nftp.1
   
   cp *.html     $TARGETDOC
   cp readme.*   $TARGETDOC
   chmod 644     $TARGETDOC/*.html $TARGETDOC/readme.*
else
   echo "NOTE: manual page and HTML documentation are only copied"
   echo "in the system-wide installation"
fi

echo
echo "Installation complete."
