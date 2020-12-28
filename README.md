# houselights
A web server to control lights
## Overview
This web server controls lights based on a schedule. The actual control of the lights is provided by a "control" web service and the location for each light is retrieved based on the [HousePortal](https://github.com/pascal-fb-martin/houseportal) discovery mechanism.

Examples of control web services compatible with the server  are [orvibo](https://github.com/pascal-fb-martin/orvibo) and [HouseRelays](https://github.com/pascal-fb-martin/houserelays).

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

