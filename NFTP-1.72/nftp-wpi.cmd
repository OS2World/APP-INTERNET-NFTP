/* */

call directory "nftp-1.61"

"D:\Apps\WarpIN\wic.exe nftp-1.61.wpi -a 1 file_id.diz LICENSE makeobjs.cmd nftp.bm nftp.exe nftp.i nftp.ico nftppm.exe nftppm.ico order.frm readme.1st regform.txt english.nls english.mnu"
"D:\Apps\WarpIN\wic.exe nftp-1.61.wpi -a 2 *.mnu *.nls";
"D:\Apps\WarpIN\wic.exe nftp-1.61.wpi -a 3 readme.xfree86 nftpx.exe xnftp.exe";
"D:\Apps\WarpIN\wic.exe nftp-1.61.wpi -s ..\nftp.wis"
"mv -f nftp-1.61.wpi ..\.."

exit
