
/* returns 0 if OK, 1 if no files to transfer */
int mirror (int d, int noisy, mirror_options_t *mo);

int get_mirror_options (mirror_options_t *mo);
int set_mirror_options (mirror_options_t *mo);
int edit_mirror_options (mirror_options_t *mo);
