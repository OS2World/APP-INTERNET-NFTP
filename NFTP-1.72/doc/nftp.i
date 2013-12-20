;
; Do not change nftp.i file; customize nftp.ini!
; Uncomment (remove ";") and change value to customize.
; Note: logical values may be specified as 1/0 or yes/no.

[network]

; your name for anonymous transfers
;
;anonymous-name=anonymous


; your password for anonymous logins. Insert your e-mail address here
; and don't forget to remove leading ";"
;
;anonymous-password="nobody@nowhere.net"


; default ftp port. do not change unless you know what you are doing
;
;default-port=21


; data network timeout value. when no data is received during that period
; while transferring, NFTP considers connection broken. default is
; three minutes (180 sec)
;
;network-timeout=180


; control connection network timeout value. when nothing is received during that period
; while reading server response, NFTP considers connection broken. Default is
; three minutes (180 sec)
;
;cc-timeout=180


; User-Agent header field for HTTP proxies. default: NFTP-1.61
;
;user-agent=

;-----------------------------------------------------------------------
;   Firewalling

[firewalling]

; The firewall descriptions below were taken from WS_FTP docs.
;
; your firewall type:
; 0 - no firewalling (default)
; 1 - SITE hostname
;        Firewall host, userid and password are required.
;        User is logged on the firewall and the remote connection is
;        established using SITE remote_host.
; 2 - USER after logon
;        Firewall host, userid and password are required.
;        User is logged on the firewall and the remote connection is
;        established using USER remote_userid@remote_host
; 3 - USER with no logon
;        Firewall host required, userid and password are ignored.
;        USER remote_userid@remote_host is sent to firewall upon initial 
;        connection.
; 4 - Proxy OPEN
;        Firewall host required, userid and password are ignored.  
;        OPEN remote_host is sent to firewall upon initial connection.
;
; 5 - HTTP proxy
;        Squid and Netscape Proxy fall into this category. You have
;        to specify `firewall-host' and port (typically 3128 for Squid,
;        8080 for Netscape Suitespot server)
;
; 6 - Check Point FireWall-1 Secure FTP server
;        The connection is made by sending
;        USER remote_userid@firewall_userid@remote_host
;        PASS remote_password@firewall_password
;
;firewall-type=0

; name or IP address of your firewall machine
;
;firewall-host=

; your login on the firewall (not applicable to all firewall types)
;
;firewall-login=

; your password on the firewall (not applicable to all firewall types)
;
;firewall-password=

; port number on the firewall
;
;firewall-port=

;  There exist a firewall which is basically of type 3 but does not
;  want USER keyword on login. Set fwbug1 to "yes" for it.
;
;fwbug1=no

;  Whether to delay entering password until actual connection to
;  server is made. Useful for S/Key authorization schemes when
;  server will present a 'challenge' before entering the password.
;  Default: off
;
;delayed-password-entering=no


;-----------------------------------------------------------------------
;
[options]

; NFTP version. Do not change; it is used by mechanism of automatic
; nftp.ini updates when upgrading to next version
;
version="1.72"


; dereference symlinks on server. Setting to "no" is not recommended
; because NFTP does not yet handle symlinks in reasonable way
;
;dereference-links=yes


; Include files starting with "." into list?
;
;show-dotted=yes


; Debugging. Produces large logs (the worst case was 1GB log which has
; filled the rest of disk space :-) and is disabled by default
;
;debug=no


; action to run at the end of transfer. "bell" means a simple beep
; through operating system, other strings will be executed as if
; they were external commands. this parameter replaces previous
; "transfer-bell" setting. use empty value to disable bell. examples:
; OS/2:
;;transfer-complete-action="play file=d:\mmos2\sounds\applaude.wav"
; Unix:
;;transfer-complete-action="echo dowload complete | /bin/mail `whoami`"
; default is to issue a simple beep
;transfer-complete-action=bell


; whether to beep when logged in
;
;login-bell=yes


; whether to pause at transfer stats screen at the end of transfer
;
;transfer-pause=yes


; whether to log transfers. Pretty useful option to keep history and
; statistics
;
;log-transfers=yes


; name of transfer log. This may be global file (one for several instances
; of nftp running simultaneously). default is nftp.fls in nftp directory
;
;log-transfers-name=


; whether to ask to save current host/dir as bookmark at logoff.
; this option is better to be left alone; history subsystem does
; the job better
;
;save-bookmark-at-logoff=no


; where to save bookmark information. This may be global file (one for
; several instances of nftp). default is nftp.bmk in nftp directory
;
;bookmarks-file=


; external program used for viewing/editing files
; Substitute your favourite editor
; default is TEDIT on OS/2 textmode, E on OS/2 TGUI, xedit on OS/2 XFree86,
; Notepad on Windows, vi on Unix, StyledEdit on BeOS
;
;text-editor-os2vio="tedit"
;text-editor-os2pm="e"
;text-editor-os2x="xedit"
;text-editor-unixterm="vi"
;text-editor-unix_x11="xedit"
;text-editor-beosterm="StyledEdit"
;text-editor-win32cons="notepad"
;text-editor-win32gui="notepad"


; whether to switch to Control Connection Window automatically when
; sending commands to server
;
;auto-switch-to-control=no


; directory sort order for remote files
; 0-unsorted, 1-by name, 2-by ext, 3-by size, 4-by time
; use negative numbers to reverse, i.e. "-3" gives small files first 
;
;default-sort=0


; local directory where downloads go. if this parameter is set,
; nftp will change into this directory after start
;
;default-download-path=


; default view of remote directory. 1 - raw (as supplied by server),
; 2 - parsed (owner/group removed). switch between views with Ctrl-P.
; default is parsed
;
;default-dir-mode=2


; whether to display server info in bookmarks when description is present
;
;bmk-show-server-info=yes


; directory sort order for local files
; 0-unsorted, 1-by name, 2-by ext, 3-by size, 4-by time
; use negative numbers to reverse, i.e. "-3" gives small files first 
;
;default-local-sort=0


; interval (in seconds) between retries when anonymous login fails. 
; set to 0 to disable retries. default is 5 minutes
;
;login-retry=300


; 00index.txt size at which NFTP starts to warn before automatic
; downloading, in KBytes. default is 50KB
;
;desc-size-warn=50


; 00index.txt estimated transfer time at which NFTP starts to warn before
; automatic, in seconds. Default is 25 sec (50KB at 2KB/s)
;
;desc-time-warn=25


; number of spaces to show in front of description line. default is 4;
; if you don't like to waste screen space for readability, set to 0.
; this option is obsolete since 1.21 because descriptions are no longer
; shown in "raw" mode, only in the "parsed" mode
;
;description-indentation=4


; whether to look up host names when numeric address is given. Set to
; yes if your DNS is fast enough and you want to try to find site name
;
;dns-lookups=no


; what prompt to show upon startup. 0 -- no prompt,
; 1 - anonymous login, 2 - bookmarks, 3 - history,
; 5 - menu. default is no prompt
;
;start-prompt=0


; pseudographics style.
; 0 - as specified in NLS file (default for OS/2, Win32)
; 1 - funny chars from ASCII set: /\|-+<> (was previously default for Unix)
; 2 - plain but elegant style (default for Unix)
;
;pseudographics=0


; use english menu regardless of language selected? (default: no, but
; NFTP will fall back to English menu when translation not found)
;
;english-menu=no


; what submenu to open when you press F9 or Ctrl-F. when set
; to 0 (default), submenu "Sites" will be opened; with 1, "Directory"
; and so on. set to -1 to stay in upper line menu (and use hotkeys to
; select entries in it)
;
;menu-open-submenu=0


; whether to convert file name to lowercase when it is all-uppercase.
; mixed case filenames will always be kept intact (default: no under Unix,
; yes under OS/2 and Windows). affects downloading only
;
;lowercase-names=no


; whether to set colours for monochrome displays (esp. important for some
; terminal emulators displaying Unix version of NFTP). this option also
; disables colours explicitly specified in nftp.ini. default: no
;
;monochrome=no


; whether to use termcap entries for key definitions (Unix, BeOS).
; NFTP falls back to its defaults when termcap entry is missing.
; sometimes it helps to process keys (F1-F10, arrow keys etc.) correctly,
; sometimes it doesn't and built-in ANSI defaults work better.
; you only have to disable termcap if you are running NFTP on a machine
; with poor termcap with wrong entry for your terminal emulator.
; by default, this feature is on
;
;use_termcap1=yes


; file name to write descriptions when downloading. when this option
; is specified, NFTP will write descriptions (when available) into
; this file. by default, NFTP will not write descriptions
;
;description-file=


; command to start external download program. %s will be substituted with
; name of the file, containing URLs. by default, wget is used (not supplied
; with NFTP); it must be in the PATH
;
;launch-wget="wget -a wget.log -i %s"


; whether to allow non-anonymous downloads via wget (by default, they
; are disabled for security reasons). If you enable it, be aware that
; NFTP will write files for WGET with your password in them!
;
;enable-passworded-wget=no


; whether to use right/left arrows to enter/leave directory, as in Lynx.
; you can use Shift->, Shift-< to scroll filenames and descriptions if you
; enable this option. by default, this option is off (arrow keys are used
; to scroll filenames and descriptions)
;
;lynx-navigation-keys=no


; date format for AS/400 (NFTP will switch between MDY and DMY when
; wrong format is detected, but in some cases autodetect will not
; work, and one needs some default).
; Day/month/year - 2, month/day/year - 3, year/day/month - 5.
; Default is 2 (DMY).
;
;as400-date-format=3


; whether to paste clipboard contents into entry field when "Open URL..."
; is invoked. NFTP tries to be smart and will not paste strings which
; contain spaces inside. by default, this feature is enabled.
;
;autopaste-url=yes


; whether to initialize and use mouse. by default, mouse is enabled.
; you can disable mouse if you prefer keyboard; all NFTP functions can
; be accessed using keyboard only (but not using mouse only -- yet!)
;
;mouse-support=yes


; whether to assume that NFTP is run via slow link, i.e. you are
; telnetting to distant machine and running NFTP there. applies to
; all versions, but makes sense only for Unix/BeOS.  you can also use
; '-s' command-line option. this will cause NFTP to skip some lengthy
; and not very informative screen output and reduce amount of data
; output to terminal. also additional optimizations will be turned on
; in Unix terminal driver (may cause harmless but annoying screen
; corruption on some terminals)
;
;assume-slow-link=no


; whether to save/restore window position upon startup. this applies only to
; some platforms (under version 1.50, OS/2 VIO and PM). default is yes
;
;save-window-position=yes


; whether to save/restore window size upon startup. this applies only to
; some platforms (under version 1.51, OS/2 VIO and PM, X11). default is yes
;
;save-window-size=yes


; interval (in seconds) after which data connection is considered 
; stalled and window title changes to reflect it. default is 30 sec
;
;stalled-interval=30


; whether to activate menu when mouse pointer is over menu bar
; (default is yes)
;
;hotmouse-in-menu=yes


; whether to show timestamp near every event in the control connection
; window (default: yes)
;
;show-timestamp-in-cc=yes


; whether to use PUT command instead of STOR on AS/400 servers.
; default: no
;
;as400-server-uses-put=no


; whether to confirm traversing directory tree when marking directory
;
;ask-before-walking-dir-tree=yes


; whether to preserve Unix access permissions when downloading. Mostly
; useful in Unix versions of NFTP. Default: yes
;
;preserve_permissions_dowload=yes


; whether to preserve Unix access permissions when uploading (not all
; FTP servers support that feature). Mostly useful in Unix versions
; of NFTP. Default: yes
;
;preserve_permissions_upload=yes


; whether to set time/date of retrieved file to original values
; (as stored on server). default: yes
;
;preserve-timestamp-on-download=yes


; whether to set time/date of uploaded file to original values
; (as stored locally). default: yes
;
;preserve-timestamp-on-upload=yes


; whether to strip version numbers from filenames on VMS systems.
; default: yes
;
;VMS-strip-version-number=yes


; the number of files in the transfer batch after which NFTP stops
; asking about overwriting/continuing/skipping and does SmartAppend.
; default: 1 (when more than one file is marked, no questions are asked)
;
;batch-transfer-limit=1


; whether to send No-Cache directive to HTTP proxies. set this parameter
; if you are dealing with content which changes frequently and your
; HTTP proxy has problems refreshing directory contents. default: 
; enable caching
;
;flush-HTTP-cache=no


; whether to try new FTP protocol extensions as specified in
; IETF draft which might soon become an RFC. these extensions
; speed up directory listing retrieving and allow for more accurate
; data about remote files to be retrieved. default: don't try
;
;try-FTP-extensions=no


; whether to ask user to confirm quitting when F10 is hit. default: yes
;
;confirm-quit=yes


; whether to perform case-INsensitive comparisons when mirroring.
; default: no. it is not recommended to do the mirroring to
; braindead filesystems
;
;relaxed-mirroring=no


; whether to query server about BeOS filesystem attribute transfer
; support. by default, this option is "yes" in BeOS version and "no"
; in other NFTP versions
;
;query-bfs-attributes-support=


; whether to use files with ".-BFS-attributes-" extensions to store
; BeOS attributes when local filesystem or remote server do not support
; attributes natively. default: no
;
;use-bfs-files=no


; whether to confirm remote directory re-read. default: no when sites
; are on the same subnet, otherwise yes
;
;confirm-reread=yes


; whether to confirm logoff. default: yes
;
;confirm-logoff=yes


; whether to display only PS and PO entries on OS/390 servers. default: no
;
;os390-hide-non-datasets=no


; whether to display hardware cursor. useful for people with weak eyes.
; default: no
;
;show-hw-cursor=no


; threshold for backing up files: when overwriting files which are larger
; than this value will be renamed, not overwritten. this is to protect
; big files from accidental overwrite. default: 100 megs
;
;backup-threshold=100000000


; whether to backup files larger than 'backup-threshold' when overwriting.
; default: yes
;
;backups=yes


; whether to try to shorten filenames on failure to open files. this
; enables to download files with long names to braindead filesystems
; such as FAT; applicable to OS/2 running on FAT. this feature is not 
; recommended to use with mirroring and is not enabled by default. most
; modern operating systems has filesystems which can store long filenames
;
;try-to-shorten-filenames=no


; when doing mirroring delete will not be performed if percentage of the
; the files that would be deleted is greater than this number. default: 10
; (this number is percentage). set to 101 to force deletion
;
;file-delete-percentage-limit=10


; when doing mirroring delete will not be performed if percentage of the
; the directories that would be deleted is greater than this number. 
; default: 10 (this number is percentage). set to 101 to force deletion
;
;directory-delete-percentage-limit=10


; whether to use reworked transfer algorithms (BETA quality!)
;
;new-transfer-algorithms = yes


; whether to issue MDTM command to obtain exact timestamps when this
; command is available on server. default: yes
;
;use-MDTM = yes


[history]

; whether to store information about sites last visited.
; if set to "no", new sites will not be added
;
;active=yes


; name of the file where history is kept.
; default is nftp.hst in nftp directory
;
;history-file=


; whether to record site/path upon entering every directory. with this
; option disabled, recorded are only start of login procedure, logoff
; and downloads. enabled by default
;
;record-everything=yes


; the max. allowed number of entries in the history file.
; after reaching that number, old entries will be discarded
; one-by-one to give space for new ones. default is to stop
; at 5000 site/path combinations. Lower this figure to say 1000
; you have large history on 486 machine. NOTE: this setting is
; ignored for now (don't know yet how to make it right)
;
;entries-watermark=5000


; the max. allowed number of sites in the history file.
; after reaching that number, old sites will be discarded
; one-by-one to give space for new ones. default is to stop
; at 1000 sites. Lower this figure to say 100 you have large
; history on 486 machine.
;
;sites-watermark=1000


; the max. allowed number of directories per site in the history file.
; after reaching that number, old directories will be discarded.
; default is to keep 50 directories per site when "record-everything"
; is set to "no", and 500 otherwise.
;
;persite-watermark=50



[passwords]

; whether to use password caching feature of NFTP, and what encryption
; to apply to passwords in the file:
; 0 - disable password caching (NFTP won't write nftp.psw file);
; 1 - use plain, unencrypted passwords (dangerous!);
; 2 - use scrambled passwords (insecure);
; 3 - use encrypted passwords; you'll have to supply keyphrase
;     to decode the file when NFTP starts (secure).
; By default, this feature is disabled on Unix for security reasons
; and enabled (with method 2) on OS/2, Windows, BeOS. Note: writing
; passwords into history and bookmark files no longer works since 1.60!
;
;encryption-type=0


; name of the file where passwords are kept. default is nftp.psw in
; nftp directory
;
;passwords-file=



[layout]

; upper status line style. choices: 0, 1, 2 (default: 0)
; 0 - no status line
; 1 - single line
; 2 - two lines as in NFTP prior to 1.50
;
;top-status-line=1


; lower status line style. choices: 0, 1, 2 (default: 1)
; 0 - no status line
; 1 - single line
; 2 - two lines as in NFTP prior to 1.50
;
;bottom-status-line=1


; whether to keep menu always on screen (not applicable for GUI versions)
; (default: yes)
;
;menu-onscreen=yes


; whether to hide dates when file times are hidden in two-panel view
; (default: no)
;
;hide-dates-too=no


; whether to draw full frames in two-panel mode
; (default: no)
;
;full-frames=no


; should dialog boxes cast shadows (default: yes)
;
;shadows=yes


; should we use GUI dialog boxes (when available). set to "no"
; to always show plain character-mode representations. applicable
; to OS/2 PM and Windows GUI versions only. default is to use GUI
; controls
;
;GUI-controls=yes



[ftpsearch]

; when doing find, open servers window to confirm selection
; (default: yes)
;
;show-servers=yes



;-----------------------------------------------------------------------
;
; Colours are hexadecimal. First digit - foreground, second - background
; Add 80 to blink
;
; Fore             Fore                Back
; 00 black         08 darkgrey         00 black
; 01 blue          09 light blue       10 blue
; 02 green         0a light green      20 green
; 03 cyan          0b light cyan       30 cyan
; 04 red           0c light red        40 red
; 05 magenta       0d light magenta    50 magenta
; 06 brown         0e yellow           60 brown
; 07 grey          0f white            70 grey

[colours]

; ----- File Listing -----------------------------------------------

; cursor pointing to marked directory (default: bright-green on cyan)
;
;pointer-marked-dir=3a


; cursor pointing to marked file (default: yellow on cyan)
;
;pointer-marked=3e


; cursor pointing to directory (default: blue on cyan)
;
;pointer-dir=31


; cursor pointing to file (default: black on cyan)
;
;pointer=30


; cursor over description in parsed mode (default: black on cyan)
;
;pointer-desc=30


; marked directory (default: bright-green on black)
;
;marked-dir=0a


; marked file (default: yellow on black)
;
;marked=0e


; directory (default: green on black)
;
;dir=02


; file (default: grey on black)
;
;regular-file=07


; description (default: brown on black)
;
;file-description=06


; ----- File Panels (two-panel mode)

;TP-file=17
;TP-directory=17
;TP-file-marked=1c
;TP-directory-marked=1c
;TP-file-pointer=30
;TP-directory-pointer=30
;TP-file-marked-pointer=34
;TP-directory-marked-pointer=34

; ----- Dialog windows -----------------------------------------------

; dialog window (default: black on white)
;
;dialog-box-text=70


; selected element in a dialog window (default: bright white on black)
;
;dialog-box-selected=0f


; entry field (default: black on cyan)
;
;dialog-box-entryfield=30


; dialog window containing important information (default: grey on red)
;
;dialog-box-text-warn=47


; ----- Control Connection Window -------------------------------------

; window header (default: grey on blue)
;
;controlconn-header=17


; server messages and responces (default: grey on black)
;
;controlconn-respline=07


; command which was sent to server (default: bright-green on black)
;
;controlconn-command=0a


; comment (actually not a part of control connection)
; (default: bright-red on black)
;
;controlconn-comment=0c


; ------ Bookmarks -----------------------------------------------

; site description (default: black on grey)
;
;bookmark-background=70


; cursor over site description (default: black on cyan)
;
;bookmark-pointer=30


; host/path (default: red on grey)
;
;bookmark-hostpath=74


; cursor over host/path (default: red on cyan)
;
;bookmark-hostpath-pointer=34


; ------ Built-in File Viewer -----------------------------------

; file contents (default: grey on black)
;
;viewer-text=07


; status line (default: black on cyan)
;
;viewer-header=30


; string found in the text (default: bright-red on black)
;
;viewer-foundtext=0c


; -------- Menu -------------------------------------------------

; menu text (default: black on grey)
;
;menu-main=70


; disabled item (default: OS/2, Windows: pale grey on grey; Unixes, BeOS:
; dark blue on grey)
;
;menu-main-disabled=78


; hotkey in the item (default: bright-red on grey)
;
;menu-main-hotkey=7c


; highlight over the menu item (default: grey on black)
;
;menu-cursor=07


; hotkey in the item under highlight (default: bright-green on black)
;
;menu-cursor-hotkey=0a


; highlight over disabled item (default: OS/2, Windows: pale grey
; on black, Unixes, BeOS: bright blue on black)
;
;menu-cursor-disabled=08


; -------- Miscellaneous ---------------------------------------------

; status lines on remote directory (no. of files, no. of bytes, 
; current host/directory etc.) (default: grey on blue)
;
;status=17


; status lines on local directory (no. of files, no. of bytes, 
; current host/directory etc.) (default: white on blue)
;
;status-local=1f


; status lines when with style=1 (default: black on white)
;
;status2=70


; file statistics on bottom status line, when upper status line
; is disabled, and there are marked files (defaut: red on white)
;
;marked-stats=74


; file transfer panel (default: grey on blue)
;
;transfer-status=17


; help viewer (default: black on white)
;
;help=70


; general background colour (default: grey on black)
;
;background=07


;-----------------------------------------------------------------------

; Key definitions. Please note that keys should NEVER overlap, i.e. there are
; no keystrokes which act differently in different modes. You can assign
; several keystrokes to the same action; separate them by commas, e.g.:
;    enter-directory=enter,ctrl-pgdn
;
; Some keys (e.g., ctrl-ampersand) do not work at all; don't be surprised.
; Many keys do not work under Unix. Use supplied `keynames' program to
; find out what your system is capable of.
;
; Three-key combinations (like Ctrl-Shift-Left) cannot be used.
;
; Default keystrokes still perform the same functions if not assigned.
; To bind a key to nothing, list it in "nothing=" line.
;
; Available key names are (they are not case-sensitive, except standalone
; character keys like 'a' and 'A'):
;
; backspace tab enter esc lrbracket([) rrbracket(]) minus plus space
; semicolon apostrophe backapostrophe backslash comma period slash
; colon doublequote lcbracket({) rcbracket(}) questionmark lessthan
; greaterthan underline equalsign lparenth"(" rparenth")" ampersand vertline(|)
; percentsign caret(^) dollar grate(#) at(@) exclamation approx(~)
; asterisk f1..f12 insert delete home end pgup pgdn numslash numasterisk
; numminus numplus numenter left right up down gold('5' on numeric keypad)
; a..z A..Z 0..9
;
; All these parameters are of type 'string' and can take more than
; one value. Separate values by comma. Quotes aren't needed because
; key names do not contain special characters.

[keys]

;  binds key to "no action". used to disable unwanted defaults

;nothing=

;enter-menu=f9,ctrl-f

;  help system

;concise-help=K
;main-help=H
;context-sensitive-help=f1,questionmark
;show-version=
;screen-redraw=
;edit-inifile=
;update-nftp=

;  login/logout/exit

;login=ctrl-l
;open-url=L
;bookmarks=ctrl-b
;history=f2,ctrl-e,alt-f2
;logoff=ctrl-k
;passwords=
;exit=f10
;no-operation=A

;  navigating file listing

;switch-to-cc=space
;switch-local-remote=tab
;scroll-10-right=greaterthan
;scroll-10-left=lessthan
;directory-mode-switch=ctrl-p

;enter-directory=enter,rrbracket,ctrl-pgdn
;go-root=backslash
;go-up=lrbracket,ctrl-pgup
;change-local-drive=Z,alt-f1
;change-directory=C
;reread=ctrl-r
;load-descriptions=O
;hide-descriptions=ctrl-o

;  sorting file list

;sort-name=N
;sort-ext=X
;sort-time=T
;sort-size=S
;sort-unsort=Y
;sort-reverse=E

;  marking files

;select=insert,M
;select-all=ctrl-a
;deselect-all=ctrl-x
;select-by-filter=plus
;deselect-by-filter=minus
;invert-selection=asterisk
;select-alldirs=alt-numplus
;deselect-alldirs=
;information=I
;compute-dirsize=ctrl-q

;  download/upload. if 'new-transfer' is in effect copy=f5 and move=f6
;  otherwise download=f5 and upload=f6

;change-transfer-mode=
;download=f5,D
;upload=f6,U
;download-all-dirs=ctrl-d
;upload-all-dirs=ctrl-u
;download-byname=G
;download-wget=W
;export-marked=
;copy=f5,D
;move=f6

;  viewing files

;view-int=f3,ctrl-v
;view-ext=V

;  creating/deleting/renaming

;rename=R
;mkdir=f7,ctrl-g
;delete=f8,delete
;change-permissions=

; FTP Search interface

;ftpsearch-find=F
;ftpsearch-servers=
;ftpsearch-recall=P

;  screen layout 

;split-screen=ctrl-w
;show-timestamps=ctrl-t
;show-ministatus=

;  misc keys

;save-bookmark=f4,B
;use-flags=
;use-proxy=
;save-listing=
;quote=Q
;passive-mode=

; cursor movements (do not work yet)
;;up=
;;down=
;;right=
;;left=
;;pageup=
;;pagedown=
;;home=
;;end=

; -------------- end of nftp.ini -------------------------------

