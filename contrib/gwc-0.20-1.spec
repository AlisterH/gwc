#
# SPEC file for Gnome Wave Cleaner
#
%define gwc_version 0.22
%define gwc_subversion 02

Name: gtk-wave-cleaner
Summary: Gtk Wave Cleaner -- audio restoration application
Version: %{gwc_version}.%{gwc_subversion}
Release: 1
License: GPL
Group: Applications/Multimedia
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Source: http://prdownloads.sourceforge.net/gwc/%{name}-%{gwc_version}-%{gwc_subversion}.tgz
URL: http://gwc.sourceforge.net/
Distribution: Redhat
Vendor: Redhawk.org
Packager: Redhawk.org
PreReq: fftw libsndfile1 gtk2-libs
BuildPreReq: fftw-devel libsndfile1-devel gtk2-libs-devel

%description
Gtk (formerly GNOME) Wave Cleaner (GWC) is a tool for cleaning up noisy audio files in
preparation for burning to CDs. The typical application is to record
the audio from vinyl LPs, 45s, 78s, etc, to a hard disk as a 16-bit,
stereo, 44.1khz wave formatted file and then use GWC to apply denoising
and declicking algorithms.

%prep
rm -rf $RPM_BUILD_ROOT
%setup -n %{name}-%{gwc_version}

%build
./configure --prefix=%{_prefix} --bindir=%{_bindir} --datadir=%{_datadir}
make

%install
make DESTDIR=$RPM_BUILD_ROOT/usr install 

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc README COPYING Changelog
%doc gtk-wave-cleaner.html gtkrc-example.txt
/usr/bin/gwc
