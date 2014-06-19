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
and the use GWC to apply denoising and declicking
algorithms.

%prep
rm -rf $RPM_BUILD_ROOT
%setup -n %{name}-%{gwc_version}-%{gwc_subversion}

# Patch Makefile
cat <<END | patch
--- Makefile.in.orig	Wed Sep  3 06:27:43 2003
+++ Makefile.in	Sat Sep 20 19:22:31 2003
@@ -27,17 +27,17 @@
 	strip gwc
 	install -d \$(DESTDIR)/bin
 	install -d \$(DESTDIR)/share/doc/gwc
-	install -d \$(pixmapdir)
-	install -d \$(DATADIR)/\$(HELPDIR)/C
+	install -d \$(DESTDIR)/\$(pixmapdir)
+	install -d \$(DESTDIR)/\$(DATADIR)/\$(HELPDIR)/C
 	install gwc \$(DESTDIR)/bin
 	install README \$(DESTDIR)/share/doc/gwc
 	install INSTALL \$(DESTDIR)/share/doc/gwc
 	install COPYING \$(DESTDIR)/share/doc/gwc
 	install Changelog \$(DESTDIR)/share/doc/gwc
-	install doc/C/gwc_qs.html \$(DATADIR)/\$(HELPDIR)/C
-	install doc/C/gwc.html \$(DATADIR)/\$(HELPDIR)/C
-	install doc/C/topic.dat \$(DATADIR)/\$(HELPDIR)/C
-	install gwc-logo.png \$(pixmapdir)
+	install doc/C/gwc_qs.html \$(DESTDIR)/\$(DATADIR)/\$(HELPDIR)/C
+	install doc/C/gwc.html \$(DESTDIR)/\$(DATADIR)/\$(HELPDIR)/C
+	install doc/C/topic.dat \$(DESTDIR)/\$(DATADIR)/\$(HELPDIR)/C
+	install gwc-logo.png \$(DESTDIR)/\$(pixmapdir)
 
 meschach.a : meschach/meschach.a
 	cp meschach/meschach.a .
END

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
