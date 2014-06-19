use strict ;
use warnings ;
use constant M_PI=>3.14159265358979323846 ;


sub blackman($$)
{
    my ($k, $N) = @_ ;
    my $p = $k/($N-1.0) ;
    return 0.42-0.5*cos(2.0*M_PI*$p) + 0.08*cos(4.0*M_PI*$p) ;
}

sub hanning($$)
{
    my ($k, $N) = @_ ;
    my $p = $k/($N-1.0) ;
    return 0.5 - 0.5 * cos(2.0*M_PI*$p) ;
}

my @w ;
my $len = 16 ;
my @s ;

for(my $i = 0 ; $i < $len ; $i++) {
    $w[$i] = hanning($i,$len) ;
    print "i:$i w:$w[$i]\n" ;
}

for(my $i = 0 ; $i < $len ; $i++) {
    $s[$i] = $w[$i] ;
}

for(my $i = 0 ; $i < $len ; $i++) {
    $s[$i+4] += $w[$i] ;
    print "i:$i s:$s[$i]\n" ;
}
