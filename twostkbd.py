#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (C) 2024 Shiro Ninomiya
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see
# <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
#
import logging
from gpiozero import Button, LED
import time
import threading

logger=logging.getLogger('twostkbd')
logger.setLevel(logging.DEBUG)

class KbdConfig():
    flabels=("alt","ctrl","shift","ext","k0","k1","k2","k3","k4","k5","f0","f1","f2","f3")
    mnames=("shift", "alt", "ctrl", "ext")
    def get_keygpio_table(self, items):
        if len(items)<5: return 0
        try:
            kname=items[3].strip()
            gpn=int(items[2])
            if gpn<0 or gpn>27: raise ValueError
            self.btgpios[kname]=gpn
        except ValueError:
            logger.error("gpio msut be a number in 0 to 27")
            return -1
        return 0

    def get_skey_table(self, items):
        if len(items)<10: return 0
        try:
            item1=items[1].strip()
            if item1=="": return 0
            if item1=='No.': return 0
            index=int(item1)
            if index<0 or index>35: raise ValueError
        except ValueError:
            logger.error("'No.' item msut be a number in 0 to 35")
            return -1
        try:
            key=items[2].strip()
            firstkey=items[3].strip()
            secondkey=items[4].strip()
            mkeys={}
            for i in range(4):
                mkeys[self.mnames[i]]=items[5+i].strip()
        except:
            logger.error("invalid value in 6-key table")
            return -1
        self.skeytable[index]={"key":key, "1st":firstkey, "2nd":secondkey, "mkeys":mkeys}
        return 0

    def get_fkey_table(self, items):
        if len(items)<8: return 0
        try:
            item1=items[1].strip()
            if item1=="": return 0
            if item1=='f.key': return 0
            index=int(item1)
            if index<0 or index>3: raise ValueError
        except ValueError:
            logger.error("'f.key' item msut be a number in 0 to 3")
            return -1
        try:
            func=items[2].strip()
            mfuncs={}
            for i in range(4):
                mfuncs[self.mnames[i]]=items[3+i].strip()
        except:
            logger.error("invalid value in fkey table")
            return -1
        self.fkeytable[index]={"func":func, "mfuncs":mfuncs}
        return 0

    def get_fmode_table(self, items):
        if len(items)<16: return 0
        item1=items[1].strip()
        if (item1!="0") and (item1!="1"):return 0
        item1=int(item1)
        for i in range(13):
            self.fmodetable[item1][self.flabels[i+1]]=items[i+2].strip()
        return 0

    def readconf(self, conffile: str="config.org") -> int:
        inf=open(conffile, "r")
        state=0
        self.skeytable=[None]*36
        self.fkeytable=[None]*4
        self.fmodetable=[{},{}]
        self.btgpios={}
        while True:
            keydef={}
            line=inf.readline()
            if line=='': break
            if state==0:
                if line.find("key-gpio table")>0:
                    state=1
                if line.find("6-key table")>0:
                    state=2
                if line.find("fkey table")>0:
                    state=3
                if line.find("fmode table")>0:
                    state=4
                continue
            if line[0]!='|':
                state=0
                continue
            items=line.split('|')
            if state==1:
                if self.get_keygpio_table(items)!=0: return -1
            if state==2:
                if self.get_skey_table(items)!=0: return -1
            if state==3:
                if self.get_fkey_table(items)!=0: return -1
            if state==4:
                if self.get_fmode_table(items)!=0: return -1
        inf.close()
        return 0

