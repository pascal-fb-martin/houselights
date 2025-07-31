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

# Local build ---------------------------------------------------

OBJS= houselights_plugs.o houselights_schedule.o houselights.o
LIBOJS=

all: houselights

clean:
	rm -f *.o *.a houselights

rebuild: clean all

%.o: %.c
	gcc -c -Os -o $@ $<

houselights: $(OBJS)
	gcc -Os -o houselights $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lmagic -lrt

dev:

# Distribution agnostic file installation -----------------------

install-ui: install-preamble
	$(INSTALL) -m 0755 -d $(DESTDIR)$(SHARE)/public/lights
	$(INSTALL) -m 0644 public/* $(DESTDIR)$(SHARE)/public/lights

install-app: install-ui
	$(INSTALL) -m 0755 -s houselights $(DESTDIR)$(prefix)/bin
	touch $(DESTDIR)/etc/default/lights

uninstall-app:
	rm -rf $(DESTDIR)$(SHARE)/public/lights
	rm -f $(DESTDIR)$(prefix)/bin/houselights

purge-app:

purge-config:
	rm -rf /etc/house/lights.config /etc/default/lights

# System installation. ------------------------------------------

include $(SHARE)/install.mak

