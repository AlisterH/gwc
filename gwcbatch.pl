#!/usr/bin/perl
# for usage instructions run `perldoc gwcbatch.pl` 

sub time2sec
{
	my @tm = split(/:/,shift);
	my $sec = 0;

	while (@tm)
	{
		$sec *= 60;
		$sec += shift @tm;
	}
	return $sec;
}

#
#------
# Main

my
(
	 $file
	,$archive
	,@keep
	,$sens
	,@noise
);

while (@ARGV)
{
	@keep = @noise = ();
	$file = shift;
	($archive = $file) =~ s/\.wav$/_orig$&/;
	push @keep, (shift, shift);
	$sens = shift;
	push @noise, (shift,shift);
	
	#convert time to seconds
	@keep = map {&time2sec($_)} @keep;
	@noise = map {&time2sec($_)} @noise;
	
	# back up the file first.
	print `cp $file $archive`;
	
	# Now declick and remove noise
	print `gtk-wave-cleaner $file batch declick $sens $keep[0] $keep[1]`;
	# Add extra declicking runs if you want them
	#print `gtk-wave-cleaner $file batch declick $sens $keep[0] $keep[1]`;
	print `gtk-wave-cleaner $file batch denoise $noise[0] $noise[1] $keep[0] $keep[1]`;

	# truncate after cleaning in case the noise sample is from the region to truncate
	# for some reason, it *sometimes* hangs when truncating
	# both front and back, so do back first, then front
	print `gtk-wave-cleaner $file batch truncate 0 $keep[1]`;
	print `gtk-wave-cleaner $file batch truncate $keep[0] $keep[1]`;
	
	# normalize last so it isn't affected by anything we are discarding
	print `gtk-wave-cleaner $file batch normalize`;
}

1;

# Documentation follows

=pod

=head1 NAME

gwcbatch.pl - quick & dirty helper script for gwc's quick & dirty batch mode

=head1 SYNOPSIS

Usage is as follows:

  gwcbatch.pl wavfile musicStart musicEnd declickSens noiseStart noiseEnd \
  [wavfile2 musicStart2 ... \] 
  [... \]
  [wavfileN musicStartN ...]

=over

=item B<wavfile>

sound file to process (wav format)

=item B<musicStart, musicEnd>

portion of the wav file to keep.  Time format is hh:mm:ss.xxx or just
ss.xxx if desired.

=item B<declickSens>

sensitivity to use for the declick process.

=item B<noiseStart, noiseEnd>

noise sample to use for the denoise process.  Time format is same as
for musicStart & musicEnd

=back

=head1 DESCRIPTION

There's no doubt about it.  Ripping vinyl + cleaning the sound is a
time-consuming process.  gwc has some facilities for batch processing,
and this perl script helps complete the automation.

I typically record 2-4 album sides, inspect each file with gwc,
then run this script to do the declicking & denoising while I sleep.
That can take tens of minutes to several hours depending on the
condition of the vinyl, length of recordings processed, and the power
of your CPU.

Here's the process:

=over

=item *

record the album side.  I do this via ecasound like this:

      ecasound -i /dev/dsp -ezf -ev -t 1800 -o sideX.wav

the "-t 1800" means it'll record for 30 minutes and then stop so
there's always something at the end to chop off, and usually something
at the beginning too.  But recording in this fashion means I don't
have to babysit the process.

=item *

Set up your denoise preferences

=item *

If you like to iterate for declicking, mark that checkbox in the
declick setup

=item *

load the wav file into gwc.  Write down the times (to as many decimal
places as you like) for musicStart, musicEnd, noiseStart, and noiseEnd

=item *

run this script as described above

=back

This script will do the following:

=over

=item *

copy the file for archival purposes

=item *

declick using declickSens for the sensitivity

=item *

denoise using the noise sample specified in noiseStart- noiseEnd

=item *

truncate the wav file, keeping only musicStart - musicEnd

=item *

normalize the audio; amplify as much as possible without
clipping.

=back

Feel free to modify this script for your own purposes.  I sometimes
add another declick after the first one just to catch the majority of
the straglers.

=head1 BUGS

It'd be even better if this thing could mark songs and create a
minimal but functional cdrdao toc file.  That'll require a bit more
coding in gwc.

=head1 AUTHOR

Bill Jetzer (created July 2003)

=cut