class KbdDevice():
    CHATTERING_GUARD_NS=30000000
    FMODE_SWITCHING_GUARD_NS=500000000
    def __init__(self):
        self.config=KbdConfig()
        if self.config.readconf()!=0:
            logger.error("can't read 'config.org'")
            return
        self.buttons={}
        ledgpios={"LED0":17, "LED1":27, "LED2":22}
        self.leds={"LED0":None, "LED1":None, "LED2":None}
        self.fmode_switching_ts=time.time_ns()
        for l in self.config.btgpios.keys():
            bt=Button(self.config.btgpios[l])
            self.buttons[bt]={"kname":l, "ts":time.time_ns(), "state":0, "ontimer":None}
            bt.when_pressed=self.on_pressed
            bt.when_released=self.on_released
        for l in self.leds.keys():
            self.leds[l]=LED(ledgpios[l])
        self.firstkey=None
        self.secondkey=None
        self.modkeys={"shift":False, "alt":False, "ctrl":False, "ext":False}
        self.leds["LED0"].on()
        self.modifiers={'RightGUI':(1<<7), 'RightAlt':(1<<6), 'RightShift':(1<<5),
                        'RightCtl':(1<<4), 'LeftGui':(1<<3), 'LeftAlt':(1<<2),
                        'LeftShift':(1<<1), 'LeftCtr':(1<<0)}
        self.devicefd=open("/dev/hidg0", "w")
        self.print_skeytable()

    def print_skeytable(self):
        fhelp={"0":"r ","1":"m ", "2":"i ", "3":"t ", "4":"ir", "5":"tr"}
        for sk in self.config.skeytable:
            skm0=sk["mkeys"]["shift"]
            if not skm0:
                skm0=sk["key"].upper()
            elif skm0=="VBAR":
                skm0="|"
            print("%s(%s)-%s(%s): %s, %s, %s" % ( sk["1st"], fhelp[sk["1st"]],
                                                  sk["2nd"], fhelp[sk["2nd"]],
                                                  sk["key"], skm0, sk["mkeys"]["ext"]))

    def print_fmodetable(self):
        for fl in self.config.flabels[1:]:
            print("%06s" % fl, end='')
        print()
        for fl in self.config.flabels[1:]:
            print("%06s" % self.config.fmodetable[0][fl], end='')
        print()
        for fl in self.config.flabels[1:]:
            print("%06s" % self.config.fmodetable[1][fl], end='')
        print()

    def close(self):
        self.devicefd.close()
        for bt in self.buttons.keys():
            bt.close()
        for led in self.leds.values():
            led.close()

    def scancode(self, key: str, noshift: bool = False,
                 noalt: bool = False) -> tuple[int, int]:
        scodes={
            '0':(0x27,0,0),
            'RET':(0x28,0,0),
            'ESC':(0x29,0,0),
            'BS':(0x2a,0,0),
            'TAB':(0x2b,0,0),
            'SP':(0x2c,0,0),
            '-':(0x2d,0,0),
            '=':(0x2e,0,0),
            '[':(0x2f,0,0),
            ']':(0x30,0,0),
            '\\':(0x31,0,0),
            ';':(0x33,0,0),
            "'":(0x34,0,0),
            '`':(0x35,0,0),
            ',':(0x36,0,0),
            '.':(0x37,0,0),
            '/':(0x38,0,0),
            'F1':(0x3a,0,0),
            'F2':(0x3b,0,0),
            'F3':(0x3c,0,0),
            'F4':(0x3d,0,0),
            'F5':(0x3e,0,0),
            'F6':(0x3f,0,0),
            'F7':(0x40,0,0),
            'F8':(0x41,0,0),
            'F9':(0x42,0,0),
            'F10':(0x43,0,0),
            'F11':(0x44,0,0),
            'F12':(0x45,0,0),
            'HOME':(0x4a,0,self.modifiers['LeftCtr']),
            'PUP':(0x4b,0,self.modifiers['LeftAlt']),
            'DEL':(0x4c,0,self.modifiers['LeftCtr']),
            'CSDEL':(0x4c,self.modifiers['LeftShift']|self.modifiers['LeftCtr'],0),
            'END':(0x4d,0,self.modifiers['LeftCtr']),
            'PDOWN':(0x4e,0,self.modifiers['LeftCtr']),
            'RIGHT':(0x4f,0,self.modifiers['LeftCtr']),
            'CRIGHT':(0x4f,self.modifiers['LeftCtr'],self.modifiers['LeftAlt']),
            'LEFT':(0x50,0,self.modifiers['LeftCtr']),
            'CLEFT':(0x50,self.modifiers['LeftCtr'],self.modifiers['LeftAlt']),
            'DOWN':(0x51,0,self.modifiers['LeftCtr']),
            'UP':(0x52,0,self.modifiers['LeftCtr']),
            'COPY':(ord('c')-ord('a')+4,self.modifiers['LeftCtr'],0),
            'PASTE':(ord('v')-ord('a')+4,self.modifiers['LeftCtr'],0),
            'CHOME':(0x4a,self.modifiers['LeftCtr'],0),
            'CEND':(0x4d,self.modifiers['LeftCtr'],0),
            'CTAB':(0x2b,self.modifiers['LeftCtr'],0),
            'SSP':(0x2c,self.modifiers['LeftShift'],0),
            'SRET':(0x28,self.modifiers['LeftShift'],0),
            'SBS':(0x2a,self.modifiers['LeftShift'],0),
            'STAB':(0x2b,self.modifiers['LeftShift'],0),
            '!':(0x1e,self.modifiers['LeftShift'],0),
            '@':(0x1f,self.modifiers['LeftShift'],0),
            '#':(0x20,self.modifiers['LeftShift'],0),
            '$':(0x21,self.modifiers['LeftShift'],0),
            '%':(0x22,self.modifiers['LeftShift'],0),
            '^':(0x23,self.modifiers['LeftShift'],0),
            '*':(0x25,self.modifiers['LeftShift'],0),
            '&':(0x24,self.modifiers['LeftShift'],0),
            '(':(0x26,self.modifiers['LeftShift'],0),
            ')':(0x27,self.modifiers['LeftShift'],0),
            '_':(0x2d,self.modifiers['LeftShift'],0),
            '+':(0x2e,self.modifiers['LeftShift'],0),
            '{':(0x2f,self.modifiers['LeftShift'],0),
            '}':(0x30,self.modifiers['LeftShift'],0),
            'VBAR':(0x32,self.modifiers['LeftShift'],0),
            ':':(0x33,self.modifiers['LeftShift'],0),
            '"':(0x34,self.modifiers['LeftShift'],0),
            '~':(0x35,self.modifiers['LeftShift'],0),
            '<':(0x36,self.modifiers['LeftShift'],0),
            '>':(0x37,self.modifiers['LeftShift'],0),
            '?':(0x38,self.modifiers['LeftShift'],0),
        }

        mbits=0
        if not noshift and self.modkeys["shift"]:
            mbits|=self.modifiers['LeftShift']
        if not noalt and self.modkeys["alt"]:
            mbits|=self.modifiers['LeftAlt']
        if self.modkeys["ctrl"]:
            mbits|=self.modifiers['LeftCtr']

        if len(key)==1:
            if key>='a' and key<='z':
                return (ord(key)-ord('a')+0x04, mbits)
            if key>='1' and key<='9':
                return (ord(key)-ord('1')+0x1e, mbits)
        elif len(key)==0:
            return (0, mbits)

        mbits|=scodes[key][1]
        mbits&=~scodes[key][2]
        return (scodes[key][0], mbits)

    def check_fmode_switch(self, tsns: int) -> bool:
        if tsns-self.fmode_switching_ts<KbdDevice.FMODE_SWITCHING_GUARD_NS: return True
        ckeys=("shift", "k0", "k1", "k2", "k3")
        for bt,btv in self.buttons.items():
            if (btv["kname"] in ckeys) and not bt.is_pressed: return False
        for bt in self.buttons.keys():
            if self.buttons[bt]["ontimer"]!=None:
                self.buttons[bt]["ontimer"].cancel()
                self.buttons[bt]["ontimer"]=None

        if not self.leds["LED2"].is_lit:
            self.leds["LED2"].on()
            self.print_fmodetable()
        else:
            self.leds["LED2"].off()
            self.print_skeytable()
        self.leds["LED1"].off()
        self.secondkey=None
        self.firstkey=None
        for i in self.modkeys.keys(): self.modkeys[i]=False
        self.fmode_switching_ts=tsns
        return True

    def defered_on_pressed(self, bt) -> None:
        self.buttons[bt]["ontimer"]=None
        if not bt.is_pressed: return
        self.buttons[bt]["state"]=1
        kname=self.buttons[bt]["kname"]
        if not self.leds["LED2"].is_lit:
            self.on_pressed_rmode(bt, kname)
        else:
            self.on_pressed_fmode(bt, kname)

    def on_pressed(self, bt) -> None:
        tsns=time.time_ns()
        if self.check_fmode_switch(tsns): return
        if tsns-self.buttons[bt]["ts"]<KbdDevice.CHATTERING_GUARD_NS:
            self.buttons[bt]["ts"]=tsns
            return
        #if self.buttons[bt]["ontimer"]!=None: return
        self.buttons[bt]["ts"]=tsns
        self.buttons[bt]["ontimer"]=threading.Timer(KbdDevice.CHATTERING_GUARD_NS/1E9,
                                                    self.defered_on_pressed, args=(bt,))
        self.buttons[bt]["ontimer"].start()

    def on_pressed_fmode(self, bt, kname) -> None:
        if kname=="alt":
            self.modkeys[kname]=True
            return
        i=1 if self.modkeys["alt"] else 0
        fmfunc=self.config.fmodetable[i][kname]
        if not fmfunc: return
        altbk=self.modkeys["alt"]
        self.modkeys["alt"]=False
        inkey=self.scancode(fmfunc)
        self.modkeys["alt"]=altbk
        logger.debug("press %s/%d" % (fmfunc,self.modkeys["alt"]))
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        scode[0]=inkey[1]
        scode[2]=inkey[0]
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

    def on_pressed_rmode(self, bt, kname) -> None:
        if kname[0]=='k':
            if self.firstkey==None:
                self.firstkey=bt
                self.leds["LED1"].on()
            elif self.secondkey==None:
                #if self.check_fmode_switch(kname): return
                self.secondkey=bt
                self.hidevent_pressed(kname[1], fkey=False)
            else:
                logger.debug("ignore 3rd key press:%s" % kname)

        elif kname[0]=='f':
            self.secondkey=None
            if self.firstkey==None:
                self.hidevent_pressed(kname[1], fkey=True)
            self.firstkey=None
            self.leds["LED1"].off()
        else:
            # modifier key
            self.modkeys[kname]=True
            self.hidevent_pressed('', fkey=False)

    def on_released(self, bt) -> None:
        tsns=time.time_ns()
        if tsns-self.fmode_switching_ts<KbdDevice.FMODE_SWITCHING_GUARD_NS: return
        if tsns-self.buttons[bt]["ts"]<KbdDevice.CHATTERING_GUARD_NS: return
        if self.buttons[bt]["state"]!=1: return
        self.buttons[bt]["ts"]=tsns
        self.buttons[bt]["state"]=0
        kname=self.buttons[bt]["kname"]
        if not self.leds["LED2"].is_lit:
            self.on_released_rmode(bt, kname)
        else:
            self.on_released_fmode(bt, kname)

    def on_released_fmode(self, bt, kname) -> None:
        if kname=="alt":
            self.modkeys[kname]=False
            return
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

    def on_released_rmode(self, bt, kname) -> None:
        if kname[0]=='k':
            if self.secondkey==bt:
                self.hidevent_released(kname[1], fkey=False)
                self.firstkey=None
                self.leds["LED1"].off()
                self.secondkey=None
        elif kname[0]=='f':
            self.hidevent_released(kname[1], fkey=True)
        else:
            # modifier key is released
            self.modkeys[kname]=False
            self.hidevent_released('', fkey=False)

    def inkey_fkey(self, kn):
        fktable=self.config.fkeytable[int(kn)]
        for mn in KbdConfig.mnames:
            if self.modkeys[mn]:
                fk=fktable["mfuncs"][mn]
                if fk: break
        else:
            fk=fktable["func"]
        if not fk: return None
        logger.debug("press %s,%d,%d,%d,%d" % (fk,
                                               self.modkeys["shift"],self.modkeys["alt"],
                                               self.modkeys["ctrl"],self.modkeys["ext"]))
        return self.scancode(fk, noshift=True, noalt=True)

    def inkey_sk(self, kn):
        for i in range(36):
            sktable=self.config.skeytable[i]
            if sktable["1st"]!=self.buttons[self.firstkey]["kname"][1] or \
               sktable["2nd"]!=kn:
                continue
            logger.debug("press %s,%d,%d,%d,%d" % (sktable["key"],
                                                   self.modkeys["shift"],self.modkeys["alt"],
                                                   self.modkeys["ctrl"],self.modkeys["ext"]))
            if self.modkeys["ext"] and sktable["mkeys"]["ext"]:
                return self.scancode(sktable["mkeys"]["ext"])
            elif self.modkeys["shift"] and sktable["mkeys"]["shift"]:
                return self.scancode(sktable["mkeys"]["shift"])
            else:
                return self.scancode(sktable["key"])
        else:
            return None

    def hidevent_pressed(self, kn, fkey):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        if kn=='':
            inkey=self.scancode(kn)
        elif fkey:
            inkey=self.inkey_fkey(kn)
        else:
            inkey=self.inkey_sk(kn)
        if inkey==None: return
        scode[0]=inkey[1]
        scode[2]=inkey[0]
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

    def hidevent_released(self, kn, fkey):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        if kn=='':
            pass
        elif fkey:
            fktable=self.config.fkeytable[int(kn)]
            logger.debug("release %s,%d,%d,%d,%d" % (fktable["func"],
                                                     self.modkeys["shift"],self.modkeys["alt"],
                                                     self.modkeys["ctrl"],self.modkeys["ext"]))
        else:
            for i in range(36):
                sktable=self.config.skeytable[i]
                if sktable["1st"]!=self.buttons[self.firstkey]["kname"][1] or \
                   sktable["2nd"]!=kn:
                    continue
                logger.debug("release %s,%d,%d,%d,%d" % (sktable["key"],
                                                         self.modkeys["shift"],self.modkeys["alt"],
                                                         self.modkeys["ctrl"],self.modkeys["ext"]))
                break;
            else:
                return
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

if __name__ == '__main__':
    kbd = KbdDevice()
    while True:
        try:
            threading.Event().wait()
        except:
            break
    kbd.close()
