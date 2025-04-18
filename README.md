# houselights
A web server to control lights
## Overview
This web server controls lights based on a schedule. The actual control of the lights is provided by a "control" web service and the location for each light is retrieved based on the [HousePortal](https://github.com/pascal-fb-martin/houseportal) discovery mechanism.

See the [gallery](https://github.com/pascal-fb-martin/houselights/blob/main/gallery/README.md) for a view of HouseLights' web UI.

Examples of control web services compatible with the server  are [orvibo](https://github.com/pascal-fb-martin/orvibo), [HouseKasa](https://github.com/pascal-fb-martin/housekasa) and [HouseRelays](https://github.com/pascal-fb-martin/houserelays).

Schedule times can be provided as time of day (format HH:MM), relative time after sunset (format +HH:MM) or relative time before sunrise (format -HH:MM).

## Installation
* Install the OpenSSL development package(s).
* Install [echttp](https://github.com/pascal-fb-martin/echttp).
* Install [houseportal](https://github.com/pascal-fb-martin/houseportal).
* Clone this GitHub repository.
* make
* sudo make install

You will need to have at least one control service installed and running.

## Configuration
The lights schedule can be edited from the HouseLights web interface.

## Panel

The web interface includes a Panel page (/lights/panel.html) that has no menu and only shows the current lights, each as one big button to tun the device on and off. This page is meant for a phone screen, typically a shortcut on the phone's home screen. (Because HousePortal redirects the URL, it is recommended to turn the phone in airplane mode when creating the shortcut from the web browser.)

