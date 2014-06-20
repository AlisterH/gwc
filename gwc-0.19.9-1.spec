#
# SPEC file for Gnome Wave Cleaner
#
%define gwc_version 0.19
%define gwc_subversion 9

Name: gwc
Summary: Gnome Wave Cleaner -- audio restoration application
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
PreReq: fftw libsndfile1 db1 gnome-libs
BuildPreReq: fftw-devel libsndfile1-devel db1-devel gnome-libs-devel

%description
Gnome Wave Cleaner (GWC), is a tool for cleaning
up noisey audio files, in preparation for burning
to CD's.  The typical application is to record
the audio from vinyl LP's, 45's, 78's, etc to a
hard disk as a 16bit,stereo,44.1khz wave formated file,
and then use GWC to apply denoising and declicking
algorithms.

%prep
rm -rf $RPM_BUILD_ROOT
%setup -n %{name}-%{gwc_version}-%{gwc_subversion}

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
%doc gwc_help.html
/usr/bin/gwc
