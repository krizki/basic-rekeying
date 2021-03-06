#!/usr/bin/perl
use File::Copy;

my @delfiles 	= glob('/home/user/contiki/examples/basic-rekeying/one2one-node-*.c');
for my $delfile (@delfiles) {
  system("rm", $delfile, "-f");
}

my @conffiles 	= glob('/home/user/contiki/examples/basic-rekeying/conf/*.nocfg');
for my $conffile (@conffiles) {
  system("rm", $conffile, "-f");
}

my @files 	= glob('/home/user/Basic*/*.nocfg');
$k 		= 1;

# Config file initial setting
$conffilename 	= '/home/user/contiki/examples/basic-rekeying/conf/one2one-conf';
$confext 	= '.nocfg';

# C code for each node initial setting
$nodefilename 	= '/home/user/contiki/examples/basic-rekeying/one2one-node-';
@nodedata 	= ("1-1-1", "2-2-1", "3-2-2", "4-3-1", "5-1-2", "6-3-2", "7-3-3", "8-4-1", "9-5-1", "10-4-2", "11-4-3", "12-5-2", "13-2-3", "14-3-4", "15-3-5");
$nodeext 	= '.c';

for my $file (@files) {
  if($k < 16) {
    $i = 0;
    $j = 0;
    $conffullname 	= $conffilename.$k.$confext;
    $nodefullname 	= $nodefilename.$nodedata[$k-1].$nodeext;
    copy($file, $conffullname) or die "Copy failed: $!";

    open CONF, $conffullname or die "one2one-conf$k.ncfg: Open failed: $!";
    open TEMP, "temp_one2one_node.c" or die "temp_one2one_node.c: Open failed: $!";
    open MOTE, ">$nodefullname";

    while (<CONF>) {
      (/\(new\) groupKey \(hex\): (.*)/)	&& do {$groupKey = $1};
      (/nodeKey: (.*)/) 			&& do {$nodeKey = $1};
      (/nodeID: (.*)/) 				&& do {$nodeID = $1};
    }

    $groupKey 	= '0x'.join(', 0x', unpack("(A2)*", $groupKey));
    $nodeKey 	= '0x'.join(', 0x', unpack("(A2)*", $nodeKey));

    while (<TEMP>) {
      (/static uint8_t groupKey\[KEYSIZE\] = \{(.*)\}/) && do {s/$1/$groupKey/};
      (/static uint8_t nodeKey\[KEYSIZE\] = \{(.*)\}/) && do {s/$1/$nodeKey/};
      (/static uint32_t nodeID = (.*);/) && do {s/$1/$nodeID/};
      (/static uint32_t subgID = (.*);/) && do {s/$1/$subgID/};

      print MOTE
    }
  
    close CONF;
    close TEMP;
    close MOTE;
    $k++;
  }
}

