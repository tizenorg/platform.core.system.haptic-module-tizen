#sbs-git:slp/pkgs/d/devman devman 0.1.6 5bf2e95e0bb15c43ff928f7375e1978b0accb0f8
Name:       haptic-module-tizen
Summary:    Haptic Module library
Version:    0.1.0
Release:    5
Group:      System/Libraries
License:    APLv2
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(haptic-plugin)
BuildRequires: pkgconfig(devman)

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description

%prep
%setup -q

%build
%if 0%{?simulator}
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DSIMULATOR=yes
%else
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

%changelog
* Tue Jan 08 2013 - Jae-young Hwang <j-zero.hwang@samsung.com>
- Unify code for ARM/x86
- Tag : haptic-module_tizen_0.1.0-5

* Mon Jan 07 2013 - Jae-young Hwang <j-zero.hwang@samsung.com>
- Change libhpatic-module.so file path
- Tag : haptic-module_tizen_0.1.0-4

* Thu Dec 21 2012 - Jae-young Hwang <j-zero.hwang@samsung.com>
- Modify structure of functions
- Tag : haptic-module_tizen_0.1.0-3

* Mon Nov 27 2012 - Jiyoung Yun <jy910.yun@samsung.com>
- support playing file and implement file structure
- revised tid reset in thread
- Tag : haptic-module-tizen_0.1.0-2

* Mon Nov 19 2012 - Jiyoung Yun <jy910.yun@samsung.com>
- fix the problem not working on emulator
- Tag : haptic-module-tizen_0.1.0-1

* Fri Nov 02 2012 - Jae-young Hwang <j-zero.hwang@samsung.com>
- Implement haptic-module-tizen
- Tag : haptic-module-tizen_0.1.0-0
