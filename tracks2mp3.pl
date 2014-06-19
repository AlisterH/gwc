use strict ;
use warnings ;

open(my $fh, "<tracks.dat") ;

sub filename_encode($)
{
    my $in = shift ;
    $in =~ s/\'//g ;
    $in =~ s/\"//g ;
    $in =~ s/\,//g ;
    $in =~ s/ /_/g ;
    return $in ;
}

sub cmd_encode($)
{
    my $in = shift ;
    #$in =~ s/\'/\\'/g ;
    return "\"$in\"" ;
}

my $skip=0 ;

while(<$fh>) {
    chomp ;
    my ($file,$artist,$album,$title) = split(',') ;
    print "File:$file\nArtist:$artist\nTitle:$album: $title\n" ;
    my $ftitle = filename_encode($title) ;
    my $ctitle = cmd_encode($title) ;
    my $cartist =  cmd_encode($artist) ;
    my $calbum = cmd_encode($album) ;
    my $cmd = "lame -h -V 5 --tt $ctitle --ta $cartist --tl $calbum $file ${ftitle}.mp3" ;
    print "$cmd\n" ;
    system "$cmd\n" if $skip > 0 ;
    $skip++ ;
}

__END__
track02.cdda.wav,Michael Johnson,This Night Won't Last Forever
track04.cdda.wav,Michael Johnson,Bluer Than Blue
track06.cdda.wav,Michael Johnson,That's That
track10.cdda.wav,Karla Bonoff,All My Life
track12.cdda.wav,Karla Bonoff,Someone To Lay Down Beside Me
track14.cdda.wav,Karla Bonoff,Personally
track16.cdda.wav,Karla Bonoff,Wild Heart of The Young
track18.cdda.wav,Karla Bonoff,Home
