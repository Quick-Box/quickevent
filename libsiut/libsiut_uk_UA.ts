<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="uk_UA">
<context>
    <name>SICard</name>
    <message>
        <location filename="src/sicard.cpp" line="41"/>
        <source>stationNumber: %1</source>
        <translation>номерСтанції: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="42"/>
        <source>cardNumber: %1</source>
        <translation>номерКартки: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="46"/>
        <source>check: %1</source>
        <translation>перевірка: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="47"/>
        <source>start: %1</source>
        <translation>початок: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="48"/>
        <source>finish: %1</source>
        <translation>закінчення: %1</translation>
    </message>
</context>
<context>
    <name>SiStationConfig</name>
    <message>
        <location filename="src/device/sitask.cpp" line="95"/>
        <source>Station number: {{StationNumber}}
Extended mode: {{ExtendedMode}}
Auto send: {{AutoSend}}
Handshake: {{HandShake}}
Password access: {{PasswordAccess}}
Read out after punch: {{ReadOutAfterPunch}}
</source>
        <translation>Номер станції: {{StationNumber}}
Розширений режим: {{ExtendedMode}}
Авто надсилання: {{AutoSend}}
Рукопотискання: {{HandShake}}
Доступ з паролем: {{PasswordAccess}}
Зчитати після проколу: {{ReadOutAfterPunch}}
</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>True</source>
        <translation>Так</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>False</source>
        <translation>Ні</translation>
    </message>
</context>
<context>
    <name>siut::CommPort</name>
    <message>
        <location filename="src/device/commport.cpp" line="41"/>
        <source>Available ports: %1</source>
        <translation>Доступні порти: %1</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="49"/>
        <source>Connecting to %1 - baudrate: %2, data bits: %3, parity: %4, stop bits: %5</source>
        <translation>Під’єднання до %1 - швидкість передачі даних: %2, біти даних: %3, парність: %4, стоп-біти: %5</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="59"/>
        <source>%1 connected OK</source>
        <translation>%1 під’єднано ВДАЛО</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="63"/>
        <source>%1 connect ERROR: %2</source>
        <translation>%1 ПОМИЛКА під’єднання: %2</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="73"/>
        <source>%1 closed</source>
        <translation>%1 закрито</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="90"/>
        <source>possible solution:
Wait at least 10 seconds and then try again.</source>
        <translation>можливе вирішення:
Зачекати щонайменше 10 секунд і тоді спробувате знову.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="99"/>
        <source>There are no ports available.</source>
        <translation>Немає доступних портів.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="102"/>
        <source>Selected port %1 is not available.
List of accessible ports:

</source>
        <translation type="unfinished">Вибраний порт %1 не придатний.
Список доступних портів:

</translation>
    </message>
</context>
<context>
    <name>siut::DeviceDriver</name>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="123"/>
        <source>Garbage received, stripping %1 characters from beginning of buffer</source>
        <translation>Отримано Garbage, видалення %1 символів від початку буфера</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="140"/>
        <source>NAK received</source>
        <translation>отримано НАК</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="146"/>
        <source>Legacy protocol is not supported, switch station to extended one.</source>
        <translation>Старий протокол не підтримується, перемкніть станцію на розширений.</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="153"/>
        <source>Valid message shall end with ETX or NAK, throwing data away</source>
        <translation>Коректне повідомлення закінчується ETX або NAK, дані відкинуто</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="169"/>
        <source>SIDeviceDriver::sendCommand() - ERROR Sending of EXT commands only is supported for sending.</source>
        <translation>SIDeviceDriver::sendCommand() - ПОМИЛКА: надсилання EXT команд підтримується тільки для надсилань</translation>
    </message>
</context>
<context>
    <name>siut::SiTask</name>
    <message>
        <location filename="src/device/sitask.cpp" line="24"/>
        <source>SiCommand timeout after %1 sec.</source>
        <translation>Не отримано SiCommand через %1 сек.</translation>
    </message>
</context>
</TS>
