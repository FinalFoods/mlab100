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

type | subtype | name                                        | notes
---:|:-------:|:---------------------------------------------|:-----
CTRL | 0x00    | BLUFI_TYPE_CTRL_SUBTYPE_ACK                 |
CTRL | 0x01    | BLUFI_TYPE_CTRL_SUBTYPE_SET_SEC_MODE        |
CTRL | 0x02    | BLUFI_TYPE_CTRL_SUBTYPE_SET_WIFI_OPMODE     |
CTRL | 0x03    | BLUFI_TYPE_CTRL_SUBTYPE_CONN_TO_AP          |
CTRL | 0x04    | BLUFI_TYPE_CTRL_SUBTYPE_DISCONN_FROM_AP     |
CTRL | 0x05    | BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_STATUS     |
CTRL | 0x06    | BLUFI_TYPE_CTRL_SUBTYPE_DEAUTHENTICATE_STA  |
CTRL | 0x07    | BLUFI_TYPE_CTRL_SUBTYPE_GET_VERSION         |
CTRL | 0x08    | BLUFI_TYPE_CTRL_SUBTYPE_DISCONNECT_BLE      |
CTRL | 0x09    | BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_LIST       |
DATA | 0x00    | BLUFI_TYPE_DATA_SUBTYPE_NEG                 |
DATA | 0x01    | BLUFI_TYPE_DATA_SUBTYPE_STA_BSSID           |
DATA | 0x02    | BLUFI_TYPE_DATA_SUBTYPE_STA_SSID            |
DATA | 0x03    | BLUFI_TYPE_DATA_SUBTYPE_STA_PASSWD          |
DATA | 0x04    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_SSID         |
DATA | 0x05    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_PASSWD       |
DATA | 0x06    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_MAX_CONN_NUM |
DATA | 0x07    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_AUTH_MODE    |
DATA | 0x08    | BLUFI_TYPE_DATA_SUBTYPE_SOFTAP_CHANNEL      |
DATA | 0x09    | BLUFI_TYPE_DATA_SUBTYPE_USERNAME            |
DATA | 0x0a    | BLUFI_TYPE_DATA_SUBTYPE_CA                  |
DATA | 0x0b    | BLUFI_TYPE_DATA_SUBTYPE_CLIENT_CERT         |
DATA | 0x0c    | BLUFI_TYPE_DATA_SUBTYPE_SERVER_CERT         |
DATA | 0x0d    | BLUFI_TYPE_DATA_SUBTYPE_CLIENT_PRIV_KEY     |
DATA | 0x0e    | BLUFI_TYPE_DATA_SUBTYPE_SERVER_PRIV_KEY     |
DATA | 0x0f    | BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP            |
DATA | 0x10    | BLUFI_TYPE_DATA_SUBTYPE_REPLY_VERSION       |
DATA | 0x11    | BLUFI_TYPE_DATA_SUBTYPE_WIFI_LIST           |
DATA | 0x12    | BLUFI_TYPE_DATA_SUBTYPE_ERROR_INFO          |
DATA | 0x13    | BLUFI_TYPE_DATA_SUBTYPE_CUSTOM_DATA         |

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
for the packets in a transaction.

The `data_len` fields provides the number of bytes of data following
the packet headers as defined above. The fragmented packets having the
16-bit **little-endian** `total-len` field to allow the receiver to
ensure any buffers they pre-allocate for the data based on the
received header can be large enough. i.e. the first packet in a
fragmented sequence will give the total size needed to re-assemble the
data.

From wireshark investigation of Android captured `btsnoop_hci.log` we
have the following worked examples:

**TODO**: More detail and examples of the BluFi packet encoding to be
provided to ensure clarity. e.g. Encrypted and checksummed packets as
well as detailed descriptions of the **DATA** supplied in the packets.

#### WiFi status
```
Write ServiceUUID=FFFF UUID=FF01 Value=14 00 01 00
	type = 14      		 00010100    type=0=BLUFI_TYPE_CTRL subtype=000101=0x5=BLUFI_TYPE_CTRL_SUBTYPE_GET_WIFI_STATUS
	fc = 00
	seq = 01
	data_len = 00

Received ServiceUUID=FFFF UUID=FF02 Value=
	0000   3d 04 01 15 01 00 00 01 06 64 66 b3 3a e0 33 02  =........df.:.3.
	0010   08 73 68 6d 6f 75 73 69 65                       .shmousie

	type = 3D		00111101	type=BLUFI_TYPE_DATA subtype=001111=0xF=BLUFI_TYPE_DATA_SUBTYPE_WIFI_REP
	fc = 04			00000100	BLUFI_FC_DIR_E2P
	seq = 01
	data_len = 15
	data =
		opmode = 01		??
		sta_conn_state = 00	??
		softap_conn_num = 00	??
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
