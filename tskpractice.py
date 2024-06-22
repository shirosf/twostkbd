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
import argparse
import sys
import random
import time
import termios

class ConsoleKeyIn():
    def __init__(self, inf=None):
        if inf:
            self.inf=inf
        else:
            self.inf=sys.stdin
        fd=self.inf.fileno()
        self.sattr=termios.tcgetattr(fd)
        nattr=termios.tcgetattr(fd)
        nattr[3] &= ~termios.ICANON
        nattr[3] &= ~termios.ECHO
        nattr[6][termios.VMIN]=1
        nattr[6][termios.VTIME]=0
        termios.tcsetattr(fd, termios.TCSANOW, nattr)

    def close(self):
        fd=self.inf.fileno()
        termios.tcsetattr(fd, termios.TCSANOW, self.sattr)

    def read(self, n: int = 1) -> str:
        return self.inf.read(n)

class PracticeString(object):
    ALPHA="abcdefghijklmnopqrstuvwxyz"
    NUM="0123456789"
    SYM=",.()\"'-_*.;{}<>[]+/\\:=?`!@#$%^&|~"
    def __init__(self, mode: str, cset: str):
        self.mode=mode
        self.getcset(cset)

    def getqstr(self, a: chr, b: chr):
        for cs in (self.ALPHA, self.NUM, self.SYM):
            k1=cs.find(a)
            k2=cs.find(b)
            if k1>=0 and k2>=0:
                return cs[k1:k2+1]
        return ""

    def getcset(self, cset: str):
        if cset=="alpha":
            self.cset=self.ALPHA
        elif cset=="num":
            self.cset=self.NUM
        elif cset=="sym":
            self.cset=self.SYM
        elif cset=="all":
            self.cset=self.ALPHA + self.NUM + self.SYM
        else:
            self.cset=""
            pss=['','','']
            for mc in cset:
                pss.append(mc)
                if pss[0]!='.' and pss[1]=='.' and pss[2]=='.' and pss[3]!='.':
                    self.cset+=self.getqstr(pss[0], pss[3])
                    pss=['','','']
                    continue
                a=pss.pop(0)
                if a!='': self.cset+=a
            for a in pss:
                if a!='': self.cset+=a

    def genstr(self) -> str:
        pstrset=[]
        cslen=len(self.cset)
        for i in range(6):
            astr=""
            for j in range(4):
                if self.mode=="mix" or j==0:
                    k1=random.randrange(cslen)
                astr+=self.cset[k1]
            pstrset.append(astr)
        self.pstr=" ".join(pstrset)
        return self.pstr

class PracticeRun(PracticeString):
    def __init__(self, mode: str, cset: str):
        super().__init__(mode, cset)
        self.totaltime_ms=0
        self.totalchrs=0

    def oneline(self) -> int:
        self.genstr()
        print(self.pstr)
        ckin=ConsoleKeyIn()
        rstr=""
        sts=time.time_ns()
        while True:
            a=ckin.read()
            if rstr=="": sts=time.time_ns()
            if a=="\n":
                ets=time.time_ns()
                break
            if ord(a)==127:
                rstr=rstr[:-1]
                print("\b", end="")
                sys.stdout.flush()
                continue
            print(a, end="")
            sys.stdout.flush()
            rstr+=a
        ckin.close()
        print()
        if rstr=="q": return 1
        dts=(ets-sts)//1000000
        if rstr!=self.pstr:
            for i,a in enumerate(self.pstr):
                if i<len(rstr) and a==rstr[i]:
                    print(a, end="")
                else:
                    print("\033[91m%s\033[0m" % a, end="")
            print()
        if dts!=0:
            print("time=%.02f, cps=%.02f" % (dts/1000.0, 30*1000/dts))
        self.totaltime_ms+=dts
        self.totalchrs+=30
        return 0

    def total_result(self):
        if self.totaltime_ms!=0:
            print("total time=%.02f, cps=%.02f" % (self.totaltime_ms/1000.0,
                                                   self.totalchrs*1000/self.totaltime_ms))

if __name__ == "__main__":
    random.seed()
    parser=argparse.ArgumentParser(description="practice twostkbd")
    parser.add_argument("-m", "--mode", nargs="?", default="mix",
                        help="mix|repeat")
    parser.add_argument("-c", "--cset", nargs="?", default="all",
                        help="alpha|num|sym|all|x..y")
    opt=vars(parser.parse_args())
    pcs=PracticeRun(**opt)
    while True:
        if pcs.oneline(): break
    pcs.total_result()
