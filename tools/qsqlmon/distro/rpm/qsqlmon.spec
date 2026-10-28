#
# spec file for package qsqlmon
#
# Copyright (c) 2019 Frantisek Vacek <fanda.vacek@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# Please submit bugfixes or comments via
#   https://github.com/fvacek/quickbox/
# issues tracker.
#


Name:           qsqlmon
Version:        1.3.6
Release:        0
Summary:        Open Source QT Database administration tool
License:        GPL-2.0-or-later
Group:          Productivity/Databases/Tools
URL:            https://github.com/fvacek/quickbox/tree/master/tools/qsqlmon
Source:         qsqlmon_%{version}.tar.xz
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(Qt5Xml)
BuildRequires:  pkgconfig(Qt5Svg)
BuildRequires:  pkgconfig(Qt5Sql)
# BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5PrintSupport)

%if ! 0%{?suse_version}
BuildRequires:  desktop-file-utils
%endif

%if 0%{?suse_version}
BuildRequires:  update-desktop-files
%endif

%if !0%{?suse_version}
%define  qmake5  /usr/bin/qmake-qt5
%endif

%description
Open Source QT Database administration tool.

%prep
%setup -q

%build
%qmake5 "CONFIG+=release" "CONFIG+=force_debug_info"
#make %{?_smp_mflags} - do not use SMP for now, there can be problem with generated makefiles
make


%install
mkdir -p %{buildroot}/%{_bindir}
install -m755 bin/qsqlmon %{buildroot}/%{_bindir}
mkdir -p %{buildroot}/%{_datadir}/icons/hicolor/48x48/apps
install -m644 tools/qsqlmon/distro/icons/qsqlmon48x48.png %{buildroot}/%{_datadir}/icons/hicolor/48x48/apps
mkdir -p %{buildroot}/%{_datadir}/icons/hicolor/scalable/apps
install -m644 tools/qsqlmon/distro/icons/qsqlmon.svg %{buildroot}/%{_datadir}/icons/hicolor/scalable/apps

#desktop icon
%if 0%{?suse_version}
install -m755 -d %{buildroot}%{_datadir}/applications
install -m644 tools/qsqlmon/distro/deb/qsqlmon/usr/share/applications/qsqlmon.desktop %{buildroot}%{_datadir}/applications
%suse_update_desktop_file -r -i qsqlmon System Database
%endif

%if 0%{?fedora} || 0%{?rhel} || 0%{?centos}
desktop-file-install --dir=%{buildroot}%{_datadir}/applications tools/qsqlmon/distro/deb/qsqlmon/usr/share/applications/qsqlmon.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/qsqlmon.desktop
%endif

%files
%{_bindir}/qsqlmon
%{_datadir}/applications/qsqlmon.desktop
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/48x48
%dir %{_datadir}/icons/hicolor/48x48/apps
%{_datadir}/icons/hicolor/48x48/apps/qsqlmon48x48.png
%dir %{_datadir}/icons/hicolor/scalable
%dir %{_datadir}/icons/hicolor/scalable/apps
%{_datadir}/icons/hicolor/scalable/apps/qsqlmon.svg
%license LICENSE
%doc README.md

%changelog
