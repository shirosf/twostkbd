INSTALL_DIR=/usr/bin/twostkbd
WORK_DIR=/home/$(USER)/twostkbd
IFILES=twostkbd.py create_device.sh config.org
WFILES=Makefile twostkbd.service
install: $(IFILES)
	install -d $(INSTALL_DIR)
	cp --preserve=mode $^ $(INSTALL_DIR)
uninstall:
	rm -Rf $(INSTALL_DIR)
sshcopy:
	scp $(IFILES) $(WFILES) pizero:$(WORK_DIR)
enable_service:
	cp -f twostkbd.service /lib/systemd/system/
	systemctl enable twostkbd.service
start_service:
	systemctl start twostkbd.service
stop_service:
	systemctl stop twostkbd.service
disable_service:
	systemctl disable twostkbd.service
