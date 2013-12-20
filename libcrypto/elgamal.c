#include <stdlib.h>
#include <string.h>
#include <asvtools.h>

#include "scl.h"
#include "bn.h"

/*
 Internal structure of ElGamal public and private keys is more complicated
 than in RSA. Generating ElGamal key includes selecting generator g (usually
 2 or 5), computing suitable large prime p, selecting random number a
 smaller than p, modular exponentiation g^a mod p.
 
 Private key contains: p (prime), g (generator), key (secret random)
 Representation:    p,g:key
 
 Public  key contains: p (prime), g (generator), pbk=g^a mod p (public key)
 Representation:    p,g:pbk
 
 In all cases numbers are written in hex. The common sets of parameters
 can be used instead of p,g pairs: when first part of the key (before colon)
 does not contain comma it is assumed to be the ordinal number of the
 precomputed p,g combination. This is secure (see books on ElGamal).
 It is recommended to use at least 1024 bits; lower values are mostly
 provided for testing. These can be found below in this file and were
 computed by myself. Note that even if you use precomputed values you
 still have to generate keypairs, but instead of actual number of bits
 specify the ordinal from the table above. 'generator' will be ignored
 by eg_keypair subroutine in this case
 */

 /*
*/

static struct
{
    int   no;
    int   bits;
    int   generator;
    char  *prime;
}
precomp[] =
{
    { 0,  256, 5,
    "F13D8E344CF07CF9DCE7BEBA009CD371D111060033B0D83041DEEAB8DF0235AF"},
    { 1,  512, 5,
    "D14C41220F8464DCE9BC3493807EB0401AF0BACA5C680896BDBB1EC9BCCB2BE2"
    "8D411FFD642B968DDD7F22BD6F5262AB15E9F65DE6489C8EAE925C50D625E0AB"},
    { 2,  768, 5,
    "CFF0CACECF310561E9AD5E553C5946FEFFEFCBDDBF1688D715FF8356A54FF371"
    "716027A352E94B923C37801D194A4320557660424EC1F3F1FCC9267930A8077B"
    "A5D59E26CFCFDEF7C340EFA687A3F6F5631251C2FB7E92A58B1C9FB4466DAA43"},
    { 3, 1024, 5,
    "A68DEE7BA331AE509C58BF3A7D36857D9ED342923A430432A3822FB4EA8EE3EB"
    "980C8435F8834CA8D46A1C3DA8CC22BAAFB96E119040ABA58FA8831035F0C3B1"
    "E0B9B822EB6D1E49E0334E0BB10D1FB9715BD8A05E115CC0757E99605C591831"
    "3764B28121E782F2BDADB9E47BC5F925EE803286009F513A7AB6BEB022A6413B"},
    { 4, 1536, 5,
    "93A1D611FC03BD11AF5B697CD873E56A52AA68B32ED7113794F9018BC692D53D"
    "0FFF2B148B09585D672F3DD8985F8AE20CFEB1836F6F9BC91C386C12EA11FC90"
    "2ABDD52AC00B77E3032DBAFD223B4C4F36435F94ABF71411749B6E689CEBA270"
    "4917B98B4AA740C84840E721E5330E6BBF03CF581093CEF2DFABE0E95ADCED75"
    "6FB01DB5B30D7AC7C253A3186C61DFAB22FF190740B5A512BEAF9B0BAB8F3727"
    "ECD894770EF7CD4F097A07C9E81FB9AFCA64ECF2005F9737546D2F67B90481AB"},
    { 5, 2048, 5,
    "EAC4942419C7D811564E30F2ADC7999AF0D2A6C5BED46A7DB4BD72B5B7478E1C"
    "EFA576A5590155514C468319F3319CA478205E26C22F9A583E7616549B4F4774"
    "5181409DC0B9EC2E00B569E1B0C9F0DE9304A751C9598006E43A47E6CBE2850F"
    "E8135014BC000B836A4A854574DC15F960A0A134F75797E62B16A1744F287AE2"
    "AAF67822568510E7EB78340F33B877503A470C1E81B2AA0CA9BFB943324D0D28"
    "EFAE01CF6C725B121802B7D0530ED375199F1DE2DDD34D4043DCB68948986FB4"
    "9675D04DEDEA17AD40EBF6075E3F285DFEC09243A72474681F3082AFEBFC8182"
    "0E35D0A212F2304F0061D1F829D1E0A75D19248CA074AE5DC431CA137734C14B"},
    
    { 6,  256, 2,
    "80078185C4FFA2F11C7C8F9118FC9DD52990C66C464A0C8052CE4DD54FFB8C7B"},
    { 7,  512, 2,
    "96D3506116A219961987858351B03D7D5C742E5D177A10584DF4A846ADAE2136"
    "E3CD487577E8D13A31E3479AB4589B2148C16911F049BF637FFB6719102F3A7B"},
    { 8,  768, 2,
    "9DF5457F53E91BDF86F0BE101960FD514F559EE64829CD0ABDAD636C7FD2B58C"
    "92FB9B2C3DA6708922F1C8DC0658D3F10173BF88EDA60CEB5D39754DC9C8FE65"
    "8A074F5D3D1E9C516AE1B5B74BDB0646397C640D67BB9CB65C04F7793D72B68B"},
    { 9, 1024, 2,
    "BCE5A584D4F18830A5AF50E3EA1D17850427B524F1282C874B53F3BB632224F9"
    "691D562B3E8DCBE19EAC14AEBB1D8C0E4DBB3D8070B0CBD279F4C72FB4757476"
    "F1AA05AA4FB048E674691603A219725F1BCD67BAC73696B5796CC521BFB735EE"
    "CBF0CF02484A0B322152E96B0C696148D9541BCD6A16283A1B6470540AA4BA13"},
    {10, 1536, 2,
    "CFC6C09E932FEAB5BD1093C2AA2EB2BD5C89765C45BAE32EC870C5253F2945E2"
    "5E45BC7CFF80BBD4ADAFA91E186DA42DB05F5148371483AE042369236EED5E6F"
    "4EE49CDF66233F5FCFF2B9AC9B233605F2A39C457E2ECAA6AC7E6F440CA4AB6B"
    "EAE60F657A78B46A909C278475ED22AF3E82E4571671B101CC2C554FCC8AE7DC"
    "C2E9FC4D542569AA2F03BE2858733F59DB82A1CA0771BC66106C7F5D04504565"
    "28276A428ECB6B6D4843547EED01A6296DC71BAE92D77EDEE729151E83C0154B"},
    {11, 2048, 2,
    "85CE416F75CE60BE90FA50B141F0496B5566454130A6EE49F2C52A7C165E8B0B"
    "F242F334EC2A9CB36EBEAA1352DA4AEAA1D8251B22B3E5D8751531668A0E3B70"
    "B504A8D5429A6FE39039407EE1CB3E83AF6713B13810B1168E3A0874AF4EDCE4"
    "7E0EA2C4A2B7411FD722D27A5B6E888933ABA35F3DBA2D13E47CC6055A72DCDB"
    "261E7B109BC6820E20EB8B5CE92D57A71983EF2E7D65F4F5DD9C9E5FE1938022"
    "15C89EE020C0C76D8EFA933B2DEC69CAF3716439CC88C33172B72952FAE20271"
    "C52C6D84ECC736DF91542EA88BE93BD2F1EAE0A73877EFE486AE9FA430FAC7C6"
    "1E4C2286A449A739BA6E9CAAF697249E25C22A514B12FBC426D00826D440A08B"},
    {12, 4096, 2,
    "AE2D3C8F0B9733DCC4CD8CADF834C5CFA94AB13B409B6A9295D2E430A83EE637"
    "AFF15C6FE3E35BF033C239E82AF3DBC50E309B8EE72841CCA047BB9A0771B84E"
    "D8DF9FD41AAB8839AF74F30969DC2DF767E393E2DE4FC615FF4A798379430796"
    "E29AA7193D8B9116AFB498A69ED7BF47F00793A62D7ADD7F5E72F979D44327A4"
    "0A4153A0F08ACCCF46D9E43DA046EDFB7746172D71AF4BB0F4A6015A95C7130C"
    "D39D4EE361514436AE9AE622F95DEAEC5354FFEAA65A185D58533B989F871E62"
    "35E6D2FF2557BB9F4340B7C7F39F13752E6E28757F6CE66C6C52892BCF24ED33"
    "4336B2821D884716E225D08517CDD1D4758A802FD3CFD67A0E3C1501DEB14725"
    "A2F98BB5799CFF51D577B08D0F87417F823595E7D7D34D2835C4041518AAB070"
    "0A380DF8E069C9828B9BA9EC3D818E2B5788F9B52FE7B484474BCA9AC90CA47A"
    "E6F2AAF017FDD971132BB3B6FBFED99288F4FC791A8914851BFD84D87AD83186"
    "99A96620621410A69863434D5444E800EA192D97C5CACD3681D27C1AEB2D0821"
    "AE7770A59AB89AEEAE6F51260816CDCA01280F91F77C73AB8E829410917F29D9"
    "6A6801D1435DF4DB0690CCD581E3E7AA177EC5553036727C9624D1FBB9AD936D"
    "BF893DA35C3DBA3698BD5230C7282EC06399EBD522CE26D28C77DAB420872D32"
    "0755B798BC947263761BBC80AA7F6F34D6301C7760C50218A50B8306B855BF73"},
};

