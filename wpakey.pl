#!/usr/bin/perl
#------------------------------------------------------------------------------
# WPA key editor with 32-byte hex key support. Designed for Maemo 5.
#
# Copyright (C) 2010, Shaun Amott <shaun@inerd.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id: wpakey.pl,v 1.1.1.1 2010/06/15 18:44:06 samott Exp $
#------------------------------------------------------------------------------

use strict;
use warnings;


#------------------------------------------------------------------------------
# Globals
#------------------------------------------------------------------------------

my (%ci, $net, $key, $count);


#------------------------------------------------------------------------------
# Begin code
#------------------------------------------------------------------------------

if (@ARGV != 2) {
	print "Usage:\n";
	print "       $0 <name> <key>\n";
	exit 1;
}

$net = $ARGV[0];
$key = $ARGV[1];

conninfo(\%ci)
	or die "Couldn't query connection info";

$count = 0;

foreach my $guid (keys %ci) {
	if ($ci{$guid}->{name} && $ci{$guid}->{name} eq $net) {
		print "Setting key for $guid...\n";
		setkey($guid, $key)
			or die 'Failed to set key';
		$count++;
	}
}

print "Updated $count connections.\n";

exit ($count ? 0 : 1);


#------------------------------------------------------------------------------
# Func: conninfo()
# Desc: Query system config tool for info about all connections.
#
# Args: \%res    - Hash of results (GUIDs as keys, hashref of data as values).
#
# Retn: $success - true/false.
#------------------------------------------------------------------------------

sub conninfo
{
	my ($res) = @_;

	my (@outp, $guid);

	@outp = split /\n/, qx(gconftool-2 -R /system/osso/connectivity/IAP);

	if ($?) {
		print STDERR "gconftool-2 failed\n";
		return 0;
	}

	foreach my $line (@outp) {
		my ($key, $val);

		if ($line =~ /^\s\/system\/osso\/connectivity\/IAP\/(.+):$/) {
			$guid = $1;
			$res->{$guid} = {} if !$res->{$guid};
			next;
		} elsif ($line =~ /^\s\s(\S+)\s*=\s*(.*)$/) {
			$key = $1;
			$val = $2;
		}

		if ($guid && $key) {
			$res->{$guid}->{$key} = $val;
		}
	}

	return 1;
}


#------------------------------------------------------------------------------
# Func: setkey()
# Desc: Set a new key for the given GUID.
#
# Args: $guid    - GUID of connection.
#       $key     - New key (32-byte hex key or passphrase).
#
# Retn: $success - true/false.
#------------------------------------------------------------------------------

sub setkey
{
	my ($guid, $key) = @_;

	my $path = "/system/osso/connectivity/IAP/$guid";

	if ($key =~ /^[A-Fa-f0-9]{64}$/) {
		# Valid hex format

		my (@bytes, $bytelist);

		@bytes =
			map +(hex $_),
			grep {$_} split /(..)/,
			$key;

		$bytelist = join ',', @bytes;
		$bytelist = '['.$bytelist.']';

		# Remove passphrase
		qx(gconftool-2 --unset $path/EAP_wpa_preshared_passphrase);

		# Set bytes
		qx(gconftool-2 --set --type list --list-type int $path/EAP_wpa_preshared_key $bytelist);

		if ($?) {
			print STDERR "gconftool-2 failed\n";
			return 0;
		}

		return 1;
	} elsif (length $key == 64) {
		print STDERR '64-char key specified, but not valid hexadecimal;'
			. " setting as (invalid) passphrase\n";
	} elsif (length $key < 8 || length $key > 64) {
		print STDERR "Invalid key length; setting anyway\n";
	}

	# Remove key
	qx(gconftool-2 --unset $path/EAP_wpa_preshared_key);

	# Set passphrase
	qx(gconftool-2 --set --type string $path/EAP_wpa_preshared_passphrase "$key");

	if ($?) {
		print STDERR "gconftool-2 failed\n";
		return 0;
	}

	return 1;
}
