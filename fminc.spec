%define LIBNAME fminc
Summary: fminc library
Name: lib%{LIBNAME}
Version: 23.10.19
Release: 1%{dist}.fmi
License: FMI
Group: Development/Tools
URL: http://www.fmi.fi
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot-%(%{__id_u} -n)
Provides: %{LIBNAME}
BuildRequires: netcdf-devel >= 4.7.0
BuildRequires: netcdf-cxx-devel >= 4.2
BuildRequires: boost169-devel
Requires: boost169-filesystem
Requires: netcdf-devel >= 4.7.0
Requires: netcdf-cxx-devel >= 4.2

%description
FMI netcdf library

%package devel
Summary: development package
Group: Development/Tools
Requires: netcdf-cxx-devel

%description devel
Headers and static libraries for fminc
Requires: fminc

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
%{_libdir}/lib%{LIBNAME}.so

%files devel
%defattr(-,root,root,0644)
%{_libdir}/lib%{LIBNAME}.a
%{_includedir}/*.h

%changelog
* Thu Oct 19 2023 Mikko Partio <mikko.partio@fmi.fi> - 22.10.19-1.fmi
- spec file dependency update
* Mon Oct 12 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.10.12-2.fmi
- Fix memory leak(s)
* Mon Oct 12 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.10.12-1.fmi
- Reduce log spam
* Wed Oct  7 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.10.7-1.fmi
- Better handling of attribute values
* Thu Oct  1 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.10.1-2.fmi
- ... and also depthu, depthv etc
* Thu Oct  1 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.10.1-1.fmi
- Support nemo dimension deptht
* Thu Sep 24 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.9.24-1.fmi
- A bit more refactoring
* Tue Sep 22 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.9.22-1.fmi
- Refactoring
* Mon Apr 27 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.4.27-1.fmi
- Check standard name for longitude and latitude vars
* Mon Apr 20 2020 Mikko Partio <mikko.partio@fmi.fi> - 20.4.20-1.fmi
- boost 1.69
* Fri Aug 24 2018 Mikko Partio <mikko.partio@fmi.fi> - 18.8.24-1.fmi
- Bugfixes
* Wed Aug 22 2018 Mikko Partio <mikko.partio@fmi.fi> - 18.8.22-1.fmi
- Support for ensemble members
* Thu May  3 2018 Mikko Partio <mikko.partio@fmi.fi> - 18.5.3-1.fmi
- Support lcc projection
* Tue Apr 10 2018 Mikko Partio <mikko.partio@fmi.fi> - 18.4.10-1.fmi
- New boost
* Tue Mar 13 2018 Mikko Partio <mikko.partio@fmi.fi> - 18.3.13-1.fmi
- Change calculation of grid resolution
* Wed Aug 30 2017 Mikko Partio <mikko.partio@fmi.fi> - 17.8.30-1.fmi
- New boost
* Mon Feb 27 2017 Mikko Partio <mikko.partio@fmi.fi> - 17.2.27-1.fmi
- New Type[X,Y,T,Z]() functions
* Thu Feb 23 2017 Mikko Partio <mikko.partio@fmi.fi> - 17.2.23-1.fmi
- Function to get true latitude for polar stereographic projections
* Tue Dec 15 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.12.15-1.fmi
- Copernicus data support
* Wed Dec  2 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.12.2-1.fmi
- Add functions X1() and Y1()
* Mon Nov 16 2015 Andreas Tack  <andreas.tack@fmi.fi> - 15.11.16-1.fmi
- Fixes in HasDimension function
* Thu Nov  5 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.11.5-1.fmi
- Fixes in precisions of coordinates
* Thu Oct  1 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.10.1-1.fmi
- Add latitude and longitude varibables to output file if projection is stereographic
* Wed Sep 30 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.9.30-1.fmi
- Write projection variable to ouput file if it exists
* Thu Sep 17 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.9.17-1.fmi
- Bugfixes to follow conventions to the letter
* Tue Sep 15 2015 Mikko Partio <mikko.partio@fmi.fi> - 15.9.15-1.fmi
- Reworked library with support to different data types inside file
* Thu Sep  4 2014 Mikko Partio <mikko.partio@fmi.fi> - 14.9.4-1.fmi
- Add function CoordinatesInRowMajorOrder()
* Tue Sep  2 2014 Mikko Partio <mikko.partio@fmi.fi> - 14.9.2-1.fmi
- WriteSlice() uses same dimension ordering as source data
* Tue Aug 19 2014 Mikko Partio <mikko.partio@fmi.fi> - 14.8.19-1.fmi
- Adding height/0 as default level if no level is defined
* Mon Aug 11 2014 Andreas Tack <andreas.tack@fmi.fi> - 14.8.11-1.fmi
- Additional fixes for z-dimension handling
* Mon Aug  4 2014 Mikko Partio <mikko.partio@fmi.fi> - 14.8.4-1.fmi
- Fixes on z-dimension handling (UKMO data)
* Tue Oct  8 2013 Mikko Partio <mikko.partio@fmi.fi> - 13.10.8-1.el6.fmi
- Separating devel-package
* Tue Oct  2 2013 Mikko Partio <mikko.partio@fmi.fi> - 13.10.2-1.el6.fmi
- Initial build
