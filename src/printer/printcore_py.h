
/*
    This file is a part of the RepSnapper project.
    Copyright (C) 2012 martin.dieringer@gmx.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/



const char * printcore_py = "\n\
#!/usr/bin/env python\n\
\n\
# This file is part of the Printrun suite.\n\
#\n\
# Printrun is free software: you can redistribute it and/or modify\n\
# it under the terms of the GNU General Public License as published by\n\
# the Free Software Foundation, either version 3 of the License, or\n\
# (at your option) any later version.\n\
#\n\
# Printrun is distributed in the hope that it will be useful,\n\
# but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
# GNU General Public License for more details.\n\
#\n\
# You should have received a copy of the GNU General Public License\n\
# along with Printrun.  If not, see <http://www.gnu.org/licenses/>.\n\
\n\
from serial import * #Serial, SerialException\n\
from threading import Thread\n\
from select import error as SelectError\n\
import time, getopt, sys\n\
\n\
class printcore():\n\
    def __init__(self,port=None,baud=None):\n\
        \"\"\"Initializes a printcore instance. Pass the port and baud rate to connect immediately\n\
        \"\"\"\n\
        self.baud=None\n\
        self.port=None\n\
        self.printer=None #Serial instance connected to the printer, None when disconnected\n\
        self.clear=0 #clear to send, enabled after responses\n\
        self.online=False #The printer has responded to the initial command and is active\n\
        self.printing=False #is a print currently running, true if printing, false if paused\n\
        self.mainqueue=[]\n\
        self.priqueue=[]\n\
        self.queueindex=0\n\
        self.lineno=0\n\
        self.resendfrom=-1\n\
        self.paused=False\n\
        self.sentlines={}\n\
        self.log=[]\n\
        self.sent=[]\n\
        self.tempcb=None#impl (wholeline)\n\
        self.recvcb=None#impl (wholeline)\n\
        self.sendcb=None#impl (wholeline)\n\
        self.errorcb=None#impl (wholeline)\n\
        self.startcb=None#impl ()\n\
        self.endcb=None#impl ()\n\
        self.onlinecb=None#impl ()\n\
        self.loud=True#emit sent and received lines to terminal\n\
        self.greetings=['start','Grbl ']\n\
        if port is not None and baud is not None:\n\
            #print port, baud\n\
            self.connect(port, baud)\n\
            #print \"connected\\n\"\n\
\n\
\n\
    def disconnect(self):\n\
        \"\"\"Disconnects from printer and pauses the print\n\
        \"\"\"\n\
        if(self.printer):\n\
            self.printer.close()\n\
        self.printer=None\n\
        self.online=False\n\
        self.printing=False\n\
\n\
    def connect(self,port=None,baud=None):\n\
        \"\"\"Set port and baudrate if given, then connect to printer\n\
        \"\"\"\n\
        if(self.printer):\n\
            self.disconnect()\n\
        if port is not None:\n\
            self.port=port\n\
        if baud is not None:\n\
            self.baud=baud\n\
        if self.port is not None and self.baud is not None:\n\
            self.printer=Serial(self.port,self.baud,timeout=0.5)\n\
            self.clear=True\n\
            #thread = Thread(target=self._listen).start()\n\
            #print \"thread?\",thread\n\
\n\
    def reset(self):\n\
        \"\"\"Reset the printer\n\
        \"\"\"\n\
        if(self.printer):\n\
            print \"resetting\"\n\
            self.printer.setDTR(1)\n\
            self.printer.setDTR(0)\n\
\n\
\n\
    def _listen_single(self):\n\
        \"\"\"This function acts on messages from the firmware\n\
        \"\"\"\n\
        try:\n\
            line=self.printer.readline()\n\
            print \"line:\",line\n\
        except SelectError, e:\n\
            if 'Bad file descriptor' in e.args[1]:\n\
                print \"Can't read from printer (disconnected?).\"\n\
                return False\n\
            else:\n\
                print \"error\"\n\
        except SerialException, e:\n\
            print \"Can't read from printer (disconnected?).\"\n\
            return False\n\
        except OSError, e:\n\
            print \"Can't read from printer (disconnected?).\"\n\
            return False\n\
\n\
        if(len(line)>1):\n\
            self.log+=[line]\n\
            if self.recvcb is not None:\n\
                try:\n\
                    self.recvcb(line)\n\
                except:\n\
                    pass\n\
            if self.loud:\n\
                print \"RECV: \",line.rstrip()\n\
        if(line.startswith('DEBUG_')):\n\
            return True\n\
        if(line.startswith(tuple(self.greetings)) or line.startswith('ok')):\n\
            self.clear=True\n\
        if(line.startswith(tuple(self.greetings)) or line.startswith('ok') or \"T:\" in line):\n\
            if (not self.online or line.startswith(tuple(self.greetings))) and self.onlinecb is not None:\n\
                try:\n\
                    self.onlinecb()\n\
                except:\n\
                    pass\n\
            self.online=True\n\
            if(line.startswith('ok')):\n\
                #self.resendfrom=-1\n\
                #put temp handling here\n\
                if \"T:\" in line and self.tempcb is not None:\n\
                    try:\n\
                        self.tempcb(line)\n\
                    except:\n\
                        pass\n\
                #callback for temp, status, whatever\n\
        elif(line.startswith('Error')):\n\
            if self.errorcb is not None:\n\
                try:\n\
                    self.errorcb(line)\n\
                except:\n\
                    pass\n\
            #callback for errors\n\
            pass\n\
        if line.lower().startswith(\"resend\") or line.startswith(\"rs\"):\n\
            try:\n\
                toresend=int(line.replace(\"N:\",\" \").replace(\"N\",\" \").replace(\":\",\" \").split()[-1])\n\
            except:\n\
                if line.startswith(\"rs\"):\n\
                    toresend=int(line.split()[1])\n\
            self.resendfrom=toresend\n\
            self.clear=True\n\
        print \"end _listen_single\"\n\
        return True\n\
\n\
\n\
    def _listen(self):\n\
        self.clear=True\n\
        self.send_now(\"M105\")\n\
        while(True):\n\
            if(not self.printer or not self.printer.isOpen):\n\
                break\n\
            if not self._listen_single(): break\n\
        time.sleep(1.0)\n\
        self.clear=True\n\
        #callback for disconnect\n\
\n\
    def _checksum(self,command):\n\
        return reduce(lambda x,y:x^y, map(ord,command))\n\
\n\
    def startprint(self,data):\n\
        \"\"\"Start a print, data is an array of gcode commands.\n\
        returns True on success, False if already printing.\n\
        The print queue will be replaced with the contents of the data array, the next line will be set to 0 and the firmware notified.\n\
        Printing will then start in a parallel thread.\n\
        \"\"\"\n\
        if(self.printing or not self.online or not self.printer):\n\
            print \"can't print\",self.printing,self.online  ,self.printer\n\
            return False\n\
        self.printing=True\n\
        self.mainqueue=[]+data\n\
        self.lineno=0\n\
        self.queueindex=0\n\
        self.resendfrom=-1\n\
        self._send(\"M110\",-1, True)\n\
        print \"mainqueue\",len(self.mainqueue),\"lines\"\n\
        if len(data)==0:\n\
            return True\n\
        self.clear=False\n\
        Thread(target=self._print).start()\n\
        return True\n\
\n\
    def startprint_text(self,text):\n\
        data = text.split(\"\\n\")\n\
        print \"startprint with\",len(data),\"lines\"\n\
        return self.startprint(data)\n\
\n\
    def pause(self):\n\
        \"\"\"Pauses the print, saving the current position.\n\
        \"\"\"\n\
        self.paused=True\n\
        self.printing=False\n\
        time.sleep(1)\n\
\n\
    def resume(self):\n\
        \"\"\"Resumes a paused print.\n\
        \"\"\"\n\
        self.paused=False\n\
        self.printing=True\n\
        Thread(target=self._print).start()\n\
\n\
    def send(self,command):\n\
        \"\"\"Adds a command to the checksummed main command queue if printing, or sends the command immediately if not printing\n\
        \"\"\"\n\
\n\
        if(self.printing):\n\
            self.mainqueue+=[command]\n\
        else:\n\
            while not self.clear:\n\
                print \"not clear\"\n\
                time.sleep(0.001)\n\
            self._send(command,self.lineno,True)\n\
            self.lineno+=1\n\
\n\
\n\
    def send_now(self,command):\n\
        \"\"\"Sends a command to the printer ahead of the command queue, without a checksum\n\
        \"\"\"\n\
        #print \"send_now\",command\n\
        if(self.printing):\n\
            self.priqueue+=[command]\n\
        else:\n\
            while not self.clear:\n\
                print \"not clear\"\n\
                time.sleep(0.001)\n\
            self._send(command)\n\
        #callback for command sent\n\
\n\
    def _print(self):\n\
        #callback for printing started\n\
        if self.startcb is not None:\n\
            try:\n\
                self.startcb()\n\
            except:\n\
                pass\n\
        while(self.printing and self.printer and self.online):\n\
            print \"print thread\"\n\
            self._sendnext()\n\
        self.log=[]\n\
        self.sent=[]\n\
        if self.endcb is not None:\n\
            try:\n\
                self.endcb()\n\
            except:\n\
                pass\n\
        #callback for printing done\n\
\n\
    def _sendnext(self):\n\
        if(not self.printer):\n\
            return\n\
        while not self.clear:\n\
            print \"not clear\"\n\
            time.sleep(0.001)\n\
        self.clear=False\n\
        if not (self.printing and self.printer and self.online):\n\
            self.clear=True\n\
            return\n\
        if(self.resendfrom<self.lineno and self.resendfrom>-1):\n\
            self._send(self.sentlines[self.resendfrom],self.resendfrom,False)\n\
            self.resendfrom+=1\n\
            return\n\
        self.sentlines={}\n\
        self.resendfrom=-1\n\
        for i in self.priqueue[:]:\n\
            self._send(i)\n\
            del(self.priqueue[0])\n\
            return\n\
        if(self.printing and self.queueindex<len(self.mainqueue)):\n\
            tline=self.mainqueue[self.queueindex]\n\
            tline=tline.split(\";\")[0]\n\
            if(len(tline)>0):\n\
                self._send(tline,self.lineno,True)\n\
                self.lineno+=1\n\
            else:\n\
                self.clear=True\n\
            self.queueindex+=1\n\
        else:\n\
            self.printing=False\n\
            self.clear=True\n\
            if(not self.paused):\n\
                self.queueindex=0\n\
                self.lineno=0\n\
                self._send(\"M110\",-1, True)\n\
\n\
    def _send(self, command, lineno=0, calcchecksum=False):\n\
        command = command.upper();\n\
        print \"_send\", command\n\
        if(calcchecksum):\n\
            prefix=\"N\"+str(lineno)+\" \"+command\n\
            command=prefix+\"*\"+str(self._checksum(prefix))\n\
            if(\"M110\" not in command):\n\
                self.sentlines[lineno]=command\n\
        if(self.printer):\n\
            self.sent+=[command]\n\
            if self.loud:\n\
                print len(self.mainqueue), \"SENT: \", command\n\
            if self.sendcb is not None:\n\
                try:\n\
                    self.sendcb(command)\n\
                except:\n\
                    pass\n\
            try:\n\
                self.printer.write(str(command+\"\\n\"))\n\
            except SerialException, e:\n\
                print \"Can't write to printer (disconnected?).\"\n\
\n\
\n\
    def is_online(self):\n\
        return (self.printer != None)\n\
\n\
";