/* generates ElGamal key pair. returns 0 when generation went ok, and
 -1 if error occured. 'bits' is the number of bits in p; it should not
 be too low (at least 512 is recommended, 1024 is more realistic number.
 you can use precomputed p,g pairs; set bits to the ordinal of the
 precomputed combination (see table above). generator is either 2 or 5.
 public_key and secret_key will be malloc()ed and contain keys */
int eg_keypair (int bits, int generator, char **public_key, char **secret_key)
{
    BIGNUM       *p, *g, *t1, *t2, *key, *pbk;
    BN_CTX       *ctx2;
    BN_MONT_CTX  *mont;
    char         *buf1, *buf2, *buf3, *buf4, buf[8];
    int          rc;

    // create things needed for work
    ctx2 = BN_CTX_new ();         if (ctx2 == NULL) return -1;
    t1   = BN_new ();             if (t1 == NULL)   return -1;
    t2   = BN_new ();             if (t2 == NULL)   return -1;
    g    = BN_new ();             if (g == NULL)    return -1;
    key  = BN_new ();             if (key == NULL)  return -1;
    pbk  = BN_new ();             if (pbk == NULL)  return -1;
    mont = BN_MONT_CTX_new ();    if (mont == NULL) return -1;

    if (bits < 32)
    {
        if (bits > sizeof(precomp)/sizeof(precomp[0])-1) return -1;
        p = NULL;
        rc = BN_hex2bn (&p, precomp[bits].prime);
        if (rc == 0) return -1;
        // put generator into bignum
        BN_set_word (g, precomp[bits].generator);
    }
    else
    {
        // set values which will be used for checking when generating proper prime
        if (generator == 2)
        {
            BN_set_word (t1,24);
            BN_set_word (t2,11);
        }
        else if (generator == 5)
        {
            BN_set_word (t1,10);
            BN_set_word (t2,3);
            /* BN_set_word(t3,7); just have to miss
             * out on these ones :-( */
        }
        else
            goto err;
    
        // generate proper prime
        p = BN_generate_prime (NULL, bits, 1, t1, t2, NULL, NULL);
        if (p == NULL) goto err;

        // put generator into bignum
        BN_set_word (g, generator);
    }

    // create random private key
    if (!BN_rand (key, BN_num_bits (p)-1, 0, 0)) goto err;

    // create public part of the key
    BN_MONT_CTX_set (mont, p, ctx2);
    if (!BN_mod_exp_mont (pbk, g, key, p, ctx2, mont)) goto err;

    // p, g, key, pbk are ready. secret key: p,g:key, public key: p,g:pbk
    if (bits < 32)
    {
        snprintf1 (buf, sizeof(buf), "%d", bits);
        buf1 = strdup (buf);
    }
    else
    {
        buf1 = BN_bn2hex (p);
    }
    buf2 = BN_bn2hex (key);
    buf3 = BN_bn2hex (pbk);
    buf4 = BN_bn2hex (g);

    *secret_key = malloc (strlen(buf1) + strlen(buf2) + strlen(buf4) + 4);
    *public_key = malloc (strlen(buf1) + strlen(buf3) + strlen(buf4) + 4);

    strcpy (*secret_key, buf1);
    if (bits >= 32)
    {
        strcat (*secret_key, ",");
        strcat (*secret_key, buf4);
    }
    strcat (*secret_key, ":");
    strcat (*secret_key, buf2);
    
    strcpy (*public_key, buf1);
    if (bits >= 32)
    {
        strcat (*public_key, ",");
        strcat (*public_key, buf4);
    }
    strcat (*public_key, ":");
    strcat (*public_key, buf3);
    memset (buf2, 0, strlen (buf2));
    free (buf1); free (buf2); free (buf3);

    // cleanup
    BN_free (p);            BN_free (g);
    BN_clear_free (key);    BN_free (pbk);
    BN_CTX_free (ctx2);
    return 0;
    
err:
    return -1;
}

