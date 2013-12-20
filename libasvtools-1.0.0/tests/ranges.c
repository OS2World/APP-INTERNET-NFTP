#include <asvtools.h>

/* id_range_t ir1[] = {{1,1}, {8191,28100}, {36600,36950}, {248308,4000000}}; */
id_range_t ir1[] = {{1,4000000}};
id_range_t ir2[] = {{3647,3677}, {3888,4159}, {4863,6604},
{8191,28176}, {30395,30395}, {36639,36913}, {37071,37236},
{56902,56905}, {58603,58607}, {58862,58869}, {71709,71853},
{91144,91147}, {91225,91269}, {248308,263451}, {267241,272264}};

id_range_t ir3[] =
{
    {8519,11119}, {8519,11119}, {8519,11119}, {248308,263451},
    {251197,258151}, {251197,258151}, {262242,262763}, {262242,262763},
    {262643,262643}
};

int main (int argc, char *argv[])
{
    int rc, i, nr, nr3;
    rangeset_t rs1, rs2, rs3;
   /* id_range_t *ir; */

    rs1.ir = ir1;
    rs1.nr = sizeof(ir1) / sizeof(id_range_t);
    rs2.ir = ir2;
    rs2.nr = sizeof(ir2) / sizeof(id_range_t);
    nr3 = sizeof(ir3) / sizeof(id_range_t);

    rc = ranges_intersect (&rs1, &rs2, &rs3);

    printf ("results (%d ranges),\n", rs3.nr);
    for (i=0; i<rs3.nr; i++)
        printf ("%d:%d ", rs3.ir[i].s_id, rs3.ir[i].e_id);
    printf ("\n");

    printf ("\n\nmerging ");
    for (i=0; i<nr3; i++)
        printf ("%d:%d ", ir3[i].s_id, ir3[i].e_id);
    printf ("\n");

    nr = merge_ranges (nr3, ir3);

    printf ("results (%d ranges),\n", nr);
    for (i=0; i<nr; i++)
        printf ("%d:%d ", ir3[i].s_id, ir3[i].e_id);
    printf ("\n");

    return 0;
}
