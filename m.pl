#!/usr/bin/perl
open(F,"text");
while($_=<F>) {
@A=split("vi " , $_);
print "git add $A[1]";  
}
close(F);


