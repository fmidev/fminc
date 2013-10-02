%define LIBNAME fminc
Summary: fminc library
Name: %{LIBNAME}
Version: 13.10.2
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
%{_libdir}/lib%{LIBNAME}.a

%changelog
* Tue Oct  2 2013 Mikko Partio <mikko.partio@fmi.fi> - 13.10.2-1.el6.fmi
- Initial build
