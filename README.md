<!-- PROJECT SHIELDS -->
[![Build status][build-shield]][build-url]
[![Translation status][weblate-shield]][weblate-url]
[![Stargazers][stars-shield]][stars-url]
[![Contributors][contributors-shield]][contributors-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]

# QuickEvent

QuickEvent is a set of applications for organizing Orienteering Events.

Single, multi-day events and relays can be organized. The program is stable for general use, however, keep in mind it is still under development and bugs might occur. Tester feedback is greatly appreciated, feel free to organize your training or events using the QuickEvent application and tell us your experience in the [issue section](https://github.com/Quick-Event/quickbox/issues). Releases are available on the [release page](https://github.com/Quick-Event/quickbox/releases). The goal is to create a free full-featured software set for organizing orienteering events.

The QuickBox is built upon the Qt framework, targeted for Linux and Windows and released under the GPL-2.0 License. The project is actively maintained by a collective of Czech orienteering runners in their free time.

### Components
* QuickEvent - event organization
* QuickShow - show start lists and results via application
* QuickHttpd - show start lists and results via webserver

### Features
* competitors management
* SI card readout
* receipts print
* course import (OCAD TXT | [IOF XML](https://github.com/international-orienteering-federation/datastandard-v3))
* start lists creation with classes layout
* results print and export (PDF | [IOF XML](https://github.com/international-orienteering-federation/datastandard-v3))
* export results for Liveresultat and Emma client
* [Weblate localization](https://hosted.weblate.org/projects/quickbox/) - thanks to Michal Cihar and https://weblate.org/ for free localization hosting
* integration with [ORIS](https://oris.ceskyorientak.cz/) - Czech orienteering federation information system
  * import events
  * sync entries
  * import clubs and registrations

### Documentation
For now, most of the [documentation](https://github.com/Quick-Event/quickbox/wiki) is in Czech.
* [Čipy, mezičasy a trocha základní filozofie programu](https://github.com/Quick-Event/quickbox/wiki/%C4%8Cipy%2C-mezi%C4%8Dasy-a-trocha-z%C3%A1kladn%C3%AD-filozofie-programu)
* [Tipy a triky](https://github.com/Quick-Event/quickbox/wiki/Tipy-a-triky)
* [Postup při pořádání závodů](https://github.com/Quick-Event/quickbox/wiki/Postup-p%C5%99i-po%C5%99%C3%A1d%C3%A1n%C3%AD-z%C3%A1vod%C5%AF)
  * [Zlínská uživatelská příručka](https://docs.google.com/document/d/1W8cPFhdmi7qP76Qv8TkzROivUucRSOzlqyeKix4pB7U/edit)
* [Online mezičasy / Liveresultat / Emma](https://github.com/Quick-Event/quickbox/wiki/Online-mezi%C4%8Dasy---Liveresultat---Emma)
* [Instalace tiskárny a nastavení tisku mezičasů](https://github.com/Quick-Event/quickbox/wiki/Instalace-tisk%C3%A1rny-a-nastaven%C3%AD-tisku-mezi%C4%8Das%C5%AF)
* [Často kladené otázky, FAQ](https://github.com/Quick-Event/quickbox/wiki/%C4%8Casto-kladen%C3%A9-ot%C3%A1zky%2C-FAQ)

---
## QuickEvent installation
#### Windows
1. download the installation package (*.exe) from https://github.com/Quick-Event/quickbox/releases
2. follow the installation wizard (keeping the default installation path is recommended)

#### Linux
1. download universal AppImage from https://github.com/Quick-Event/quickbox/releases
2. make it executable
```sh
chmod +x quickevent-*-x86_64.AppImage 
```
3. add yourself to `dialout` group (required by SI reader unit)
```sh
sudo usermod -aG dialout $USER
```
4. add yourself to `lp` group (required by character printer)
```sh 
sudo usermod -aG lp $USER 
```

### SQLite vs PostgreSQL
If you do not need to connect to the database simultaneously with multiple computers, you do not have to use a bulky database and opt for the SQLite instead. Event data will be stored as a single file with a _.qbe_ extension. When starting QuickEvent and `Connect to database` dialog will pop up, select `Single file` and choose a working directory where event files will be stored.

You can later connect to a PostgreSQL database and import the event stored as _*.qbe_ file created earlier.

### Setting up for PostgreSQL
#### Windows
* download from https://www.enterprisedb.com/downloads/postgres-postgresql-downloads
* tested version is PostreSQL 12.7.x
* install (keep the default settings, enter superuser password)

#### Debian
```sh
sudo apt install postgresql
```
#### RHEL/CentOS/Fedora
```sh
sudo yum install postgresql
```
### Create a Postgres database
QuickEvent needs a database named _quickevent_. Open `psql` shell: `[PostgreSQL home]\bin\psql -U postgres`, enter superuser password and type:
```sql
CREATE USER quickevent_admin PASSWORD 'good password';
CREATE DATABASE quickevent OWNER quickevent_admin;
```

#### Enable database access from network
If you need access to the database from other computers, you need to enable access from other IP addresses. By default, only localhost connections are enabled.

Add following line to your `[PostgreSQL home]\data\pg_hba.conf` file:
```
hostnossl    all          all            0.0.0.0/0  md5
```

Ensure following line is present in your `[PostgreSQL home]\data\postgresql.conf` (though this is default):
```
listen_addresses = '*'
```
Restart the PostgreSQL service.

---
<!-- BUILD -->
## Build QE3 (Qt6 version)
```
git clone https://github.com/Quick-Event/quickbox.git
cd quickbox
git sw qe3
git submodule update --init --recursive
mkdir build
cd build
cmake .. -DCMAKE_GENERATOR:STRING=Ninja -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_PREFIX_PATH:PATH=/home/fanda/app/qt5/6.6.2/gcc_64
cmake --build . 
cmake --install . --prefix /path/to/install/qe3
```
## Run QE3 from QTC (Qt6 version)  
When QE3 is run from QTC (QT Creator), some symlinks must be created to load QML modules.
1. build QE3 manually and run `cmake --install ...` as described in previous chapter.
2. Open `CMakeLists.txt` as project in QTC
3. Configure project
4. Build project
5. Create symlinks to manual istallation, for example `/tmp/qe3`
```sh
cd <qtc-build-dir>/quickevent/app/quickevent
ln -s /tmp/qe3/bin/qml qml
ln -s /tmp/qe3/bin/reports reports
ln -s /tmp/qe3/bin/translations translations
```

---
<!-- CONTRIBUTING -->
## Contributing

Contributions are what makes the open source community such an amazing place to learn, inspire and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

Please see the [Code style](https://github.com/Quick-Event/quickbox/wiki/Code-style). Before opening a merge request, open related issue to discuss the upcoming changes first.

---
<!-- LICENSE -->
## License

Distributed under the GNU GPL v2.0 License. See `LICENSE` for more information.

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[build-shield]: https://github.com/Quick-Event/quickbox/actions/workflows/c-cpp.yml/badge.svg?branch=master
[build-url]: https://github.com/Quick-Event/quickbox/actions?query=branch%3Amaster
[weblate-shield]: https://hosted.weblate.org/widgets/quickbox/-/svg-badge.svg
[weblate-url]: https://hosted.weblate.org/engage/quickbox/
[contributors-shield]: https://img.shields.io/github/contributors/Quick-Event/quickbox
[contributors-url]: https://github.com/Quick-Event/quickbox/graphs/contributors
[stars-shield]: https://img.shields.io/github/stars/Quick-Event/quickbox
[stars-url]: https://github.com/Quick-Event/quickbox/stargazers
[issues-shield]: https://img.shields.io/github/issues/Quick-Event/quickbox
[issues-url]: https://github.com/Quick-Event/quickbox/issues
[license-shield]: https://img.shields.io/github/license/Quick-Event/quickbox
[license-url]: https://github.com/Quick-Event/quickbox/blob/master/LICENSE
