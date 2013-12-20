#ifndef DO_KEY_H_INCLUDED
#define DO_KEY_H_INCLUDED

void do_key (int key);

void do_select_language (int key);
void do_enter_dir (int key);
void do_view (int key);
void do_mkdir (int key);
void do_show_cc (int key);
void do_delete (int key);
void do_copy (int key);
void do_move (int key);
void do_chdir (int key);
void do_select_panel_contents (int key);
void do_rename (int key);
void do_chmod (int key);
void do_reread (int key);
void do_go_root (int key);
void do_load_descriptions (int key);
void do_walk (int key);
void do_sort (int key);
void do_mark (int key);
void do_mark_by_mask (int key);
void do_mark_multiple (int key);
void do_export_marked (int key);
void do_save_listing (int key);
void do_select_font (int key);
void do_exit (int key);
void do_edit_ini (int key);
void do_logoff (int key);
void do_quote (int key);
void do_custom_1 (int key);
void do_custom_2 (int key);
void do_scroll (int key);
void do_launch_wget (int key);
void do_ftps_find (int key);
void do_ftps_recall (int key);
void do_binascii (int key);

void do_transfer (int where, int from, int to);
void do_upload_marked (int pnl, int nsite);
void do_download_marked (int nsite, int pnl);
void do_fxp (int ns1, int ns2);

void drawline_select_language (int n, int len, int shift, int pointer, int row, int col);
void drawline_select_font (int n, int len, int shift, int pointer, int row, int col);
void drawline_conn (int n, int len, int shift, int pointer, int row, int col);
void drawline_goto (int n, int len, int shift, int pointer, int row, int col);

#endif
