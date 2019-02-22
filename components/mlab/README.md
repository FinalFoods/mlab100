# mlab

This component is a holding place for application agnostic Microbiota
Labs code which may need to be shared between different applications
or target platform images.

# Internal information

The following sections are holding places for useful internal
information about the Microbiota Labs implementation.

## BLEADV

A standard BLE advertisement packet is transmitted by the device when
it is waiting for a BLE-GATT connection to be established.

The advertisement packet is a limited size broadcast packet consisting
of binary data fields as defined by the Bluetooth [Generic Access
Profile](https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile).

Our devices can be identified by the **Manufacturer Specific Data**
tag (`0xFF`) containing the required manufacturer 16-bit UUID and our
own "arbitrary" manufacturer data. The Microbiota Labs devices that
are currently based on the ESP32-WROOM-32D (or ESP-WROOM-32) SOMs use
the Espressif manufacturer ID **`0x02E5`** (as defined by [Company
Identifiers](https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers))
since we are using the standard Espressif SOM.

For example, the raw BLEADV packet (hexadecimal) data: **`02 01 06 08
09 4D 4C 41 42 31 30 30 05 FF E5 02 4D 4C 02 0A 03 03 03 FF FF 05 12
06 00 10 00 00`** is the set of fields:

offset | len | tag | data                 | tag-name                                    | notes
:-----:|:---:|:---:|:---------------------|:--------------------------------------------|:-----
00     |  02 |  01 | 06                   | Flags                                       | BLE discoverable and BR/EDR not supported
03     |  08 |  09 | 4D 4C 41 42 31 30 30 | Complete Local Name                         | "MLAB100"
0C     |  05 |  FF | E5 02 4D 4C          | Manufacturer Specific Data                  | Espressif H/W manufacturer followed by Microbiota Labs ID "ML" (we may extend the binary data we advertise in the future)
12     |  02 |  0A | 03                   | Tx Power Level                              | +3dBm (can be used in conjunction with RSSI to calculate "pathloss" when detecting devices which are closer than others)
15     |  03 |  03 | FF FF                | Complete List of 16-bit Service Class UUIDs | **`0xFFFF`** used since not a Bluetooth standard service class
19     |  05 |  12 | 06 00 10 00          | Slave Connection Interval Range             | 7.5ms min, 20ms max
1F     |  00 |     |                      | undefined terminator                        | May **NOT** be present; depends on transmitter

**NOTE**: Since the advertisement has a limited size, if we want to
provide more identifying information in the **Manufacturer Specific
Data** field we can avoid providing the **Complete Local Name** and
rely on the application to detect Microbiota Labs devices without
having to provide a human-readable name. As always the BLE-GATT
service can provide more detail (longer strings), than we can encode
in the advertisement broadcast packet.

## BLE-GATT

As is the norm for GATT when we refer to the Client we mean the UI
device (Android, iOS, etc), and the Server being the device (ESP32)
firmware.

A custom service class is implemented (**`0xFFFF`**) since the BluFi
support is not a defined Bluetooth organisation standard.

