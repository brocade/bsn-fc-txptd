
%global commit          c195e67ee3b55ba8fa5e9829369b8f13977fbbbf
%global shortcommit     %(c=%{commit}; echo ${c:0:7})
%global snapshotdate    20190813

Name:   fc_txptd
Version:        0.1
Release:        1.%{snapshotdate}git%{shortcommit}%{?dist}
Summary:        Fibrechannel transport daemon

License:        GPLv2+
URL:            https://github.com/brocade/bsn-fc-txptd/
Source:         %{url}/archive/%{commit}/%{name}-%{shortcommit}.tar.gz

BuildRequires:  gcc
BuildRequires:  device-mapper-devel
BuildRequires:  systemd-devel
BuildRequires:  libudev-devel
BuildRequires:  udev
BuildRequires: device-mapper-multipath
BuildRequires: systemd
BuildRequires: device-mapper-multipath-devel
Requires:       device-mapper >= 1.2.78


%description
The purpose of this daemon is to add FC network intelligence in host and
host intelligence in FC network. This daemon would inter-operate with
Brocade FC fabric in order to improve the response time of the MPIO failover.
In future, it can also collect the congestion related details and perform
workload analysis, and provide QOS at application level by inter-operating with
application performance profiling software.

%prep
%autosetup -n bsn-fc-txptd-%{commit}

%post
%systemd_post fctxpd.service

%preun
%systemd_preun fctxpd.service

%postun
%systemd_postun_with_restart fctxpd.service

%build
%make_build


%install
%make_install

%files
%doc README
%{_sbindir}/fctxpd
%{_unitdir}/fctxpd.service
%license LICENSES/GPL-2.0

%changelog
*Mon Aug 12 2019  Muneendra <muneendra.kumar@broadcom.com> 0.1-1.20190813gitc195e67
-No functional changes,just Licenses
-Spec file:Created LICENSES dir with the text of all used license
-Added the license header in the corresponding source files

*Fri Jul 26 2019  Muneendra <muneendra.kumar@broadcom.com> 0.1-1
-Initial package for fedora
