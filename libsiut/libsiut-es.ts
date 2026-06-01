<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es">
<context>
    <name>SICard</name>
    <message>
        <location filename="src/sicard.cpp" line="38"/>
        <source>stationNumber: %1</source>
        <translation>Número de base: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="39"/>
        <source>cardNumber: %1</source>
        <translation>Número de pinza: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="43"/>
        <source>check: %1</source>
        <translation>Comprobación: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="44"/>
        <source>start: %1</source>
        <translation>Salida: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="45"/>
        <source>finish: %1</source>
        <translation>Meta: %1</translation>
    </message>
    <message>
        <location filename="src/sicard.cpp" line="46"/>
        <source>batteryStatus: %1</source>
        <translation>estadoBatería: %1</translation>
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
        <translation>Número de estación: {{StationNumber}}
Protocolo extendido: {{ExtendedMode}}
Auto enviar: {{AutoSend}}
Handshake: {{HandShake}}
Contraseña: {{PasswordAccess}}
Lectura después de picar: {{ReadOutAfterPunch}}
</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>True</source>
        <translation>Verdadero</translation>
    </message>
    <message>
        <location filename="src/device/sitask.cpp" line="104"/>
        <location filename="src/device/sitask.cpp" line="105"/>
        <location filename="src/device/sitask.cpp" line="106"/>
        <location filename="src/device/sitask.cpp" line="107"/>
        <location filename="src/device/sitask.cpp" line="108"/>
        <source>False</source>
        <translation>Falso</translation>
    </message>
</context>
<context>
    <name>siut::CommPort</name>
    <message>
        <location filename="src/device/commport.cpp" line="41"/>
        <source>Available ports: %1</source>
        <translation>Puertos disponibles: %1</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="49"/>
        <source>Connecting to %1 - baudrate: %2, data bits: %3, parity: %4, stop bits: %5</source>
        <translation>Conectándose a %1 - ratio de baudios: %2, bits de datos: %3, paridad: %4, bit de parada: %5</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="59"/>
        <source>%1 connected OK</source>
        <translation>%1 se conectó correctamente</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="63"/>
        <source>%1 connect ERROR: %2</source>
        <translation>%1 error de conexión: %2</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="73"/>
        <source>%1 closed</source>
        <translation>%1 cerrado</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="90"/>
        <source>possible solution:
Wait at least 10 seconds and then try again.</source>
        <translation>Posible solución:
Espere al menos 10 segundos y vuelva a intentarlo.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="99"/>
        <source>There are no ports available.</source>
        <translation>No hay puerto disponibles.</translation>
    </message>
    <message>
        <location filename="src/device/commport.cpp" line="102"/>
        <source>Selected port %1 is not available.
List of accessible ports:

</source>
        <translation>El puerto seleccionado (%1) no se encuentra disponible.
Lista de puertos disponibles:

</translation>
    </message>
</context>
<context>
    <name>siut::DeviceDriver</name>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="123"/>
        <source>Garbage received, stripping %1 characters from beginning of buffer</source>
        <translation>Datos corruptos recibidos, eliminado %1 caracteres del principio del buffer</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="140"/>
        <source>NAK received</source>
        <translation>El dispositivo rechazó la solicitud</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="146"/>
        <source>Legacy protocol is not supported, switch station to extended one.</source>
        <translation>Protocolo legacy no soportado, cambie la base al protocolo extendido.</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="153"/>
        <source>Valid message shall end with ETX or NAK, throwing data away</source>
        <translation>Un mensaje válido debe terminar con ETX o NAK; de lo contrario, se descartan los datos</translation>
    </message>
    <message>
        <location filename="src/device/sidevicedriver.cpp" line="169"/>
        <source>SIDeviceDriver::sendCommand() - ERROR Sending of EXT commands only is supported for sending.</source>
        <translation>SIDeviceDriver::sendCommand() - ERROR - El envío solo admite comandos EXT.</translation>
    </message>
</context>
<context>
    <name>siut::SiTask</name>
    <message>
        <location filename="src/device/sitask.cpp" line="24"/>
        <source>SiCommand timeout after %1 sec.</source>
        <translation>Se agotó el tiempo de espera del comando SportIdent después de %1 s.</translation>
    </message>
</context>
</TS>
