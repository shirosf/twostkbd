#!/bin/bash

hid_descriptor()
{
    local wdata=""
    wdata+="\x05\x01" #	/* USAGE_PAGE (Generic Desktop) */
    wdata+="\x09\x06" #	/* USAGE (Keyboard) */
    wdata+="\xa1\x01" #	/* COLLECTION (Application) */
    wdata+="\x05\x07" #	/* USAGE_PAGE(KeyCodes) */
    wdata+="\x19\xE0" #	/* USAGE_MINIMUM (224) */
    wdata+="\x29\xE7" #	/* USAGE_MAXIMUM (231) */
    wdata+="\x15\x00" #	/* LOGICAL_MINIMUM (0) */
    wdata+="\x25\x01" #	/* LOGICAL_MAXIMUM (1) */
    wdata+="\x75\x01" #	/* REPORT_SIZE (1) */
    wdata+="\x95\x08" #	/* REPORT_COUNT (8) */
    wdata+="\x81\x02" #	/* Input (Data,Variable,Absolute);Modifier byte */
    wdata+="\x95\x01" #	/* REPORT_COUNT (1) */
    wdata+="\x75\x08" #	/* REPORT_SIZE (8) */
    wdata+="\x81\x01" #	/* Input (Constant);Reserved byte */
    wdata+="\x95\x05" #	/* REPORT_COUNT (5) */
    wdata+="\x75\x01" #	/* REPORT_SIZE (1) */
    wdata+="\x05\x08" #	/* USAGE_PAGE(for LEDs) */
    wdata+="\x19\x01" #	/* USAGE_MINIMUM (1) */
    wdata+="\x29\x05" #	/* USAGE_MAXIMUM (5) */
    wdata+="\x91\x02" #	/* Output (Data,Var,Abs);LED report */
    wdata+="\x95\x01" #	/* REPORT_COUNT (1) */
    wdata+="\x75\x03" #	/* REPORT_SIZE (3) */
    wdata+="\x91\x01" #	/* Output (Constant);LED report padding */
    wdata+="\x95\x06" #	/* REPORT_COUNT (6) */
    wdata+="\x75\x08" #	/* REPORT_SIZE (8) */
    wdata+="\x15\x00" #	/* LOGICAL_MINIMUM (0) */
    wdata+="\x25\x65" #	/* USAGE_MAXIMUM (101) */
    wdata+="\x05\x07" #	/* USAGE_PAGE(Key Codes) */
    wdata+="\x19\x00" #	/* USAGE_MINIMUM (0) */
    wdata+="\x29\x65" #	/* USAGE_MAXIMUM (101) */
    wdata+="\x81\x00" #	/* Input (Data, Array); Key array(6 bytes) */
    wdata+="\xc0"     #	/* END_COLLECTION */
    echo -n -e ${wdata}
}

if [ ! -d /sys/kernel/config/usb_gadget ]; then
    sudo modprobe dwc2
    sudo modprobe libcomposite
fi
if [ ! -d /sys/kernel/config/usb_gadget ]; then
    echo "no \"/sys/kernel/config/usb_gadget\""
    exit 1
fi
GAD_DEVNAME=twstkbd
GAD_DEVDIR=/sys/kernel/config/usb_gadget/${GAD_DEVNAME}
GAD_STRINGS_LANG="strings/0x409" # en-US is 0x409
GAD_CONFIGS_LANG="configs/c.1/strings/0x409"
mkdir ${GAD_DEVDIR}
pushd .
if ! cd ${GAD_DEVDIR}; then
    popd
    exit 1
fi
echo 0x1d6b > idVendor # Linux Foundation
echo 0x0104 > idProduct # Multifunction Composite Gadget
echo 0x0100 > bcdDevice # v1.0.0
echo 0x0200 > bcdUSB # USB2
mkdir -p ${GAD_STRINGS_LANG}
echo "20240511" > ${GAD_STRINGS_LANG}/serialnumber
echo "snino" > ${GAD_STRINGS_LANG}/manufacturer
echo "two strokes left hand keyboard" > ${GAD_STRINGS_LANG}/product

mkdir -p ${GAD_CONFIGS_LANG}
echo 250 > configs/c.1/MaxPower
mkdir -p functions/hid.usb0
echo 1 > functions/hid.usb0/protocol
echo 1 > functions/hid.usb0/subclass
echo 8 > functions/hid.usb0/report_length
hid_descriptor > functions/hid.usb0/report_desc
ln -s functions/hid.usb0 configs/c.1/

ls /sys/class/udc > UDC
chmod 666 /dev/hidg0
popd
