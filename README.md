# HouseLights

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

The list of control points is dynamically retrieved from the House services implementing the `control` interface.

This service supports a graphic map display to control the lights.  That map display requires the presence of a user created `floorplan.svg` file in `/var/lib/house/lights`. This SVG file is typically created using Inkscape (see later).

An HTML page is automatically generated based on this `floorplan.svg` file and cached as `/var/cache/house/lights/mapbody.html`. All files in `/var/cache/house/lights` must be deleted after each change to any `/var/lib/house/lights` file.

## Creating a floor plan display using Inkscape

A floor plan SVG display can be created using Inkscape, but a few conventions must be followed:

* Define a background color for the page in `Document Properties`. It is recommended to use `#355b1eff`, which is the background color used in the web UI.

* Each SVG element that is intended to reflect the state of a point must not have the `stroke` attribute (i.e. not have an explicit stroke color), and must have an `id` attribute matching the name of the point except for any ' ' replaced with '_' (Inkscape does this automatically).

* Symbols can be used, but the same "no stroke" rule applies to all their elements that must be animated. The `id` attribute must then be applied to the `use` element.

* If multiple elements are to be animated from the same point, they must all be joined into a single group, and the `id` attribute must be applied to the group, not to the elements within that group.

* The Inkscape project must be exported as `plain SVG`, and this plain SVG file must be installed as `floorplan.svg` in `/var/lib/house/lights`.

> The `stroke` and `id` attributes can be modified using the Inkscape's XML Editor.

## Panel

The web interface includes a Panel page (/lights/panel.html) that has no menu and only shows the current lights, each as one big button to tun the device on and off. This page is meant for a phone screen, typically a shortcut on the phone's home screen. (Because HousePortal redirects the URL, it is recommended to turn the phone in airplane mode when creating the shortcut from the web browser.)

## Debian Packaging

The provided Makefile supports building private Debian packages. These are _not_ official packages:

- They do not follow all Debian policies.

- They are not built using Debian standard conventions and tools.

- The packaging is not separate from the upstream sources, and there is
  no source package.

To build a Debian package, use the `debian-package` target:

```
make debian-package
```

