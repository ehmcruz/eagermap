<?php

$nt = $argv[1];

if ($nt > 16)
	$max = 1 << 16;
else
	$max = 1 << $nt;

for ($i=$nt-1; $i>=0; $i--) {
	for ($j=0; $j<$nt; $j++) {
		$diff = $i - $j;
		if ($diff < 0)
			$diff *= -1;
		if ($i == $j)
			$comm = 0;
		elseif ($diff == 1)
			$comm = $max;
		else
			$comm = $max >> $diff;
		echo "$comm";
		if ($j<($nt-1))
			echo ",";
	}
#	if ($i>)
		echo "\n";
}

?>
