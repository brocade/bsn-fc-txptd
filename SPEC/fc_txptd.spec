%define _sysdir usr/lib

Name:	fc_txptd	
Version:	0.1
Release:	1%{?dist}
Summary:	Fibrechannel transport daemon

License:	GPLv2+
URL:		https://github.com/brocade/bsn-fc-txptd/
Source:		https://github.com/brocade/bsn-fc-txptd/blob/master/%{name}-%{version}.tar.gz

BuildRequires:	gcc
BuildRequires:	device-mapper-devel
BuildRequires:	libaio-devel
BuildRequires:	libudev-devel
BuildRequires:	readline-devel
BuildRequires:	udev
Requires:	device-mapper >= 1.2.78


%description
The purpose of this daemon is to add FC network intelligence in host and
host intelligence in FC network. This daemon would interoperate with
Brocade FC fabric in order to improve the response time of the MPIO failovers.
In future, it can also collect the congestion related details and perform
workload analysis, and provide QOS at application level by interoperating with
application performance profiling software.

%prep
%setup -q

%build
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}

%files
%doc README
/sbin/fctxpd
%dir /%{_sysdir}/systemd/system
/%{_sysdir}/systemd/system/fctxpd.service

%changelog
*Fri Jul 26 2019  Muneendra <muneendra.kumar@broadcom.com> 0.1-1
-Initial package for fedora
