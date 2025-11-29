<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="cs_CZ">
<context>
    <name>SICard</name>
    <message>
        <location filename="src/sicard.cpp" line="41"/>
        <source>stationNumber: %1</source>
        <translation>číslo kontroly: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="42"/>
        <source>cardNumber: %1</source>
        <translation>číslo čipu: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="46"/>
        <source>check: %1</source>
        <translation>kontrola čipu: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="47"/>
        <source>start: %1</source>
        <translation>start: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="48"/>
        <source>finish: %1</source>
        <translation>cíl: %1</translation>
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
        <translation>Jednotka číslo: {{StationNumber}}
Extended mód: {{ExtendedMode}}
Auto send: {{AutoSend}}
Handshake: {{HandShake}}
Password access: {{PasswordAccess}}
Vyčítat po oražení: {{ReadOutAfterPunch}}
</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>True</source>
        <translation>Zapnuto</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>False</source>
        <translation>Vypnuto</translation>
    </message>
</context>
<context>
    <name>siut::CommPort</name>
    <message>
        <location filename="src/device/commport.cpp" line="41"/>
        <source>Available ports: %1</source>
        <translation>Dostupné porty: %1</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="49"/>
        <source>Connecting to %1 - baudrate: %2, data bits: %3, parity: %4, stop bits: %5</source>
        <translation>Připojování k %1 - baudrate: %2, datové bity: %3, parita: %4, stop bity: %5</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="59"/>
        <source>%1 connected OK</source>
        <translation>%1 připojeno OK</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="63"/>
        <source>%1 connect ERROR: %2</source>
        <translation>%1 chyba při připojování: %2</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="73"/>
        <source>%1 closed</source>
        <translation>%1 odpojeno</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="90"/>
        <source>possible solution:
Wait at least 10 seconds and then try again.</source>
        <translation>možné řešení:
Počkejte alespoň 10 sekund a potom to zkuste znovu.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="99"/>
        <source>There are no ports available.</source>
        <translation>Nejsou k dispozici žádné porty.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="102"/>
        <source>Selected port %1 is not available.
List of accessible ports:

</source>
        <translation>Vybraný port %1 není k dispozici.
Seznam dostupných portů:

</translation>
    </message>
</context>
<context>
    <name>siut::DeviceDriver</name>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="123"/>
        <source>Garbage received, stripping %1 characters from beginning of buffer</source>
        <translation>Přijata nevalidní data, odstraňuji %1 znaků ze začátku vyrovnávací paměti</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="140"/>
        <source>NAK received</source>
        <translation>Přijato NAK</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="146"/>
        <source>Legacy protocol is not supported, switch station to extended one.</source>
        <translation>Legacy protokol není podporován, přepněte jednotku na extended protokol.</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="153"/>
        <source>Valid message shall end with ETX or NAK, throwing data away</source>
        <translation>Validní zpráva musí končit s ETX nebo NAK, přeskakuji data</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="169"/>
        <source>SIDeviceDriver::sendCommand() - ERROR Sending of EXT commands only is supported for sending.</source>
        <translation>SIDeviceDriver::sendCommand() - CHYBA - je podporováno pouze odesílání EXT příkazů.</translation>
    </message>
</context>
<context>
    <name>siut::SiTask</name>
    <message>
        <location filename="src/device/sitask.cpp" line="24"/>
        <source>SiCommand timeout after %1 sec.</source>
        <translation>SiCommand timeout po %1 sek.</translation>
    </message>
</context>
</TS>
