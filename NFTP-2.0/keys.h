#define KEY_MAX                              130

#define KEY_NOTHING                           -1 // has no effect

#define KEY_HELP                               1 // help
#define KEY_MAINHELP                           2 // general help
#define KEY_SHORTHELP                          3 // short help
#define KEY_PG_RIGHT                           4 // ctrl-right
#define KEY_PG_LEFT                            5 // ctrl-left
#define KEY_INTERNAL_VERSION                   6 // displays time of build
#define KEY_USING_MENUS                        7 // using menus
#define KEY_MENU                               8 // enter menu
#define KEY_CRASH                              9 // crashes NFTP

#define KEY_GEN_BOOKMARKS                     10 // bookmarks
#define KEY_GEN_LOGIN                         11 // login
//#define KEY_GEN_LOGIN_NAMED                   12 // login with userid/passwd
#define KEY_GEN_LOGOFF                        13 // log off
#define KEY_GEN_EXIT                          14 // exit NFTP
#define KEY_GEN_INFORMATION                   15 // get current status
#define KEY_GEN_EDIT_INI                      16 // invoke editor on nftp.ini
#define KEY_GEN_HISTORY                       17 // history
#define KEY_GEN_OPEN_URL                      18 // connect to URL
#define KEY_EXPORT_MARKED                     19 // export list of marked files

#define KEY_SHOW_OWNER                        20 // show/hide owner/group information
#define KEY_BINARY_ASCII                      21 // switch ASCII/BINARY
#define KEY_GEN_NOOP                          22 // send NOOP
#define KEY_GEN_USEFLAGS                      23 // use/disable NLST with flags
#define KEY_GEN_QUOTE                         24 // quote command
#define KEY_GEN_SWITCH_PROXY                  25 // switch proxy on/off
#define KEY_PASSIVE_MODE                      26 // switch passive mode on/off
#define KEY_RESOLVE_SYMLINKS                  27 // switch symlink resolving on/off
#define KEY_EDIT_PASSWORDS                    28 // edit passwords
#define KEY_CHANGE_KEYPHRASE                  29 // change keyphrase for password encryption

#define KEY_GEN_SWITCH_TO_CC                  30 // switch between CC and dir
#define KEY_GEN_SWITCH_LOCREM                 31 // switch between local/remote
#define KEY_GEN_MODE_SWITCH                   32 // switch between raw/parsed modes
#define KEY_GEN_SHOW_DESC                     34 // show/hide descriptions
#define KEY_GEN_SAVEMARK                      35 // save host/dir as bookmark
#define KEY_GEN_SCREENREDRAW                  36 // repaint screen
#define KEY_GEN_SPLITSCREEN                   37 // split screen to remove and local parts
#define KEY_GEN_SHOWTIMESTAMPS                38 // show/hide file timestamps in panels
#define KEY_GEN_SHOWMINISTATUS                39 // show/hide mini-status field

//#define KEY_FM_DOWNLOAD                       40 // download
//#define KEY_FM_UPLOAD                         41 // upload
//#define KEY_FM_DOWNLOAD_EVERYTHING            42 // d/l from all dirs
//#define KEY_FM_UPLOAD_EVERYTHING              43 // u/l from all dirs
#define KEY_FM_DOWNLOAD_BYNAME                44 // d/l by name
#define KEY_FM_SAVE_LISTING                   45 // save directory listing
#define KEY_FM_DOWNLOAD_WGET                  46 // launch wget to retrieve files
#define KEY_SKIP_TRANSFER                     47 // skips file during transfer
#define KEY_STOP_TRANSFER                     48 // cancels transfer
#define KEY_RESTART_TRANSFER                  49 // restart transfer

