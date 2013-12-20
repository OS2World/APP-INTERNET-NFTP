/* REXX */

/* Load RexxUtil extensions */
if RxFuncQuery("SysLoadFuncs") then do
    say "loading RexxUtil extensions..."
    call RxFuncAdd "SysLoadFuncs","RexxUtil","SysLoadFuncs"
    if result \= "0" then do
        say "error loading RexxUtil.dll"
        exit
    end
    call SysLoadFuncs
end

/* ask user about directories etc. */
Call SysCls
say
say "NFTP Version 1.72 installation"
say
say "This script will do the following:"
say "1) copy required files into the directory you specified;"
say "2) create objects on your desktop"
say
say "Enter destination directory (where NFTP files will be installed)."
say "WARNING: this must be HPFS drive (to support long filenames)."
say "If you wish to install NFTP into the subtree (eg, ""d:\apps\tcpip\nftp"")"
say "you have to create upper directory first (""d:\apps\tcpip"" in the"
say "above example). Enter ""."" to install into the current directory."
say "Empty line entered will abort installation."
say
parse pull destpath .
if destpath = "" then exit

destpath = strip(destpath, "T", "\")
dlpath = strip(dlpath, "T", "\")

/* verify choices */
call SysCls
say "Installing NFTP into the directory :" destpath
say
say "Press Ctrl-C to abort or any other key to continue"
say
'@pause >nul'
Call SysCls

/* creating target directories if necessary */
if destpath <> "." then
do
    call SysFileTree destpath, "srch", "D"
    if srch.0 = 0 then do
        rc = SysMkDir(destpath)
        if rc <> 0 then do
            say "Fatal error: cannot create directory" destpath
            exit
        end
    end
    call SysFileTree destpath"\nls", "srch", "D"
    if srch.0 = 0 then do
        rc = SysMkDir(destpath"\nls")
        if rc <> 0 then do
            say "Fatal error: cannot create directory" destpath"\nls"
            exit
        end
    end
end
    
/* copying NFTP files */
if destpath <> "." then
do
    '@copy * 'destpath' >nul'
    '@copy 'destpath'\*.nls 'destpath'\nls\ >nul'
    '@copy 'destpath'\*.mnu 'destpath'\nls\ >nul'
    '@del 'destpath'\*.nls' destpath'\*.mnu'
    call SysFileTree destpath"\nftpx.exe", "srch", "F"
    if srch.0 <> 0 then '@del 'destpath'\nftpx.exe'
    call SysFileTree destpath"\xnftp.exe", "srch", "F"
    if srch.0 <> 0 then '@del 'destpath'\xnftp.exe'
    say "Files were copied to" destpath
end

if destpath = "." then destpath = directory()
if dlpath = "." then dlpath = directory()

'@pause'
Call SysCls

call makeobjs destpath

/* looks like we're done... */
say
say "Installation complete."
say
'@pause'
Exit
