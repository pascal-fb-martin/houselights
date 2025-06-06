
OBJS= houselights_plugs.o houselights_schedule.o houselights.o
LIBOJS=

SHARE=/usr/local/share/house

# Local build ---------------------------------------------------

all: houselights

clean:
	rm -f *.o *.a houselights

rebuild: clean all

%.o: %.c
	gcc -c -Os -o $@ $<

houselights: $(OBJS)
	gcc -Os -o houselights $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lrt

dev:

# Distribution agnostic file installation -----------------------

install-ui:
	mkdir -p $(SHARE)/public/lights
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/lights
	cp public/* $(SHARE)/public/lights
	chown root:root $(SHARE)/public/lights/*
	chmod 644 $(SHARE)/public/lights/*
	touch /etc/default/lights

install-files: install-ui
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f /usr/local/bin/houselights
	cp houselights /usr/local/bin
	chown root:root /usr/local/bin/houselights
	chmod 755 /usr/local/bin/houselights

uninstall-files:
	rm -f /usr/local/bin/houselights
	rm -rf $(SHARE)/public/lights

purge-config:
	rm -rf /etc/house/lights.config /etc/default/lights

# Distribution agnostic systemd support -------------------------

install-systemd:
	cp systemd.service /lib/systemd/system/houselights.service
	chown root:root /lib/systemd/system/houselights.service
	systemctl daemon-reload
	systemctl enable houselights
	systemctl start houselights

uninstall-systemd:
	if [ -e /etc/init.d/houselights ] ; then systemctl stop houselights ; systemctl disable houselights ; rm -f /etc/init.d/houselights ; fi
	if [ -e /lib/systemd/system/houselights.service ] ; then systemctl stop houselights ; systemctl disable houselights ; rm -f /lib/systemd/system/houselights.service ; systemctl daemon-reload ; fi

stop-systemd: uninstall-systemd

# Distribution agnostic runit support ---------------------------

install-runit:
	mkdir -p /etc/sv/houselights
	cp runit.run /etc/sv/houselights/run
	chown root:root /etc/sv/houselights /etc/sv/houselights/run
	chmod 755 /etc/sv/houselights/run
	rm -f /etc/runit/runsvdir/default/houselights
	ln -s /etc/sv/houselights /etc/runit/runsvdir/default/houselights
	/bin/sleep 5
	/usr/bin/sv up houselights

uninstall-runit:
	if [ -e /etc/sv/houselights ] ; then /usr/bin/sv shutdown houselights ; rm -rf /etc/sv/houselights ; rm -f /etc/runit/runsvdir/default/houselights ; /bin/sleep 5 ; fi

stop-runit:
	if [ -e /etc/sv/houselights ] ; then /usr/bin/sv shutdown houselights ; fi

# Debian GNU/Linux install --------------------------------------

install-debian: stop-systemd install-files install-systemd

uninstall-debian: uninstall-systemd uninstall-files

purge-debian: uninstall-debian purge-config

# Devuan GNU/Linux install (using runit) ------------------------

install-devuan: stop-runit install-files install-runit

uninstall-devuan: uninstall-runit uninstall-files

purge-devuan: uninstall-devuan purge-config

# Void Linux install --------------------------------------------

install-void: stop-runit install-files install-runit

uninstall-void: uninstall-runit uninstall-files

purge-void: uninstall-void purge-config

# Default install (Debian GNU/Linux) ----------------------------

install: install-debian

uninstall: uninstall-debian

purge: purge-debian

