#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asvtools.h>
#include "scl.h"

int main (int argc, char *argv[])
{
    int    bits, length, rc, v;
    double t1, t2;
    char   *s, *p, *pub_key, *sec_key, *p1, *sig;

    /*
    s = "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        "the sample string of arbitrary contents. what a waste of computing time! "
        ;
    */
    s = "the sample string of arbitrary contents";
    
    if (argc != 2) error1 ("usage: egtest <bits>\n");
    
    bits = atoi (argv[1]);

    fprintf (stderr, "generating key set (%d bits)\n", bits);
    t1 = clock1 ();
    rc = eg_keypair (bits, 2, &pub_key, &sec_key);
    t2 = clock1 ();
    if (rc) error1 ("key generation failed\n");
    printf ("secret: %s\n", sec_key);
    printf ("public: %s\n", pub_key);
    printf ("key generation took %f seconds\n", t2-t1);
    
    //pub_key = "C101050016D15B03A7E35042E7C36DB91B125E0BE4F2FF0E6352E9FF26ACD481D255EB791585594A9C178140F0EF8036CF4E39B4215FDEEDDB2B96A366ED8C05:010001";
    //sec_key = "C101050016D15B03A7E35042E7C36DB91B125E0BE4F2FF0E6352E9FF26ACD481D255EB791585594A9C178140F0EF8036CF4E39B4215FDEEDDB2B96A366ED8C05:9CA9C4324F533EEA97C3A729B7D0E566A4B7E3090A9DC83518A07BFA359D829C36F27DEC9AF4926007FE4ADA88581481E7F8031FB329919020B2F0BA24783F21";
    
    //pub_key = "ED3490A3B4897539B75FB70DA19D61191DAD5ECE2DC595B37893456F02104A41648326C58A6FC9330937899C19F6318B4007FC8EB7B2E1C3F6EB6DD52459BB4E1852444C2A7A0ADE04C4A957C0F19EFDB6FFBCD20EDB1FDC00D166EF01388B0F5C7697D69E87135625B1F173982A3BE2BDCE40EEE3E90781BA9278DFBA665FA1512B85203620D0BAC59EC613F2AE699B68B2AA275A604CA7EB1BDEF58398DF941B91EC5BEBB9D0E418CF154C970D253297524B9B9B9682D30E88473FCB445A71C4B8F3A261312883A84687E38BE7DB3B7E1475976C58BC115DB79A0F9294D1BDACB4A634550473C6D3B320D1C5AB3017D95387FD4545BDFE2361D83C101FDCCD:010001";
    //sec_key = "ED3490A3B4897539B75FB70DA19D61191DAD5ECE2DC595B37893456F02104A41648326C58A6FC9330937899C19F6318B4007FC8EB7B2E1C3F6EB6DD52459BB4E1852444C2A7A0ADE04C4A957C0F19EFDB6FFBCD20EDB1FDC00D166EF01388B0F5C7697D69E87135625B1F173982A3BE2BDCE40EEE3E90781BA9278DFBA665FA1512B85203620D0BAC59EC613F2AE699B68B2AA275A604CA7EB1BDEF58398DF941B91EC5BEBB9D0E418CF154C970D253297524B9B9B9682D30E88473FCB445A71C4B8F3A261312883A84687E38BE7DB3B7E1475976C58BC115DB79A0F9294D1BDACB4A634550473C6D3B320D1C5AB3017D95387FD4545BDFE2361D83C101FDCCD:947161D6E0E368D5EEB8DA80905441F08ACEF55687F46F83BA0047E2796322651BF6AB6BD1FFB4E4D86EC6124778F7765262ED1D8F3E45E4F2005162A275F92E9D2FD687E7C92A45D8AC0DE1D9E01B84616257930932FE141AFAF0B4BF89D8148BE3B78FDAF36319754B8F73AC953996E2FB514D6E9965563D20EEBDEC13A6E1C3CE23BBA1975A6C8FA99E09D0A7A9A55531809BD4AE8B5A825F94EA60D5446E3164B879599277569D6C0C357F77AF1F62A330A7831469422C4C6E44DC3470805E1D42E240B9D51923FA40930728F4C41D765133F8EDE738A25279749A446F375C4DC5EC8445631DD206D2DBC029C159A6CAB0F41693BDCE06C8ABC8D323754D";
    
    //pub_key = "C0171F098FB8B5C8E734E60BF8E87A7B95174561F1F037E4500049FF249BCF0E6B:730BE6A9B984DCF8DD7E5530144D7DBB7F1F297ECBD10C5B705A76EFC2EE9075C3";
    //sec_key = "C0171F098FB8B5C8E734E60BF8E87A7B95174561F1F037E4500049FF249BCF0E6B:2EE0726CD2C8284F18383FEB5399A2BDFE9B6E3C203488DB6E3E62914F3F96ECB";
    fprintf (stderr, "encoding....\n");
    t1 = clock1 ();
    p = eg_encode (s, strlen (s), pub_key);
    t2 = clock1 ();
    printf ("encoded: [%s]\n", p);
    printf ("encoding took %f seconds\n", t2-t1);

    fprintf (stderr, "decoding....\n");
    t1 = clock1 ();
    length = eg_decode (p, sec_key, &p1);
    t2 = clock1 ();
    if (length < 0) error1 ("%d from rsa_decode1()\n");
    printf ("decoded: [%s]\n", p1);
    printf ("compare result: %d\n", strcmp (s, p1));
    printf ("decoding took %f seconds\n", t2-t1);
/*
    // signature compute test
    fprintf (stderr, "signing....\n");
    t1 = clock1 ();
    sig = eg_sign (s, strlen (s), sec_key);
    t2 = clock1 ();
    printf ("signature: [%s]\n", sig);
    printf ("signing took %f seconds\n", t2-t1);

    // signature verification test
    fprintf (stderr, "verifying signature....\n");
    t1 = clock1 ();
    v = eg_verify (s, strlen (s), sig, pub_key);
    t2 = clock1 ();
    printf ("verification result: %d\n", v);
    printf ("verification took %f seconds\n", t2-t1);
*/
    return 0;
}

