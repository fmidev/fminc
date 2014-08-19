%define LIBNAME fminc
Summary: fminc library
Name: lib%{LIBNAME}
Version: 14.8.11
Release: 1.el6.fmi
License: FMI
Group: Development/Tools
URL: http://www.fmi.fi
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
Provides: %{LIBNAME}
BuildRequires: netcdf-devel >= 4.1.1

%description
FMI netcdf library

%package devel
Summary: development package
Group: Development/Tools

%description devel
Headers and static libraries for fminc

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n "%{LIBNAME}" 

%build
make %{_smp_mflags} 

%install
%makeinstall

%post
umask 007
/sbin/ldconfig > /dev/null 2>&1

%postun
umask 007
/sbin/ldconfig > /dev/null 2>&1

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0644)
%{_libdir}/lib%{LIBNAME}.so.*

%files devel
%defattr(-,root,root,0644)
%{_libdir}/lib%{LIBNAME}.a
%{_includedir}/*.h

%changelog
* Mon Aug 11 2014 Andreas Tack <andreas.tack@fmi.fi> - 14.8.11-1.fmi
- Additional fixes for z-dimension handling
* Mon Aug  4 2014 Mikko Partio <mikko.partio@fmi.fi> - 14.8.4-1.fmi
- Fixes on z-dimension handling (UKMO data)
* Tue Oct  8 2013 Mikko Partio <mikko.partio@fmi.fi> - 13.10.8-1.el6.fmi
- Separating devel-package
* Tue Oct  2 2013 Mikko Partio <mikko.partio@fmi.fi> - 13.10.2-1.el6.fmi
- Initial build