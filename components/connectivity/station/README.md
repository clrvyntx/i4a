# Station Component

Responsabilities:
- The station component is a component that represents a station in the network. 
- It is responsible for connecting to a certain network and sending and receiving messages to and from other stations in the network.
- It does NOT manage bussiness logic, it is only responsible for the communication between station and AP.

## Station contract

What the station component should do:
- Connect/disconnect to a certain network.
- Scan for N available networks.
- Specify the nearest network?
- Manage the events?

What the station component should NOT do:
- Manage bussiness logic.
- Manage the network.

## Station parameters

- SSID of the network to connect to
- Password of the network to connect to
- Device uuid