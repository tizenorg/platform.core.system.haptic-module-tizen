#sbs-git:slp/pkgs/d/devman devman 0.1.6 5bf2e95e0bb15c43ff928f7375e1978b0accb0f8
Name:       haptic-module-tizen
Summary:    Haptic Module library
Version:    0.1.0
Release:    9
Group:      System/Libraries
License:    APLv2
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(haptic-plugin)
BuildRequires: pkgconfig(device-node)

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description

%prep
%setup -q

%build
%if 0%{?simulator}
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSIMULATOR=yes
%else
%ifarch %{ix86}
CFLAGS=`echo %{optflags} |sed 's/\-fexceptions//g'`
%endif
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSIMULATOR=no
%endif
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libhaptic-module.so
