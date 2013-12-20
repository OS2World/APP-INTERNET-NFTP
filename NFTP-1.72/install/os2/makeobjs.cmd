/* REXX */

version="1.72"

say
say "Creating/updating WPS objects for NFTP"
say

/* Load RexxUtil extensions */

if RxFuncQuery("SysLoadFuncs") then do
    call RxFuncAdd "SysLoadFuncs","RexxUtil","SysLoadFuncs"
    if result \= "0" then do
        say "error loading RexxUtil.dll"
        exit
    end
    call SysLoadFuncs
end

/* analyze arguments */

parse arg destpath .
if destpath = "" then destpath = directory()
destpath1=translate(destpath,'/','\');

foldername = "<NFTP_FOLDER>"

/* creating folder */

settings = 'OBJECTID='foldername';ICONFILE='destpath'\nftp-fld.ico'
rc = SysCreateObject('WPFolder', 'NFTP', '<WP_DESKTOP>', settings, 'update')
if rc != 1 then say "Cannot create NFTP folder"
else            say "NFTP folder                 has been created/updated"

/* creating WPS objects */

settings = 'EXENAME='destpath'\nftp.exe;PARAMETERS=[FTP server to log in?];PROGTYPE=WINDOWABLEVIO;MINIMIZED=NO;ICONFILE=nftp.ico'
rc = SysCreateObject('WPProgram', 'NFTP', foldername, settings, 'update')
if rc != 1 then say "Cannot create NFTP object"
else            say "NFTP object                 has been created/updated"

settings = 'EXENAME='destpath'\nftppm.exe;MINIMIZED=NO'
rc = SysCreateObject('WPProgram', 'NFTP/PM', foldername, settings, 'update')
if rc != 1 then say "Cannot create NFTP/PM object"
else            say "NFTP/PM object              has been created/updated"

settings = 'EXENAME=E.EXE;PARAMETERS='destpath'\nftp.bmk;'
rc = SysCreateObject('WPProgram', 'Edit bookmarks', foldername, settings, 'update')

if rc != 1 then say "Cannot create 'Edit bookmarks' object"
else            say "'Edit bookmarks' object     has been created/updated"

settings = 'EXENAME=E.EXE;PARAMETERS='destpath'\nftp.ini;'
rc = SysCreateObject('WPProgram', 'Edit NFTP.INI', foldername, settings, 'update')
if rc != 1 then say "Cannot create 'Edit NFTP.INI' object"
else            say "'Edit NFTP.INI' object      has been created/updated"

settings = 'EXENAME=NETSCAPE.EXE;PARAMETERS=file://'destpath1'/nftp-faq.html;STARTUPDIR='destpath';PROGTYPE=PM;MINIMIZED=NO'
rc = SysCreateObject('WPProgram', 'Frequently Asked Questions', foldername, settings, 'update')
if rc != 1 then say "Cannot create FAQ           object"
else            say "FAQ object                  has been created/updated"

settings = 'EXENAME=NETSCAPE.EXE;PARAMETERS=file://'destpath1'/nftp-keys.html;STARTUPDIR='destpath';PROGTYPE=PM;MINIMIZED=NO;ICONFILE=nftp-man.ico'
rc = SysCreateObject('WPProgram', 'Keyboard reference', foldername, settings, 'update')
if rc != 1 then say "Cannot create documentation object"
else            say "Documentation object        has been created/updated"

settings = 'EXENAME=NETSCAPE.EXE;PARAMETERS=file://'destpath1'/history.html;STARTUPDIR='destpath';PROGTYPE=PM;MINIMIZED=NO;ICONFILE=nftp-man.ico'
rc = SysCreateObject('WPProgram', 'History of changes', foldername, settings, 'update')
if rc != 1 then say "Cannot create documentation object"
else            say "Documentation object        has been created/updated"

settings = 'EXENAME=E.EXE;PARAMETERS='destpath'\order.txt;'
rc = SysCreateObject('WPProgram', 'Ordering information', foldername, settings, 'update')
if rc != 1 then say "Cannot create 'Ordering information' object"
else            say "'Ordering information' object  has been created/updated"

settings = 'EXENAME=E.EXE;PARAMETERS='destpath'\regform.txt;'
rc = SysCreateObject('WPProgram', 'Registration form', foldername, settings, 'update')
if rc != 1 then say "Cannot create 'Registration form' object"
else            say "'Registration form' object  has been created/updated"

return