/* ------------------------------------------------------------------ */
char *eg_encode (char *s, int length, char *public_key)
{
    // we use NULL-with-random padding. this allows to pass
    // NULL-terminated strings without any additional processing.
    // if you want to exchange binary data, define your own padding in
    // your application or pass the data length somehow

    BIGNUM         message, gamma, delta, k, temp;
    BIGNUM         *p=NULL, *g=NULL, *key=NULL;
    int            i, nl, nc, no, pc, rc1, rc2, rc3, index;
    unsigned char  *buf = NULL;
    BN_CTX         *ctx = NULL;
    char           *p1, *p2, *p3, *output;

    // setup key (p, g, key)
    p1 = strdup (public_key);
    p2 = strchr (p1, ':');
    if (p2 == NULL) {free (p1); return NULL;}
    *p2 = '\0';
    p3 = strchr (p1, ',');
    if (p3 == NULL)
    {
        index = atoi (p1);
        if (index > sizeof(precomp)/sizeof(precomp[0])-1) return NULL;
        p = NULL;
        rc1 = BN_hex2bn (&p, precomp[index].prime);
        if (rc1 == 0) return NULL;
        g = BN_new ();
        if (g == NULL) return NULL;
        BN_set_word (g, precomp[index].generator);
    }
    else
    {
        rc1 = BN_hex2bn (&p, p1);
        rc2 = BN_hex2bn (&g, p3+1);
        if (rc1 == 0 || rc2 == 0) return NULL;
    }
    rc3 = BN_hex2bn (&key, p2+1);
    free (p1);
    if (rc3 == 0) return NULL;
                 
    // initialize temp variables
    BN_init (&message);
    BN_init (&gamma);
    BN_init (&delta);
    BN_init (&k);
    BN_init (&temp);
    ctx = BN_CTX_new ();
    if (ctx == NULL) return NULL;

    // number of bytes in p. this is the amount of bytes
    // we can convert in one gulp
    nl = BN_num_bytes (p);
    buf = malloc (nl);
    if (buf == NULL) return NULL;

    // compute the 'nc', the number of cycles (gulps)
    nc = length/nl;
    if (length % nl) nc++;
    
    // preallocate output buffer: nl*2 -- bin->hex conversion,
    // nl*2*2 + 1 -- each gulp consists of two bignums and comma
    // between them, nl*2*2+1+1 -- spaces between gulps in the output
    no = nc * (nl*2*2+1+1) + 1;
    output = malloc (no);
    if (output == NULL) return NULL;
    output[0] = '\0';

    // cycle by pieces of input, each piece is 'nl' bytes long
    // (except the last one)
    for (i=0; i<nc; i++)
    {
        // compute piece length
        pc = (i == nc-1) ? length % nl : nl;
        memcpy (buf, s+i*nl, pc);
        // do NULL+random padding if necessary
        if (pc != nl)
        {
            buf[pc] = '\0';
            if (nl-pc-1 > 0)
                rand_bytes (buf+pc+1, nl-pc-1);
        }
        // convert to bignum
        BN_bin2bn (buf, nl, &message);
        // ElGamal: get random k, gamma = g^k mod p, delta = message * key^k mod p
        BN_rand (&k, BN_num_bits (p)-1, 0, 0);
        BN_mod_exp (&gamma, g,   &k, p, ctx);
        BN_mod_exp (&temp,  key, &k, p, ctx);
        BN_mod_mul (&delta, &temp, &message, p, ctx);
        // convert into hex
        p1 = BN_bn2hex (&gamma);
        p2 = BN_bn2hex (&delta);
        // copy result to output buffer and add delimiting space
        // fairly ineffective at the moment
        strcat (output, p1);
        strcat (output, ",");
        strcat (output, p2);
        if (i != nc-1) strcat (output, " ");
        free (p1);
        free (p2);
    }
    
    BN_CTX_free (ctx);
    BN_clear_free (p);
    BN_clear_free (g);
    BN_clear_free (key);
    memset (buf, 0, nl);
    free(buf);

    return output;
}

