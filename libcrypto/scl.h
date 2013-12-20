/* random number generation ------------------------------------------- */

void rand_cleanup (void);

void rand_seed (const void *buf, int num);

/* put 'num' random bytes into 'buf'. 'buf' must be at least
 'num' bytes long */
void rand_bytes (unsigned char *buf, int num);

/* RSA encryption ----------------------------------------------------- */

int  rsa_keypair (int bits, unsigned long e_value, char **public_key, char **secret_key);
char *rsa_encode (char *s, int length, char *key);
int  rsa_decode (char *s, char *key, char **result);

/* computes RSA signature for 'message' of size 'length' with 'private_key'. 
 returns malloc()ed buffer with hexadecimal representation of signature */
char *rsa_sign (char *message, int length, char *private_key);
    
/* verifies RSA signature computed by rsa_sign(). message has 'length',
 signature is in hex representation (as computed by rsa_sign).
 returns 1 if verified, 0 if verification failed */
int rsa_verify (char *message, int length, char *signature, char *public_key);

/* ElGamal encryption --------------------------------------------- */

int   eg_keypair (int bits, int generator, char **public_key, char **secret_key);
char *eg_encode (char *s, int length, char *key);
int   eg_decode (char *s, char *key, char **result);
/*char *eg_sign (char *message, int length, char *private_key);*/
/*int   eg_verify (char *message, int length, char *signature, char *public_key);*/

/* Blowfish encryption --------------------------------------------- */

char *bf_encrypt (char *s);
char *bf_decrypt (char *s);
void bf_setkey (char *key);
void blowfish_setkey (char *key, int keylen);
void blowfish_encrypt (char *src, char *dest, int len);
void blowfish_decrypt (char *src, char *dest, int len);

/* Secure Hash Algorithm (SHA1) ------------------------------------ */

/* computes SHA1 digest for message 'd' of length 'n'. result is stored in 'md'.
 if 'md' is NULL, the pointer to static internal buffer of 20 bytes is returned.
 the result is binary! */
unsigned char *sha1 (const unsigned char *d, unsigned long n, unsigned char *md);

/* computes SHA1 digest. returns pointer to NULL-terminated, malloc()ed
 buffer of 41 bytes (the digest) in hexadecimal form */
char *sha1x (char *s);

