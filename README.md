# fty-asset

fty-asset is an agent managing information about assets.
Assets are either locations (datacenter, room, row, rack) or devices (RC, UPS, EPDU, etc.).

## How to build

To build `fty-asset` project run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=On ..
make
```

## What is built

### `fty-asset` shared library
This library provides the asset DTO used to exchange asset information between modules.

### `fty-asset-server` binary
The agent that provides access to all assets functionalities

### `fty-asset-accessor` shared library
This library provides a point of access for the server requests

## How to run
The asset agent is installed and runs as a system service

* from an installed base, using systemd, run:

```bash
systemctl start fty-asset
```

<!-- Agent also has a command line interface fty-asset-cli. -->

<!-- For further information, refer to the manual page of fty-asset-cli. -->

<!-- ### Configuration file

Configuration file - fty-asset.cfg - is currently ignored.
Agent reads environment variable BIOS\_ASSETS\_REPEAT, which sets how often are all the assets republished on ASSETS stream. -->

## Architecture

### Overview

fty-asset is composed of 3 actors:

* asset-server: does general asset management
* asset-autoupdate: publishes inventory messages when available information about RCs changes
* asset-inventory: parses inventory messages and stores their content into DB

In addition to actors, there are 2 timers:

* asset server timer: runs every BIOS\_ASSET\_REPEATS seconds (by default once every hour) and triggers republish of all known assets on ASSETS stream
* autoupdate timer: runs once in a 5 minutes and triggers update of information about all RCs

## Protocols

### Published metrics

Agent doesn't publish any metrics.

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

It is possible to request the fty-asset agent for:

* power topology chains,
* republishing assets,
* list of assets in given container,
* list of assets of given type/subtype,
* creation and updating of assets,
* user-friendly name of given asset,
* all the data available about given asset

#### Power topology

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* REQUEST/'uuid'/POWER/'asset-iname'

where
* '/' indicates a multipart string message
* 'asset-iname' MUST be the asset iname
* 'uuid' is the correlation id of the message
* subject of the message MUST be TOPOLOGY

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'uuid'/REPLY/POWER/'asset-iname'/OK/'D1'/.../'Dn'
* 'uuid'/REPLY/POWER/'asset-iname'/ERROR/reason

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be the same as in request
* 'uuid' MUST be the same as in request
* 'D1',...,'Dn' MUST be assets in the power topology of 'asset-iname'
* 'reason' is a standard message (JSON payload) describing the error

Example of bmsg request:
```bash
bmsg request asset-agent TOPOLOGY REQUEST 1234 POWER server-17
```

#### Upstream topology of the nearest energy chain of a powered asset

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* REQUEST/'uuid'/POWER\_TO/'asset-iname'

where
* '/' indicates a multipart string message
* 'asset-iname' MUST be the asset iname
* 'uuid' is the correlation id of the message
* subject of the message MUST be TOPOLOGY

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'uuid'/REPLY/POWER\_TO/'asset-iname'/OK/\<powerchains\>
* 'uuid'/REPLY/POWER\_TO/'asset-iname'/ERROR/reason

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be the same as in request
* 'uuid' MUST be the same as in request
* 'reason' is a standard message (JSON payload) describing the error
* \<powerchains\> is a unique frame (JSON payload) as:
```bash
{
    "asset-id": "<asset-iname>",
    "powerchains": [
        {
            "src-id": "<D1>",
            "src-socket": "<D1-id>",
            "dst-id": "<asset-iname>"
        },
        ...,
        {
            "src-id": "<Dn>",
            "src-socket": "<Dn-id>",
            "dst-id": "<asset-iname>"
        }
    ]
}
```
    with
    * 'D1',...,'Dn' the assets in the upstream powerchain topology of 'asset-iname'
    * 'D1-id',...,'Dn-id' the corresponding socket identifiers (src-id outlet)

Example of bmsg request:
```bash
bmsg request asset-agent TOPOLOGY REQUEST 1234 POWER_TO server-17
```
with response (unique frame for the JSON payload):
```bash
1234
REPLY
POWER_TO
server-17
OK
{
	"asset-id": "server-17",
	"powerchains": [
		{
			"src-id": "epdu-13",
			"src-socket": "7",
			"dst-id": "server-17"
		},
		{
			"src-id": "epdu-10",
			"src-socket": "1",
			"dst-id": "server-17"
		}
	]
}
```

#### Powerchains topology of a powered asset

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* REQUEST/'uuid'/POWERCHAINS/'command'/'asset-iname'

where
* '/' indicates a multipart string message
* 'command' MUST be 'from', 'to', 'filter_dc' or 'filter_group'
* 'asset-iname' MUST be the asset iname
* 'uuid' is the correlation id of the message
* subject of the message MUST be TOPOLOGY

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'uuid'/REPLY/POWERCHAINS/'asset-iname'/OK/\<powerchains\>
* 'uuid'/REPLY/POWERCHAINS/'asset-iname'/ERROR/reason

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be the same as in request
* 'uuid' MUST be the same as in request
* 'reason' is a standard message (JSON payload) describing the error
* \<powerchains\> is a unique frame (JSON payload) as:

    to be documented (see RFC11 /api/v1/topology/power)

Example of bmsg request:
```bash
bmsg request asset-agent TOPOLOGY REQUEST 1234 POWERCHAINS to server-17
```

#### Topology location

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* REQUEST/'uuid'/LOCATION/'command'/'asset-iname'/'options'

where
* '/' indicates a multipart string message
* 'command' MUST be 'from', 'to'
* 'asset-iname' MUST be the asset iname
* 'options' MUST be empty if command is 'to'
* 'options' MUST be a valid JSON paylaod if command is 'from', with members as:

    \"recursive\" is a boolean
    \"feed_by\" is a string describing a valid device asset
    \"filter\" is a string in {\"rooms\", \"rows\", \"racks\", \"groups\", \"devices\", \"\"}
    defaults are { \"recursive\": false, \"filter\": \"\", \"feed_by\": \"\" }
* 'uuid' is the correlation id of the message
* subject of the message MUST be TOPOLOGY

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'uuid'/REPLY/LOCATION/'asset-iname'/OK/\<location\>
* 'uuid'/REPLY/LOCATION/'asset-iname'/ERROR/reason

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be the same as in request
* 'uuid' MUST be the same as in request
* 'reason' is a standard message (JSON payload) describing the error
* \<location\> is a unique frame (JSON payload) as:

    to be documented (see RFC11 /api/v1/topology/location)

Example of bmsg request:
```bash
bmsg request asset-agent TOPOLOGY REQUEST 1234 LOCATION to server-17
bmsg request asset-agent TOPOLOGY REQUEST 1234 LOCATION from room-9 '{ "filter": "devices" }'
```

#### Topology, input power chain

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* REQUEST/'uuid'/INPUT_POWERCHAIN/'asset-iname'

where
* '/' indicates a multipart string message
* 'asset-iname' MUST be the asset iname of a datacenter
* 'uuid' is the correlation id of the message
* subject of the message MUST be TOPOLOGY

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'uuid'/REPLY/INPUT_POWERCHAIN/'asset-iname'/OK/\<input_powerchain\>
* 'uuid'/REPLY/INPUT_POWERCHAIN/'asset-iname'/ERROR/reason

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be the same as in request
* 'uuid' MUST be the same as in request
* 'reason' is a standard message (JSON payload) describing the error
* \<input_powerchain\> is a unique frame (JSON payload) as:

    to be documented (see RFC11 /api/v1/topology/input_power_chain)

Example of bmsg request:
```bash
bmsg request asset-agent TOPOLOGY REQUEST 1234 INPUT_POWERCHAIN server-17
```

#### Republishing assets

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* 'message'

where:

* subject MUST be REPUBLISH

AND EITHER

* message MUST consist of string $all

OR

* message MUST be multipart string message, where every string is asset iname (message MAY be empty)

The FTY-ASSET-AGENT peer act:

* IF message was $all OR empty:

    publication of fty-proto asset update message on the stream ASSETS for each asset in DB
* IF message was multipart string message

    publication of fty-proto asset update message on the stream ASSETS for each asset in request

The FTY-ASSET-AGENT peer MUST respond to USER peer using MAILBOX SEND.

* 'DONE'

Example of bmsg request:
```bash
bmsg request asset-agent REPUBLISH
bmsg request asset-agent REPUBLISH $all
bmsg request asset-agent REPUBLISH ups-02973445
```

#### Assets in given container

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* GET/'container-name'/'type-1'/.../'type-n'

where
* '/' indicates a multipart string message
* 'container-name' MAY be asset iname
* 'type-1'/.../'type-n' MAY be asset types/subtypes
* subject of the message MUST be "ASSETS\_IN\_CONTAINER"

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* OK/'A1'/.../'An'
* ERROR/<reason>

where
* '/' indicates a multipart frame message
* IF 'container-name' in request was empty AND 'type-1/.../type-n' in request was empty,

    'A1'/.../'An' MUST be ALL the assets in the DB (MAY be empty)
* IF 'container-name' in request was empty AND 'type-1/.../type-n' in request was non-empty,

    'A1'/.../'An' MUST be ALL the assets in the DB of ANY of the specified types/subtypes (MAY be empty)
* IF 'container-name' in request was non-empty AND 'type-1/.../type-n' in request was empty,

    'A1'/.../'An' MUST be ALL assets which have 'container-name' as parent on any level (MAY be empty)
* IF 'container-name' in request was non-empty AND 'type-1/.../type-n' in request was non-empty,

    'A1'/.../'An' MUST be ALL assets which have 'container-name' as parent on any level of ANY of the given types or subtypes (MAY be empty)
* 'reason' is string detailing reason for error. Possible values are:

    ASSET\_NOT\_FOUND / BAD\_COMMAND/INTERNAL\_ERROR
* subject of the message MUST be "ASSETS\_IN\_CONTAINER"

#### Assets of given type/subtype

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* GET/<uuid>/'type-1'/.../'type-n'

where
* '/' indicates a multipart frame message
* <uuid> zuuid of the message
* 'type-1'/.../'type-n' MAY be asset types/subtypes
* subject of the message MUST be "ASSETS".

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* OK/<uuid>/'A1'/.../'An'
* ERROR/<uuid>/<reason>

where
* '/' indicates a multipart frame message
* IF 'type-1/.../type-n' in request was empty,

   'A1'/.../'An' MUST be ALL assets in the DB
* IF 'type-1/.../type-n' in request was non-empty,

   'A1'/.../'An' MUST be ALL assets of ANY of the specified types/subtypes in the DB
* 'reason' is string detailing reason for error. Possible values are:

    ASSET\_NOT\_FOUND / BAD\_COMMAND / MISSING\_COMMAND / INTERNAL\_ERROR
* subject of the message MUST be "ASSETS".

#### Creation and updating of assets

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* 'asset'

where
* 'asset' is fty-proto create OR update asset message
* subject of the message MUST be "ASSET\_MANIPULATION".

The FTY-ASSET-AGENT peer MUST:
* insert provided data into DB
* publish UPDATE message for given asset on stream ASSETS
* respond with one of the messages back to USER
peer using MAILBOX SEND:

    * OK/'asset-iname'
    * ERROR/<reason>

where
* '/' indicates a multipart frame message
* 'asset-iname' MUST be iname of created or updated asset
* 'reason' is string detailing reason for error. Possible values are:

    OPERATION\_NOT\_IMPLEMENTED (MAY be empty)
* subject of the message MUST be "ASSET\_MANIPULATION".

#### Getting user-friendly name for given asset

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* 'asset-iname'

where
* 'asset-iname' is internal name of an asset
* subject of the message MUST be "ENAME\_FROM\_INAME".

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* OK/'asset-ename'
* ERROR/<reason>

where
* '/' indicates a multipart frame message
* 'asset-ename' is user-friendly name corresponding to 'asset-iname' from the request
* 'reason' is string detailing reason for error. Possible values are:

   MISSING\_INAME/ASSET\_NOT\_FOUND
* subject of the message MUST be "ENAME\_FROM\_INAME".

#### Getting all the available asset data for given asset

The USER peer sends the following messages using MAILBOX SEND to
FTY-ASSET-AGENT ("asset-agent") peer:

* GET/<uuid>/'asset-iname'

where
* '/' indicates a multipart frame message
* 'asset-iname' is internal name of an asset
* <uuid> is zuuid of the message
* subject of the message MUST be "ASSET\_DETAIL".

The FTY-ASSET-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'asset-message'
* ERROR/<uuid>/<reason>

where
* '/' indicates a multipart frame message
* 'asset-message' is fty-proto update message containing available data for given asset
* 'reason' is string detailing reason for error. Possible values are:

   BAD\_COMMAND/ASSET\_NOT\_FOUND
* subject of the message MUST be "ASSET\_DETAIL".


Example of bmsg request:
```bash
bmsg request asset-agent ASSET_DETAIL GET 1234 ups-02973445
```

### Stream subscriptions

Agent is subscribed to ASSETS stream.

If it receives inventory message, asset-inventory actor extracts data from it and puts them into DB.
If it receives any other asset message, asset-server actor for all the children of this asset:

* updates power topology

* updates stored data

* publishes update message on ASSETS stream