/* ------------------------------------------------------------------ */
int eg_decode (char *s, char *private_key, char **result)
{
    // there is no any padding processing in the decoding routine
    //  (see comment in rsa_encode)

    BIGNUM         message, *gamma, *delta, k, temp1, temp2, one;
    BIGNUM         *p=NULL, *g=NULL, *key=NULL;
    int            i, nl, nc, rc1, rc2, rc3, length, index;
    unsigned char  *buf = NULL;
    BN_CTX         *ctx = NULL;
    char           *p1, *p2, *p3;

    // setup key (p, g, key)
    p1 = strdup (private_key);
    p2 = strchr (p1, ':');
    if (p2 == NULL) {free (p1); return -1;}
    *p2 = '\0';
    p3 = strchr (p1, ',');
    if (p3 == NULL)
    {
        index = atoi (p1);
        if (index > sizeof(precomp)/sizeof(precomp[0])-1) return -1;
        p = NULL;
        rc1 = BN_hex2bn (&p, precomp[index].prime);
        if (rc1 == 0) return -1;
        g = BN_new ();
        if (g == NULL) return -1;
        BN_set_word (g, precomp[index].generator);
    }
    else
    {
        rc1 = BN_hex2bn (&p, p1);
        rc2 = BN_hex2bn (&g, p3+1);
        if (rc1 == 0 || rc2 == 0) return -1;
    }
    rc3 = BN_hex2bn (&key, p2+1);
    free (p1);
    if (rc3 == 0) return -1;

    // initialize temp variables
    BN_init (&message);
    BN_init (&k);
    BN_init (&temp1);
    BN_init (&temp2);
    BN_init (&one);
    BN_one (&one);
    gamma = BN_new ();
    if (gamma == NULL) return -1;
    delta = BN_new ();
    if (delta == NULL) return -1;
    ctx = BN_CTX_new ();
    if (ctx == NULL) return -1;

    // number of bytes in the modulus. this is the amount of bytes
    // we can convert in one gulp and should expect to be in one
    // group
    nl = BN_num_bytes (p);
    buf = malloc (nl);
    if (buf == NULL) return -1;

    // find the number of pieces in the encrypted message (the last
    // piece is not terminated with space)
    nc = str_numchars (s, ' ') + 1;
    
    // preallocate output buffer
    length = nc * nl;
    *result = malloc (length);
    if (*result == NULL) return -1;

    // cycle by pieces of input, each piece is 'nl' bytes long
    // (except the last one)
    p1 = s;
    for (i=0; i<nc; i++)
    {
        // extract next piece
        p2 = strchr (p1, ' ');
        if (p2 == NULL)
        {
            if (i != nc-1) return -1;
        }
        else
        {
            *p2 = '\0';
        }
        p3 = strchr (p1, ',');
        if (p3 == NULL) return -1;
        *p3++ = '\0';
        // convert to bignum
        rc1 = BN_hex2bn (&gamma, p1);
        if (rc1 == 0) return -1;
        rc1 = BN_hex2bn (&delta, p3);
        if (rc1 == 0) return -1;
        // ElGamal
        BN_sub (&temp1, p, &one);
        BN_sub (&temp2, &temp1, key);
        BN_mod_exp (&temp1, gamma, &temp2, p, ctx);
        BN_mod_mul (&message, &temp1, delta, p, ctx);
        // convert into binary output
        BN_bn2bin (&message, (unsigned char *)(*result+i*nl));
        // advance pointer to prepare search for next piece
        p1 = p2 + 1;
    }
    
    BN_CTX_free (ctx);
    memset (buf, 0, nl);
    free(buf);

    return length;
}