**Aside**: There are many Android BLE scanner tools available in the
Play store. For example (just one of many): [BLE
Scanner](https://play.google.com/store/apps/details?id=com.macdom.ble.blescanner&hl=en_GB)
can be used show the standard GATT features provided by the current
MLAB100 (BluFi) firmware:

```
Generic Attribute	0x1801	Primary Service
- Service Changed		00002A05-0000-1000-8000-00805F9B34FB	INDICATE		Descriptor UUID 0x2902

Generic Access		0x1800	Primary Service
- Device Name			00002A00-0000-1000-8000-00805F9B34FB	READ
- Appearance			00002A01-0000-1000-8000-00805F9B34FB	READ
- Central Address Resolution	00002AA6-0000-1000-8000-00805F9B34FB	READ

Custom Service		0000FFFF-0000-1000-8000-00805F9B34FB
- Custom Characteristic		0000FF01-0000-1000-8000-00805F9B34FB	WRITE (WRITE REQUEST)
- Custom Characteristic		0000FF02-0000-1000-8000-00805F9B34FB	READ, NOTIFY		Descriptor UUID 0x2902
```

So the **Generic Attribute** and **Generic Access** are as defined by
the GATT standard. The **Custom Service** is a simple custom interface
to communicate between the Client (running on the Android/iOS device)
and the Server (ESP32).

So in summary (as per the definitions in [GATT
Services](https://www.bluetooth.com/specifications/gatt/services)) our
device provides:

Number | Name              | [GATT Characteristics](https://www.bluetooth.com/specifications/gatt/characteristics)
:-----:|:------------------|:-------------------------------------------------------------------------------------
0x1801 | Generic Attribute | **0x2A05** == Service Changed
0x1800 | Generic Access    | **0x2A00** == Device Name, **0x2A01** == Appearance and **0x2AA6** == Central Address Resolution
0xFFFF | undefined         | **0xFF01** == Client-to-ESP32 packets and **0xFF02** == ESP32-to-Client packets

So, our base GATT functionality from the Client perspective is:
- BluFi service UUID **0xFFFF**
- - Client->ESP32 **0xFF01** writable
- - ESP32->Client **0xFF02** readable and callable

#### Bluetooth Packet Capture

When working with a physical Android device for testing it is possible
to enable the **Developer options** **`Enable Bluetooth HCI snoop
log`** option and have the device bluetooth packet activity logged for
post-analysis. e.g. the file can be read on the development host
using:

```
adb pull /sdcard/btsnoop_hci.log
```

and the file viewed with the Wireshark packet analysis tool. It is
worth filtering on the wireshark view on the Bluetooth address of the
actual ESP32 device being interacted with to cut down on the bluetooth
noise. e.g. Filter for advertisements:
```
  bthci_evt.bd_addr == 24:0a:c4:1D:46:12
```
or for actual GATT data packets:
```
  (bluetooth.src == 24:0a:c4:1d:46:12) || (bluetooth.dst == 24:0a:c4:1d:46:12)
```

### react

Looking at the [react BLE manager
README.md](https://github.com/innoveit/react-native-ble-manager)
documentation shows that to write from the Client to the Server on
UUID **`0xFF01`** should just be a case of calling:

```
write(peripheralId,serviceUUID=0xFFFF,characteristicUUID=0xFF01,data=<byte-array>,maxByteSize)
```

It is unclear (since I have not yet used that react package) if:

```
startNotification(peripheralId,serviceUUID=0xFFFF,characteristicUUID=0xFF02)
```

is enough for the Client to receive asynchronous data from UUID
**`0xFF02`**, or whether it needs to be polled using:

```
read(peripheralId,serviceUUID=0xFFFF,characteristicUUID=0xFF02)
```

Both the write (Client-to-Server) and read/notification
(Server-to-Client) byte-array data is a simple binary packet format as
detailed below.

### BluFi

As shown above a simple interface using a characteristic for
Client-to-ESP32 data and another for ESP32-to-Client messages
(packets) is used.

A simple 2-bit type encoding is used (currently **`CTRL=0x0`** and
**`DATA=0x1`** defined), with the type being used in conjunction with
a 6-bit subtype encoded (giving a range for each type of
**`0x00`**-to-**`0x3F`** (0..63)).

There is support for fragmented packets, for when either end needs to
send more data than will fit in a short BLE packet. An example of
normal use of the fragmented packets is when receiving a list of APs
seen by the ESP32 scan.

Currently the defined packet **subtype** encodings are:

type | subtype | name                                                                                | notes
----:|:-------:|:------------------------------------------------------------------------------------|:-----
CTRL | 0x00    | BLUFI_TYPE_CTRL_SUBTYPE_ACK                                                         |
CTRL | 0x01    | BLUFI_TYPE_CTRL_SUBTYPE_SET_SEC_MODE                                                |
CTRL | 0x02    | [BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE](#blufi_type_ctrl_subtype_set_wifi_opmode) | configure WiFi mode : 1-byte (wifi_mode_t) value
CTRL | 0x03    | [BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP](#blufi_type_ctrl_subtype_conn_to_ap)           | request Server to connect to configured AP
CTRL | 0x04    | BLUFI_TYPE_CTRL_SUBTYPE_DISCONN_FROM_AP                                             |
CTRL | 0x05    | [BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_STATUS](#blufi_type_ctrl_subtype_get_wifi_status) | request current WiFi connection status
CTRL | 0x06    | BLUFI_TYPE_CTRL_SUBTYPE_DEAUTHENTICATE_STA                                          |
CTRL | 0x07    | [BLUFI_TYPE_CTRL_SUBTYPE_GET_VERSION](#blufi_type_ctrl_subtype_get_version)         | request BluFi version (**NOTE**: This is **NOT** the firmware version string, but the description of the BluFi protocol implemented)
CTRL | 0x08    | BLUFI_TYPE_CTRL_SUBTYPE_DISCONNECT_BLE                                              |
CTRL | 0x09    | [BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_LIST](#blufi_type_ctrl_subtype_get_wifi_list)     | request Server to perform a WiFi AP scan
DATA | 0x00    | BLUFI_TYPE_DATA_SUBTYPE_NEG                                                         |
DATA | 0x01    | BLUFI_TYPE_DATA_SUBTYPE_STA_BSSID                                                   | 6-byte BSSID
DATA | 0x02    | BLUFI_TYPE_DATA_SUBTYPE_STA_SSID                                                    | 1..32 character SSID value
DATA | 0x03    | BLUFI_TYPE_DATA_SUBTYPE_STA_PASSWD                                                  | 1..64 character PSK value
DATA | 0x04    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_SSID                                                 |
DATA | 0x05    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_PASSWD                                               |
DATA | 0x06    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_MAX_CONN_NUM                                         |
DATA | 0x07    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_AUTH_MODE                                            |
DATA | 0x08    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_CHANNEL                                              |
DATA | 0x09    | BLUFI_TYPE_DATA_SUBTYPE_USERNAME                                                    |
DATA | 0x0A    | BLUFI_TYPE_DATA_SUBTYPE_CA                                                          |
DATA | 0x0B    | BLUFI_TYPE_DATA_SUBTYPE_CLIENT_CERT                                                 |
DATA | 0x0C    | BLUFI_TYPE_DATA_SUBTYPE_SERVER_CERT                                                 |
DATA | 0x0D    | BLUFI_TYPE_DATA_SUBTYPE_CLIENT_PRIV_KEY                                             |
DATA | 0x0E    | BLUFI_TYPE_DATA_SUBTYPE_SERVER_PRIV_KEY                                             |
DATA | 0x0F    | BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP                                                    | WiFi status report
DATA | 0x10    | BLUFI_TYPE_DATA_SUBTYPE_REPLY_VERSION                                               | BluFi version report
DATA | 0x11    | BLUFI_TYPE_DATA_SUBTYPE_WIFI_LIST                                                   | WiFi scan report
DATA | 0x12    | BLUFI_TYPE_DATA_SUBTYPE_ERROR_INFO                                                  | BluFi error report see [Errors](#blufi-errors)
DATA | 0x13    | BLUFI_TYPE_DATA_SUBTYPE_CUSTOM_DATA                                                 | Client-to-Server arbitrary (undefined) binary transfer as example of passing data

**NOTE**: These will be extended as we add functionality specific to
the Microbiota Labs world, so the table above should not be taken as
the complete definitive list since this documentation may be stale
with respect to the actual Server (firmware) and Client
implementations.

The format of the packets are either a non-fragmented simple structure:

```
struct blufi_hdr{
    uint8_t type;
    uint8_t fc;
    uint8_t seq;
    uint8_t data_len;
    uint8_t data[0];
};
```

or a fragmented **little-endian** structure:

```
struct blufi_frag_hdr {
    uint8_t type;
    uint8_t fc;
    uint8_t seq;
    uint8_t data_len;
    uint16_t total_len;
    uint8_t data[0];
};
```

The `type` field is encoded as follows:

```
+---7---+---6---+---5---+---4---+---3---+---2---+---1---+---0---+
|               subtype (6-bits)                | type (2-bits) |
+-------+-------+-------+-------+-------+-------+-------+-------+
```

The `fc` (flags) field is encoded as follows:

```
+---7---+---6---+---5---+---4---+---3---+---2---+---1---+---0---+
|   -   |   -   |   -   | FRAG  | RQACK |  DIR  | CHECK |  ENC  |
+-------+-------+-------+-------+-------+-------+-------+-------+
```

flag  | bit | mask | notes
:-----|:---:|:----:|:-----
ENC   | 0   | 0x01 | Encrypted
CHECK | 1   | 0x02 | Checksum
DIR   | 2   | 0x04 | Direction: 0==Client-to-ESP32 1==ESP32-to-Client
RQACK | 3   | 0x08 | Request ACK
FRAG  | 4   | 0x10 | Packet is fragmented

Fragmented packets are terminated by a non-fragmented packet.

The `seq` number is just a monotonically increasing sequence number
for the packets in a transaction. The BluFi Server firmware
**EXPECTS** the sequence number to be monotonically increasing for
each received packet from the Client. The sequence number expected is
reset to **`0x00`** on a DISCONNECT event. So a Client should always
start fromm 0x00 at the start-of-day, or when a new connection to the
BLE-GATT Server is established.

The `data_len` fields provides the number of bytes of data following
the packet headers as defined above. The fragmented packets having the
16-bit **little-endian** `total-len` field to allow the receiver to
ensure any buffers they pre-allocate for the data based on the
received header can be large enough. i.e. the first packet in a
fragmented sequence will give the total size needed to re-assemble the
data.

#### BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE

This `CTRL` packet expects a single-byte `OPMODE` value. The valid
`OPMODE` byte values are:

name            | `wifi_mode_t` value | description
----------------|:-------------------:|:-----------
WIFI_MODE_STA   | **`0x01`**          | Station mode
WIFI_MODE_AP    | **`0x02`**          | Access Point (SoftAP)
WIFI_MODE_APSTA | **`0x03`**          | SoftAP and Station

Normally we would use `WIFI_MODE_STA` for a setup where the ESP32 is
to connect to a local AP.

#### BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP

This `CTRL` message has no data body (so `data_len` should be
**`0x00`**). It is used to trigger the ESP32 Server to attempt to
associate with the AP as configured by previous
`BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE`,
`BLUFI_TYPE_IS_DATA_STA_SSID` and `BLUFI_TYPE_IS_DATA_STA_PASSWD`
requests.

#### BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_STATUS

This `CTRL` message has no data body (so `data_len` should be
**`0x00`**). It is used to trigger the ESP32 Server to generate a
`BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP` report packet describing the
current WiFi status. See [WiFi status](#wifi-status).

#### BLUFI_TYPE_CTRL_SUBTYPE_GET_VERSION

As already mentioned this `CTRL` request will result in the BluFi
version information being returned (describing the BluFi packet
protocol and subtype definitions in use). A new call will be added in
the near future to return the parent ESP32 application version and
identity information.

#### BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_LIST

This `CTRL` request provides no data (so `data_len` should be
**`0x00`**). It triggers the Server to perform a WiFi scan for APs,
and will result in a `BLUFI_TYPE_DATA_SUBTYPE_WIFI_LIST` response
packet after a few seconds.

#### BluFi Errors

The `BLUFI_TYPE_DATA_SUBTYPE_ERROR_INFO` packet provides a 1-byte
error code:

name                          | code | notes
:-----------------------------|:----:|:-----
ESP_BLUFI_SEQUENCE_ERROR      | 0x00 |
ESP_BLUFI_CHECKSUM_ERROR      | 0x01 |
ESP_BLUFI_DECRYPT_ERROR       | 0x02 |
ESP_BLUFI_ENCRYPT_ERROR       | 0x03 |
ESP_BLUFI_INIT_SECURITY_ERROR | 0x04 |
ESP_BLUFI_DH_MALLOC_ERROR     | 0x05 |
ESP_BLUFI_DH_PARAM_ERROR      | 0x06 |
ESP_BLUFI_READ_PARAM_ERROR    | 0x07 |
ESP_BLUFI_MAKE_PUBLIC_ERROR   | 0x08 |

#### Examples

From wireshark investigation of Android captured `btsnoop_hci.log` we
have the following worked examples:

**TODO**: More detail and examples of the BluFi packet encoding to be
provided to ensure clarity. e.g. Encrypted and checksummed packets as
well as detailed descriptions of the **DATA** supplied in the packets.

**NOTE**: A valid "configuration" sequence after connecting to the
Server to configure the device as a station (STA) associating against
a WPA-PSK AP would be:

- write FF01 with binary packet for BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE opmode WIFI_MODE_STA
- write FF01 with binary packet for BLUFI_TYPE_IS_DATA_STA_SSID with SSID string
- write FF01 with binary packet for BLUFI_TYPE_IS_DATA_STA_PASSWD with PSK string
- write FF01 with binary packet for BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP
- handler FF02 read of BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP containing the WiFi status

The binary dumps below show the packets in more detail.

#### WiFi configure mode
```
Write ServiceUUID=FFFF UUID=FF01 Value=08 08 00 01 01
	type = 08      		 00001000    type=0=BLUFI_TYPE_CTRL subtype=000010=0x2=BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE
	fc = 08			 00001000    RQACK (and implied BLUFI_FC_DIR_P2E)
	seq = 00
	data_len = 01
	data = 01		 OPMODE == 0x01 == WIFI_MODE_STA
```

Since RQACK is set we expect (and get) an asynchronous response on 0xFF02:
```
Receive Value Notification ServiceUUID=FFFF UUID=FF02 Value=00 04 00 01 00
	type = 00
	fc = 04                 00000100	BLUFI_FC_DIR_E2P
	seq = 00		matches seq of original request
	data_len = 01
	data = 00		0x00 == OK
```

#### WiFi configure SSID
```
Write ServiceUUID=FFFF UUID=FF01 Value=09 00 01 08 73 68 6d 6f 75 73 69 65
	type = 09		 00001001    type=1=BLUFI_TYPE_DATA subtype=000010=0x2=BLUFI_TYPE_IS_DATA_STA_SSID
	fc = 00
	seq = 01
	data_len = 08
	data = 73 68 6d 6f 75 73 69 65		"shmousie"
```

#### WiFi configure PSK
```
Write ServiceUUID=FFFF UUID=FF01 Value=0D 00 02 0E xx yy zz xx yy zz xx yy zz xx yy zz xx yy
	type = 0D		 00001101    type=1=BLUFI_TYPE_DATA subtype=000011=0x3=BLUFI_TYPE_IS_DATA_STA_PASSWD
	fc = 00
	seq = 02
	data_len = 0E
	data = ...elided.. 	 Password string
```

#### WiFi request connect to AP
```
Write ServiceUUID=FFFF UUID=FF01 Value=0c 00 03 00
	type = 0C		 00001100    type=0=BLUFI_TYPE_CTRL subtype=000011=0x3=BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP
	fc = 00
	seq = 03
	data_len = 00
```

#### WiFi status

After sending BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP we expect a status response:
```
Receive Value Notification ServiceUUID=FFFF UUID=FF02 Value=3D 04 01 15 01 00 00 01 06 64 66 B3 3A E0 33 02 08 73 68 6D 6F 75 73 69 65
	type = 3D                00111101    type=1=BLUFI_TYPE_DATA subtype=001111=0xF=BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP
	fc = 04			 00000100    BLUFI_FC_DIR_E2P
	seq = 01
	data_len = 15
	data = 01 00 00 01 06 64 66 B3 3A E0 33 02 08 73 68 6D 6F 75 73 69 65
		opmode = 01		(wifi_mode_t) 01 == WIFI_MODE_STA
		sta_conn_state = 00	(esp_blufi_sta_conn_state_t) 00 == SUCCESS
		softap_conn_num = 00	(uint8_t)
		01 = BLUFI_TYPE_DATA_SUBTYPE_STA_BSSID
		06 = len
		64 66 B3 3A E0 33 = BSSID
		02 = BLUFI_TYPE_DATA_SUBTYPE_STA_SSID
		08 = len
		73 68 6D 6F 75 73 69 65 = SSID  "shmousie"
```

The `sta_conn_state` field as shown above is **`0x00`** indicating
SUCCESS. The value **`0x01`** indicates FAIL.

#### WiFi request status
```
Write ServiceUUID=FFFF UUID=FF01 Value=14 00 01 00
	type = 14      		 00010100    type=0=BLUFI_TYPE_CTRL subtype=000101=0x5=BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_STATUS
	fc = 00
	seq = 01
	data_len = 00
```

We will then receive a WIFI_REP response on 0xFF02:
```
Received ServiceUUID=FFFF UUID=FF02 Value=
	0000   3d 04 01 15 01 00 00 01 06 64 66 b3 3a e0 33 02  =........df.:.3.
	0010   08 73 68 6d 6f 75 73 69 65                       .shmousie

	type = 3D		00111101	type=BLUFI_TYPE_DATA subtype=001111=0xF=BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP
	fc = 04			00000100	BLUFI_FC_DIR_E2P
	seq = 01
	data_len = 15
	data =
		opmode = 01		(wifi_mode_t) 01 == WIFI_MODE_STA
		sta_conn_state = 00	(esp_blufi_sta_conn_state_t) 00 == SUCCESS
		softap_conn_num = 00	(uint8_t)
		01 = BLUFI_TYPE_DATA_SUBTYPE_STA_BSSID
		06 = len
		64 66 B3 3A E0 33 = BSSID
		02 = BLUFI_TYPE_DATA_SUBTYPE_STA_SSID
		08 = len
		73 68 6D 6F 75 73 69 65 = SSID  "shmousie"
```

#### Custom data from Client-to-ESP32
```
Write ServiceUUID=FFFF UUID=FF01 Value=4d 00 03 05 68 65 6c 6c 6f  "hello"
struct blifi_hdr
	type = 4D		0b01001101	type=0b01=0x1=BLUFI_TYPE_DATA subtype=0b010011=0x13=BLUFI_TYPE_DATA_SUBTYPE_CUSTOM_DATA
	fc = 00			0b00000000	BLUFI_FC_DIR_MASK=0=BLUFI_FC_DIR_P2E
	seq = 03
	data_len = 05
	data="hello"
```

#### Get WiFi APs
```
Write ServiceUUID=FFFF UUID=FF01 Value=24 00 02 00
struct blifi_hdr
       type = 24		0b00100100	type=0b00=0x0=BLUFI_TYPE_CTRL subtype=0b001001=0x9=BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_LIST
       fc = 00			0b00000000	BLUFI_FC_DIR_MASK=0=BLUFI_FC_DIR_P2E
       seq = 02
       data_len = 00

Write Response ServiceUUID=FFFF UUID=FF01

Received ServiceUUID=FFFF UUID=FF02 Value=
	0000   45 14 02 77 db 00 09 c6 73 68 6d 6f 75 73 69 65  E..w....shmousie
	0010   0d bc 56 69 72 67 69 6e 20 4d 65 64 69 61 0a bb  ..Virgin Media..
	0020   73 68 6d 6f 75 73 69 65 32 08 b8 56 4d 2d 34 33  shmousie2..VM-43
	0030   5f 41 09 b6 73 68 6d 6f 75 73 69 65 08 b3 56 4d  _A..shmousie..VM
	0040   2d 34 33 5f 41 05 b1 56 4d 34 33 0d b1 56 69 72  -43_A..VM43..Vir
	0050   67 69 6e 20 4d 65 64 69 61 0a b1 56 4d 32 35 31  gin Media..VM251
	0060   37 32 36 36 0d b0 56 69 72 67 69 6e 20 4d 65 64  7266..Virgin Med
	0070   69 61 0b ae 73 65 74 75 70 46 46                 ia..setupFF

	type = 45	01000101	type=BLUFI_TYPE_DATA subtype=010001=0x11=BLUFI_TYPE_DATA_SUBTYPE_WIFI_LIST
	fc = 14		00010100	BLUFI_FC_DIR_E2P | BLUFI_FC_FRAG
	seq = 02
	data_len = 77
	total_len = 00DB
	data = ...
		09 = len (RSSI byte + SSID)
		C6 = RSSI
		73 68 6d 6f 75 73 69 65 = SSID "shmousie"

		0D  = len (RSSI byte + SSID)
		BC = RSSI
		56 69 72 67 69 6e 20 4d 65 64 69 61 = SSID "Virgin Media"

		.. etc ..

Received ServiceUUID=FFFF UUID=FF02 Value=
	0000   45 04 03 66 35 32 30 09 ad 53 4b 59 44 44 35 39  E..f520..SKYDD59
	0010   37 0d ac 50 4c 55 53 4e 45 54 2d 51 37 33 37 0a  7..PLUSNET-Q737.
	0020   a8 56 4d 32 39 33 31 30 34 37 0a a7 56 4d 36 37  .VM2931047..VM67
	0030   30 37 37 39 33 0d a6 56 69 72 67 69 6e 20 4d 65  07793..Virgin Me
	0040   64 69 61 0d a5 56 69 72 67 69 6e 20 4d 65 64 69  dia..Virgin Medi
	0050   61 0d a4 56 69 72 67 69 6e 20 4d 65 64 69 61 0a  a..Virgin Media.
	0060   a1 56 4d 39 36 37 33 36 36 37                    .VM9673667

	type = 45
	fc = 04		00000100	BLUFI_FC_DIR_E2P
	seq = 03
	data_len = 66
	data = ...
```

#### Disconnect BLE
This is just a notification to the Server that the Client is about to
disconnect, and is not related to the internal Client BLE disconnect
operation.

```
Write ServiceUUID=FFFF UUID=FF01 Value=20 00 04 00
	type = 20      00100000	type=0b00=0x0=BLUFI_TYPE_CTRL subtype=0b001000=0x8=BLUFI_TYPE_CTRL_SUBTYPE_DISCONNECT_BLE
	fc = 00	       0b00000000	BLUFI_FC_DIR_MASK=0=BLUFI_FC_DIR_P2E
	seq = 04
	data_len = 00
```

An overview of [The Frame Formats Defined in
BluFi](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/blufi.html#frame-formats)
though working from the esp-idf source should be treated as the definitive documentation.

### Firmware internals

For the ESP32 Server firmware the internal BluFi event API is defined
in the [esp_blufi_api.h](../../3rd_party/esp-idf/components/bt/bluedroid/api/include/api/esp_blufi_api.h)
header file. The firmware gets events triggered based on the BLE-GATT
activity of the client.

The event messages are generated from the
[blufi_protocol.c](../../3rd_party/esp-idf/components/bt/bluedroid/btc/profile/esp/blufi/blufi_protocol.c)
source, based on definitions provided by the
[blufi_int.h](../../3rd_party/esp-idf/components/bt/bluedroid/btc/profile/esp/blufi/include/blufi_int.h)
header file.

Packets are built for sending to the Client in the
[blufi_prf.c](../../3rd_party/esp-idf/components/bt/bluedroid/btc/profile/esp/blufi/blufi_prf.c)
source file.

## OTA

TODO: Implement initial pull OTA support and document features as necessary here.

## HTTPS daemon certificates

The certs files were created using:

```
openssl req -newkey rsa:2048 -nodes -keyout prvtkey.pem -x509 -days 3650 -out cacert.pem -subj "/CN=Microbiota Labs HTTPS server"
```

**NOTE**: Ideally we would NOT be storing the private keys in the
source repository, but would acquire the file via a secure mechanism
during the build process. The security for the device still needs to
be fully addressed (i.e. using secure-boot, encrypted flash and giving
each device a factory-set unique identity (and more than likely a
unique cert/key pair per-device).