#define KEY_FM_VIEW_INT                       50 // view w/int viewer
#define KEY_FM_VIEW_EXT                       51 // view w/ext viewer
#define KEY_FM_DELETE                         52 // delete file/dir
#define KEY_FM_MKDIR                          53 // create directory
#define KEY_FM_RENAME                         54 // rename file/directory
#define KEY_FM_SEARCH                         55 // search in filenames @
#define KEY_FM_REPEAT_SEARCH                  56 // repeat search in filenames @
#define KEY_FM_COMPUTE_DIRSIZE                57 // traverse directory and compute its size
#define KEY_PAUSE_TRANSFER                    58 // pause transfer
#define KEY_GEN_GOTO                          59 // select panel contents

#define KEY_FM_SORT_NAME                      60 // sort by name
#define KEY_FM_SORT_EXT                       61 // sort by extension
#define KEY_FM_SORT_TIME                      62 // sort by date
#define KEY_FM_SORT_SIZE                      63 // sort by size
#define KEY_FM_SORT_UNSORT                    64 // make unsorted
#define KEY_FM_SORT_REVERSE                   65 // reverse sort order
#define KEY_SYNC_BY_DOWNLOAD                  66 // synchronize local to remote
#define KEY_SYNC_BY_UPLOAD                    67 // synchronize remote to local
#define KEY_SYNC_DELETE                       68 // switch: delete/keep unmatched files
#define KEY_SYNC_SUBDIRS                      69 // switch: synchronize subdirs?

#define KEY_FM_REREAD                         70 // re-read directory
#define KEY_FM_ENTER_DIR                      71 // change into dir under cursor
#define KEY_FM_GO_ROOT                        72 // change to root dir
#define KEY_FM_GO_UP                          73 // change to upper dir
#define KEY_FM_CHDIR                          74 // input dir and change to it
#define KEY_FM_CHLOCAL_DRIVE                  75 // change local drive
#define KEY_FM_LOAD_DESCRIPTIONS              76 // load 00index.txt file
#define KEY_COPY                              77 // copy file(s)/directory
#define KEY_MOVE                              78 // move file(s)/directory
#define KEY_CHANGE_PERMISSIONS                79 //  change access rights

#define KEY_FM_SELECTALL                      80 // select all files
#define KEY_FM_DESELECTALL                    81 // deselect all files
#define KEY_FM_SELECT                         82 // select file under cursor
#define KEY_FM_MASK_SELECT                    83 // select by mask
#define KEY_FM_MASK_DESELECT                  84 // deselect by mask
#define KEY_FM_INVERT_SEL                     85 // invert selection
//#define KEY_FM_SELECT_ALLDIRS                 86 // select all files
//#define KEY_FM_DESELECT_ALLDIRS               87 // deselect all files
#define KEY_SELECT_CC                         88 // select which CC to display
//

#define KEY_UPDATE_NFTP                       90 // connects to NFTP site
#define KEY_SELECT_LANGUAGE                   91 // select language
#define KEY_REGISTER                          92 // enter registration info
#define KEY_SELECT_FONT                       93 // select font for NFTP window
#define KEY_GEN_EXPAND_RIGHT                  94 // make window 1 column wider
#define KEY_GEN_CONTRACT_LEFT                 95 // make window 1 column narrower
#define KEY_GEN_EXPAND_DOWN                   96 // make window 1 row taller
#define KEY_GEN_CONTRACT_UP                   97 // make window 1 row shorter
#define KEY_TRAFFIC_LIMIT                     98 // limit throughput
//

/* Keys which cannot be redefined */

#define KEY_UP                               101 // up
#define KEY_DOWN                             102 // down
#define KEY_RIGHT                            103 // right
#define KEY_LEFT                             104 // left
#define KEY_PGUP                             105 // pageup
#define KEY_PGDN                             106 // pagedown
#define KEY_HOME                             107 // home
#define KEY_END                              108 // end
#define KEY_ESC                              109 // esc

#define KEY_ENTER                            110 // enter
#define KEY_BACKSPACE                        111 // backspace
#define KEY_SUSPEND                          112 // suspend

#define KEY_FTPS_FIND                        120 // find with FTPSearch
#define KEY_FTPS_DEFINE_SERVERS              121 // define FTPSearch servers
#define KEY_FTPS_RECALL                      122 // display results of last search

