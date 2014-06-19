use strict ;
use warnings ;

use constant C => 340.29 ; # velocity of sound, meters/sec ;
use constant M_PI => 3.141592654 ;

sub mic_response($$)
{
    my ($f,$dist) = @_ ;
    my $k = 2.*M_PI*$f/C ;

    return sqrt(1+($k*$dist)**2) / ($k*$dist) ;
}

sub db2w($)
{
    return 10**($_[0]/10) ;
}

sub mic_response_c($)
{
    my ($f) = @_ ;
    my $db = 0.0 ;
    if($f < 4000) {
	$db =  0.0 ;
    } elsif($f < 7000) {
	$db =  ($f-4000)/7000*2 ;
    } elsif($f < 20000) {
	$db =  2 - (($f-7000)/13000)*12 ;
    } else {
	$db =  -10 ;
    }

    return($db, db2w($db)) ;
}

my $dist = 12.0 ;

for(my $f = 1000 ; $f < 21000 ; $f += 1000) {
    my $r = mic_response($f,$dist) ;
    my $db ;
    ($db, $r) = mic_response_c($f) ;
    print "$f $db $r\n" ;
}
