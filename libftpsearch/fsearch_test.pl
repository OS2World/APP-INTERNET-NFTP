#!/usr/bin/perl
use strict;
use fsearch;

print "setting up query...\n";
my $req = fsearch::FTPS_query ("nftp", 16, 0, 2, 0, 0, '', '',
                               '', '', 0, 0, 0, 0);

print "making request...";
my $rc = fsearch::FTPS_search ("ftpsearch.rambler.ru", 0, $req, "results.tmp", 20, 15.0);
print "rc = $rc\n";

my $n = fsearch::FTPS_num_results ($req);
print "$n hits\n";

my $i;
for ($i=0; $i<$n; $i++)
{
    print "$i. ";
    print fsearch::FTPS_get_hostname($req, $i), ":";
    print fsearch::FTPS_get_pathname($req, $i), "/";
    print fsearch::FTPS_get_filename($req, $i), "\n";
}

fsearch::FTPS_delete ($req);


