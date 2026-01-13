# HouseLights - a simple web server to control lights.
#
# Copyright 2025, Pascal Martin
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
# WARNING
#
# This Makefile depends on echttp and houseportal (dev) being installed.

prefix=/usr/local
SHARE=$(prefix)/share/house

INSTALL=/usr/bin/install

HAPP=houselights
HCAT=automation

EXTRADOC=/var/lib/house/note/extra

# Local build ---------------------------------------------------

OBJS= houselights_plugs.o \
      houselights_schedule.o \
      houselights_template.o \
      houselights.o

LIBOJS=

all: houselights

clean:
	rm -f *.o *.a houselights

rebuild: clean all

%.o: %.c
	gcc -c -Wall -Os -o $@ $<

houselights: $(OBJS)
	gcc -Os -o houselights $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lmagic -lrt

dev:

# Distribution agnostic file installation -----------------------

install-ui: install-preamble
	$(INSTALL) -m 0755 -d $(DESTDIR)$(SHARE)/public/lights
	$(INSTALL) -m 0644 public/* $(DESTDIR)$(SHARE)/public/lights
	$(INSTALL) -m 0755 -d $(DESTDIR)/var/lib/house/lights
	$(INSTALL) -m 0644 mapbody.htmt $(DESTDIR)/var/lib/house/lights
	$(INSTALL) -m 0755 -d $(DESTDIR)$(EXTRADOC)/$(HPKG)/gallery
	$(INSTALL) -m 0644 gallery/* $(DESTDIR)$(EXTRADOC)/$(HPKG)/gallery
	if [ "x$(DESTDIR)" = "x" ] ; then grep -q '^house:' /etc/passwd && chown -R house:house /var/lib/house/lights /var/cache/house/lights ; rm -rf /var/cache/house/lights/* ; fi

install-runtime: install-preamble
	$(INSTALL) -m 0755 -d $(DESTDIR)/var/cache/house/lights
	$(INSTALL) -m 0755 -s houselights $(DESTDIR)$(prefix)/bin
	touch $(DESTDIR)/etc/default/lights

install-app: install-ui install-runtime

uninstall-app:
	rm -rf $(DESTDIR)$(SHARE)/public/lights
	rm -f $(DESTDIR)$(prefix)/bin/houselights
	rm -rf $(DESTDIR)/var/lib/house/lights
	rm -rf $(DESTDIR)/var/cache/house/lights

purge-app:

purge-config:
	rm -rf $(DESTDIR)/etc/house/lights.config
	rm -rf $(DESTDIR)/etc/default/lights
	rm -rf $(DESTDIR)/var/lib/house/lights
	rm -rf $(DESTDIR)/var/cache/house/lights

# Build a private Debian package. -------------------------------

install-package: install-ui install-runtime install-systemd

debian-package: debian-package-generic

# System installation. ------------------------------------------

include $(SHARE)/install.mak

