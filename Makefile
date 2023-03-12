
OBJS= houselights_plugs.o houselights_schedule.o houselights.o
LIBOJS=

SHARE=/usr/local/share/house

all: houselights

clean:
	rm -f *.o *.a houselights

rebuild: clean all

%.o: %.c
	gcc -c -Os -o $@ $<

houselights: $(OBJS)
	gcc -Os -o houselights $(OBJS) -lhouseportal -lechttp -lssl -lcrypto -lgpiod -lrt

install:
	if [ -e /etc/init.d/houselights ] ; then systemctl stop houselights ; systemctl disable houselights ; rm -f /etc/init.d/houselights ; fi
	if [ -e /lib/systemd/system/houselights.service ] ; then systemctl stop houselights ; systemctl disable houselights ; rm -f /lib/systemd/system/houselights.service ; fi
	mkdir -p /usr/local/bin
	mkdir -p /var/lib/house
	mkdir -p /etc/house
	rm -f /usr/local/bin/houselights
	cp houselights /usr/local/bin
	chown root:root /usr/local/bin/houselights
	chmod 755 /usr/local/bin/houselights
	cp systemd.service /lib/systemd/system/houselights.service
	chown root:root /lib/systemd/system/houselights.service
	mkdir -p $(SHARE)/public/lights
	chmod 755 $(SHARE) $(SHARE)/public $(SHARE)/public/lights
	cp public/* $(SHARE)/public/lights
	chown root:root $(SHARE)/public/lights/*
	chmod 644 $(SHARE)/public/lights/*
	touch /etc/default/lights
	systemctl daemon-reload
	systemctl enable houselights
	systemctl start houselights

uninstall:
	systemctl stop houselights
	systemctl disable houselights
	rm -f /usr/local/bin/houselights
	rm -f /lib/systemd/system/houselights.service /etc/init.d/houselights
	rm -rf $(SHARE)/public/lights
	systemctl daemon-reload

purge: uninstall
	rm -rf /etc/house/lights.config /etc/default/lights

