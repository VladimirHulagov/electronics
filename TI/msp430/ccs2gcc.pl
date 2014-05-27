#!/usr/bin/perl -w

use strict;

for ($_) {
    chomp;
    print "$_";
    print "\n";
    open I, "<$_" and do {
        my $outfile = $_;
        my $vector;
        open O, ">$outfile" and do {
                chomp;
                /^#include/ and print O "#include \"signal.h\"\n";
                /^#pragma\s+vector\s*=\s*(\w+)/ and do { $vector = $1; next; };
                /^__interrupt\s+void\s*(\w+)\s*\(void\)/ and $_ = "interrupt($vector) $1(void)";
                print O "$_\n";
            }
            close O;
        };
        close I;
    };
}
