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

    def get_multikeys_table(self, items):
        if len(items)<4: return 0
        item1=items[1].strip()
        if item1=="": return 0
        if item1=='func': return 0
        multikeys=items[2].strip().split(",")
        v=0
        for mk in multikeys:
            v|=1<<self.btgpios[mk]
        self.multikeystable[v]=item1
        return 0

    def readconf(self, conffile: str="config.org") -> int:
        inf=open(conffile, "r")
        state=0
        self.skeytable=[None]*36
        self.fkeytable=[None]*4
        self.multikeystable={}
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
                if line.find("multikeys table")>0:
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
                if self.get_multikeys_table(items)!=0: return -1
        inf.close()
        return 0

class KbdDevice():
    BOUNCE_GUARD_SEC=0.01
    MULTIKEY_GAP_NS=50000000
    def __init__(self):
        self.config=KbdConfig()
        if self.config.readconf()!=0:
            logger.error("can't read 'config.org'")
            return
        self.buttons={}
        ledgpios={"LED0":17, "LED1":27, "LED2":22}
        self.leds={"LED0":None, "LED1":None, "LED2":None}
        tsns=time.time_ns()
        for l in self.config.btgpios.keys():
            bt=Button(self.config.btgpios[l], bounce_time=KbdDevice.BOUNCE_GUARD_SEC)
            self.buttons[bt]={"kname":l, "ts":time.time_ns(), "state":0, "ontimer":None,
                              "offtimer":None}
            bt.when_pressed=self.on_pressed
            bt.when_released=self.on_released
        for l in self.leds.keys():
            self.leds[l]=LED(ledgpios[l])
        self.multikey_timer=None
        self.multikey_pressedv=0
        self.modkeys={"shift":False, "alt":False, "ctrl":False, "ext":False}
        self.modkeys_lock={"shift":False, "alt":False, "ctrl":False, "ext":False}
        self.leds["LED0"].on()
        self.modifiers={'RightGUI':(1<<7), 'RightAlt':(1<<6), 'RightShift':(1<<5),
                        'RightCtl':(1<<4), 'LeftGui':(1<<3), 'LeftAlt':(1<<2),
                        'LeftShift':(1<<1), 'LeftCtr':(1<<0)}
        self.devicefd=open("/dev/hidg0", "w")
        self.print_skeytable()
        self.keyqueue=[]
        self.last_write=None

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

    def close(self):
        self.devicefd.close()
        for bt in self.buttons.keys():
            bt.close()
        for led in self.leds.values():
            led.close()

    def scancode(self, key: str, nomod: bool = False) -> tuple[int, int]:
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
            'CTLX':(ord('x')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLC':(ord('c')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLV':(ord('v')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLF':(ord('f')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLY':(ord('y')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLZ':(ord('z')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTLG':(ord('g')-ord('a')+0x04,self.modifiers['LeftCtr'],0),
            'CTL/':(0x38,self.modifiers['LeftCtr'],0),
            'ALTX':(ord('x')-ord('a')+0x04,self.modifiers['LeftAlt'],0),
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
        if not nomod:
            if self.modkeys["shift"]:
                mbits|=self.modifiers['LeftShift']
            if self.modkeys["alt"]:
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

    def on_pressed(self, bt) -> None:
        if self.multikey_timer: self.multikey_timer.cancel()
        self.multikey_timer=None
        tsns=time.time_ns()
        self.keyqueue.append((bt,tsns))
        if self.proc_keyqueue(): return
        self.multikey_timer=threading.Timer(KbdDevice.MULTIKEY_GAP_NS/1E9,
                                            self.proc_multikey_timer)
        self.multikey_timer.start()

    def proc_multikey_timer(self):
        tsns=time.time_ns()
        self.keyqueue.append((None,tsns))
        self.proc_keyqueue()

    # return True when the next press must follow after this
    def proc_keyqueue(self) -> bool:
        res=True
        ki=0
        self.holding_2keys=False
        while(len(self.keyqueue)>ki):
            bt,tsns=self.keyqueue[ki]
            if bt==None:
                ki=self.proc_gapnext(ki)
                res=True
                continue
            kname=self.buttons[bt]["kname"]
            if len(kname)>2:
                # pushed key is a mod key
                self.on_mod_pressed(bt, kname)
                self.keyqueue.pop(ki)
                res=True
                continue
            if ki==0 or self.check_possible_multikeys(ki+1)!=0:
                # "queue top" or "more keys can be expected". go next in the queue
                res=False
                ki+=1
                continue
            # process upto this key in the queue.
            self.proc_stroks(ki+1)
            self.holding_2keys=False
            ki=0
            res=True
        return res

    # get this when Timer expired "None key" come to the queue top.
    # it means the time gap exists keys beofre this "None key"
    def proc_gapnext(self, ki: int):
        self.keyqueue.pop(ki)
        if ki==2:
            dts=self.keyqueue[1][1]-self.keyqueue[0][1]
            if dts<KbdDevice.MULTIKEY_GAP_NS:
                if self.check_possible_multikeys(ki)!=0:
                    # holding 2 keys, 3rd key may be coming. Wait a next key.
                    self.holding_2keys=True
                    return ki
        if ki>1:
            self.proc_stroks(ki) # multiple keys with (0 to ki-1), pop inside the function
            self.holding_2keys=False
            return 0
        if ki==1:
            bt0=self.keyqueue[0][0]
            if self.buttons[bt0]["kname"][0]=="f":
                self.proc_stroks(ki) # process a func key
                return 0
        return ki

    def check_possible_multikeys(self, ki: int, exact=False) -> int:
        if ki==0:
            if self.keyqueue[0][0]["kname"][0]=="k": return 1
            # it must be "f" key
            return 0
        # check with multikeystable
        mkbits=0
        for i,(bt,tsns) in enumerate(self.keyqueue):
            if i==ki: break
            if bt==None: continue
            kname=self.buttons[bt]["kname"]
            kbit=1<<self.config.btgpios[kname]
            if ki==2 and mkbits==kbit:
                # the same key twice
                return 0
            mkbits|=kbit
        for mkv in self.config.multikeystable.keys():
            if exact:
                if mkv == mkbits: return mkv
            else:
                if (mkbits & mkv) == mkbits: return 1
        return 0

    def proc_stroks(self, ki: int) -> None:
        rk0=None
        if ki>2:
            mkv=self.check_possible_multikeys(ki, exact=True)
            if not mkv:
                for i in range(ki): self.keyqueue.pop(0)
                self.multikey_pressedv=0
                return
            self.on_multikeys_pressed(mkv)
            return
        or_pressed=False
        for i in range(ki):
            bt, tsns=self.keyqueue.pop(0)
            or_pressed|=bt.is_pressed
            kname=self.buttons[bt]["kname"]
            if kname[0]=="f":
                if rk0!=None:
                    # "f" comes after the first "k". It is a cancel signal
                    rk0=None
                    continue
                self.on_fkey_pressed(bt, kname)
                if not or_pressed: self.clear_devicefd()
                continue
            if kname[0]=="k":
                if rk0!=None:
                    self.on_rkey_pressed(rk0, bt)
                    if not or_pressed: self.clear_devicefd()
                    rk0=None
                    continue
                rk0=bt
                continue

    def on_mod_pressed(self, bt, kname: str) -> None:
        if self.modkeys_lock[kname]:
            self.modkeys_lock[kname]=False
            return
        self.modkeys_lock[kname]=True
        self.leds["LED1"].on()
        self.modkeys[kname]=True
        self.hidevent_pressed(kname, "mod")

    def on_fkey_pressed(self, bt, kname: str) -> None:
        self.hidevent_pressed(kname, "func")

    def on_rkey_pressed(self, bt0, bt) -> None:
        kname0=self.buttons[bt0]["kname"]
        kname=self.buttons[bt]["kname"]
        self.hidevent_pressed(kname, "reg", kname0)

    def on_multikeys_pressed(self, mkv:int) -> None:
        kname=self.config.multikeystable[mkv]
        self.hidevent_pressed(kname, "mult")
        self.multikey_pressedv=mkv

    def hidevent_pressed(self, kname: str, mode: str, kname0: str = 0):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        if mode=="reg":
            inkey=self.inkey_rk(kname0[1], kname[1])
            self.modkeys_unlock()
        elif mode=="mod":
            inkey=self.scancode('')
        elif mode=="func":
            inkey=self.inkey_fkey(kname[1])
            self.modkeys_unlock()
        elif mode=="mult":
            # only "TAB" uses modifiers
            nomod=False if kname=="TAB" else True
            inkey=self.scancode(kname, nomod=nomod)
            self.modkeys_unlock()
        else:
            return
        if inkey==None: return
        scode[0]=inkey[1]
        scode[2]=inkey[0]
        self.last_write=scode.decode("utf-8")
        self.devicefd.write(self.last_write)
        self.devicefd.flush()

    def inkey_rk(self, kn1, kn2):
        for i in range(36):
            sktable=self.config.skeytable[i]
            if sktable["1st"]!=kn1 or \
               sktable["2nd"]!=kn2:
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

    def inkey_fkey(self, kn):
        fktable=self.config.fkeytable[int(kn)]
        nomod=False
        for mn in KbdConfig.mnames:
            if self.modkeys[mn]:
                fk=fktable["mfuncs"][mn]
                if fk:
                    nomod=True
                    break
        else:
            fk=fktable["func"]
        if not fk: return None
        logger.debug("press %s,%d,%d,%d,%d" % (fk,
                                               self.modkeys["shift"],self.modkeys["alt"],
                                               self.modkeys["ctrl"],self.modkeys["ext"]))
        return self.scancode(fk, nomod=nomod)

    def all_modkeys_off(self) -> bool:
        return not (self.modkeys["shift"] or self.modkeys["alt"] or \
                    self.modkeys["ctrl"] or self.modkeys["ext"])

    def modkeys_unlock(self) -> None:
        for bt,btv in self.buttons.items():
            if btv["kname"][0]=='k' or btv["kname"][0]=='f': continue
            if self.modkeys_lock[btv["kname"]]:
                self.modkeys_lock[btv["kname"]]=False
                if not bt.is_pressed:
                    self.modkeys[btv["kname"]]=False
                    if self.all_modkeys_off(): self.leds["LED1"].off()

    def on_released(self, bt) -> None:
        kname=self.buttons[bt]["kname"]
        if self.multikey_pressedv!=0:
            self.clear_multikey_in_queue(kname)
            return self.clear_devicefd()
        if len(kname)>2:
            # release a mod key
            if self.modkeys_lock[kname]: return
            self.modkeys_unlock()
            self.modkeys[kname]=False
            if self.all_modkeys_off(): self.leds["LED1"].off()
            return self.clear_devicefd()
        if kname[0]=="k":
            # release a regular key
            if self.holding_2keys:
                self.proc_stroks(2)
                self.holding_2keys=False
        return self.clear_devicefd()

    def clear_multikey_in_queue(self, kname) -> None:
        for (bt,tsns) in self.keyqueue:
            qkname=self.buttons[bt]["kname"]
            if kname==qkname:
                self.keyqueue.remove((bt,tsns))
                mkbit=1<<self.config.btgpios[kname]
                self.multikey_pressedv&=~mkbit
                return

    def clear_devicefd(self):
        scode=bytearray(b"\0\0\0\0\0\0\0\0")
        inkey_m=self.scancode('')
        scode[0]=inkey_m[1]
        nwd=scode.decode("utf-8")
        if self.last_write!=nwd:
            self.last_write=nwd
            self.devicefd.write(self.last_write)
            self.devicefd.flush()

if __name__ == '__main__':
    kbd = KbdDevice()
    while True:
        try:
            threading.Event().wait()
        except:
            break
    kbd.close()
