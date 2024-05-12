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

logger=logging.getLogger('twostkbd')
logger.setLevel(logging.DEBUG)

class KbdConfig():
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
            mkeys=[""]*4
            for i in range(4):
                mkeys[i]=items[5+i].strip()
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
            mfuncs=[""]*4
            for i in range(4):
                mfuncs[i]=items[3+i].strip()
        except:
            logger.error("invalid value in fkey table")
            return -1
        self.fkeytable[index]={"func":func, "mfuncs":mfuncs}
        return 0

    def readconf(self, conffile: str="config.org") -> int:
        inf=open(conffile, "r")
        state=0
        self.skeytable=[None]*36
        self.fkeytable=[None]*4
        while True:
            keydef={}
            line=inf.readline()
            if line=='': break
            if state==0:
                if line.find("6-key table")>0:
                    state=1
                if line.find("fkey table")>0:
                    state=2
                continue
            if line[0]!='|':
                state=0
                continue
            items=line.split('|')
            if state==1:
                if self.get_skey_table(items)!=0: return -1
            if state==2:
                if self.get_fkey_table(items)!=0: return -1

        inf.close()
        return 0

class KbdDevice():
    CHATTERING_GUARD_NS=40000000
    def __init__(self):
        self.config=KbdConfig()
        if self.config.readconf()!=0:
            logger.error("can't read 'config.org'")
            return
        btgpios={"SW0":23, "SW1":25, "SW2":7, "SW3":21, "SW4":16, "SW5":26,
                 "SF0":24, "SF1":8, "SF2":12, "SF3":20,
                 "SM0":15, "SM1":18, "SM2":14, "SM3":4}
        self.buttons={}
        ledgpios={"LED0":17, "LED1":27, "LED2":22}
        self.leds={"LED0":None, "LED1":None, "LED2":None}
        for l in btgpios.keys():
            bt=Button(btgpios[l])
            self.buttons[bt]={"name":l, "ts":time.time_ns(), "state":0}
            bt.when_pressed=self.on_pressed
            bt.when_released=self.on_released
        for l in self.leds.keys():
            self.leds[l]=LED(ledgpios[l])
        self.firstkey=None
        self.secondkey=None
        self.modkeys=[False, False, False, False]
        self.leds["LED2"].on()
        self.modifiers={'RightGUI':(1<<7), 'RightAlt':(1<<6), 'RightShift':(1<<5),
                        'RightCtl':(1<<4), 'LeftGui':(1<<3), 'LeftAlt':(1<<2),
                        'LeftShift':(1<<1), 'LeftCtr':(1<<0)}
        self.devicefd=open("/dev/hidg0", "w")
        print("start")

    def close(self):
        self.devicefd.close()
        for bt in self.buttons.keys():
            bt.close()
        for led in self.leds.values():
            led.close()

    def scancode(self, key: str) -> tuple[int, int]:
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
        if self.modkeys[0]:
            mbits|=self.modifiers['LeftShift']
        if self.modkeys[1]:
            mbits|=self.modifiers['LeftAlt']
        if self.modkeys[2]:
            mbits|=self.modifiers['LeftCtr']

        if len(key)==1:
            if key>='a' and key<='z':
                return (ord(key)-ord('a')+0x04, mbits);
            if key>='1' and key<='9':
                return (ord(key)-ord('1')+0x1e, mbits);
        mbits|=scodes[key][1]
        mbits&=~scodes[key][2]
        return (scodes[key][0], mbits)

    def on_pressed(self, bt) -> None:
        if time.time_ns()-self.buttons[bt]["ts"]<KbdDevice.CHATTERING_GUARD_NS: return
        self.buttons[bt]["ts"]=time.time_ns()
        self.buttons[bt]["state"]=1
        kname=self.buttons[bt]["name"]
        if kname[1]=='W':
            if self.firstkey==None:
                self.firstkey=bt
            elif self.secondkey==None:
                self.secondkey=bt
                self.hidevent_pressed(kname[2], fkey=False)
            else:
                logger.error("ignore 3rd key press:%s" % kname)
        elif kname[1]=='F':
            self.firstkey=None
            self.secondkey=None
            self.hidevent_pressed(kname[2], fkey=True)
        else:
            # kname="SM0" to "SM3"
            self.modkeys[ord(kname[2])-ord('0')]=True

    def on_released(self, bt) -> None:
        if time.time_ns()-self.buttons[bt]["ts"]<KbdDevice.CHATTERING_GUARD_NS: return
        self.buttons[bt]["ts"]=time.time_ns()
        self.buttons[bt]["state"]=0
        kname=self.buttons[bt]["name"]
        if kname[1]=='W':
            if self.secondkey==bt:
                self.hidevent_released(kname[2], fkey=False)
                self.firstkey=None
                self.secondkey=None
        elif kname[1]=='F':
            self.hidevent_released(kname[2], fkey=True)
        else:
            # modifier key is released
            self.modkeys[ord(kname[2])-ord('0')]=False

    def hidevent_pressed(self, kn, fkey):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        if fkey:
            fktable=self.config.fkeytable[int(kn)]
            inkey=self.scancode(fktable["func"])
            logger.debug("press %s,%d,%d,%d,%d" % (fktable["func"],
                                                   self.modkeys[0],self.modkeys[1],
                                                   self.modkeys[2],self.modkeys[3]))
        else:
            for i in range(36):
                sktable=self.config.skeytable[i]
                if sktable["1st"]!=self.buttons[self.firstkey]["name"][2] or \
                   sktable["2nd"]!=kn:
                    continue
                if self.modkeys[3] and sktable["mkeys"][3]:
                    inkey=self.scancode(sktable["mkeys"][3])
                elif self.modkeys[0] and sktable["mkeys"][0]:
                    inkey=self.scancode(sktable["mkeys"][0])
                else:
                    inkey=self.scancode(sktable["key"])
                logger.debug("press %s,%d,%d,%d,%d" % (sktable["key"],
                                                       self.modkeys[0],self.modkeys[1],
                                                       self.modkeys[2],self.modkeys[3]))
                break
            else:
                return
        scode[0]=inkey[1]
        scode[2]=inkey[0]
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

    def hidevent_released(self, kn, fkey):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        if fkey:
            fktable=self.config.fkeytable[int(kn)]
            logger.debug("release %s,%d,%d,%d,%d" % (fktable["func"],
                                                     self.modkeys[0],self.modkeys[1],
                                                     self.modkeys[2],self.modkeys[3]))
        else:
            for i in range(36):
                sktable=self.config.skeytable[i]
                if sktable["1st"]!=self.buttons[self.firstkey]["name"][2] or \
                   sktable["2nd"]!=kn:
                    continue
                logger.debug("release %s,%d,%d,%d,%d" % (sktable["key"],
                                                         self.modkeys[0],self.modkeys[1],
                                                         self.modkeys[2],self.modkeys[3]))
                break;
            else:
                return
        self.devicefd.write(scode.decode("utf-8"))
        self.devicefd.flush()

if __name__ == '__main__':
    kbd = KbdDevice()
    while True:
        s=input()
        if s[0]=='q': break
    kbd.close()