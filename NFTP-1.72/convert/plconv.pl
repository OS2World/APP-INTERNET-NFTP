#!/usr/bin/perl
#
#        a,  c'  e,  l\  n'  o'  s'  z'  z.  A,  C'  E,  L\  N'  O'  S'  Z'  Z~
%ENC = (
'MAZ'=>"\206\215\221\222\244\242\236\246\247\217\225\220\234\245\243\230\240\241",
'LAT'=>"\245\206\251\210\344\242\230\253\276\244\217\250\235\343\340\227\215\275",
'WIN'=>"\271\346\352\263\361\363\234\237\277\245\306\312\243\321\323\214\217\257",
'ISO'=>"\261\346\352\263\361\363\266\274\277\241\306\312\243\321\323\246\254\257",
'TXT'=>"acelnoszzACELNOSZZ"
);

sub ShowHelp ()
{
    print "Usage: $0 encoding_in encoding_out < inpit_file > output_file\n";
    print "  where encodings could be:\n";
    print "      MAZ - Mazovia\n";
    print "      LAT - Latin-2 (MS-DOS CP 852)\n";
    print "      WIN - Windows ANSI (CP 1250)\n";
    print "      ISO - ISO Latin-2 (ISO 8859-2)\n";
    print "      TXT - ASCII (7-bit)\n";
    exit;
}

($std1,$std2) = (shift,shift);

ShowHelp()
  if (($std1 !~ /^(MAZ|LAT|WIN|ISO|TXT)$/) ||
      ($std2 !~ /^(MAZ|LAT|WIN|ISO|TXT)$/));

$std1 = $ENC{$std1};
$std2 = $ENC{$std2};

while (<>) {
    do {
      eval "tr/$std1/$std2/, 1" or die $@;
    } if not (/^\s*{/ && /{\s*(M_[HV]BAR|M_CROSS|M_FILL\d+|M_[DLRU][DUT]|M_PLACEHOLDER|M_MARKSIGN)\s/);
    print;
}

