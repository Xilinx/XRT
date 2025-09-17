#!/usr/bin/perl

use strict;
use warnings;

use Dpkg::Control::Info;
use Dpkg::Deps;

my $fields = Dpkg::Control::Info->new()->get_source();
my %options = (reduce_restrictions => 1);

print(deps_concat(
	deps_parse($fields->{'Build-Depends'}, %options),
	deps_parse($fields->{'Build-Depends-Arch'} || '', %options) || undef,
	deps_parse($fields->{'Build-Depends-Indep'} || '', %options) || undef,
))
