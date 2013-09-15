# -*- coding: utf-8 -*-
"""
Created on Tue Sep  3 07:12:01 2013

@author: Alan Yorinks
Copyright (c) 2013 Alan Yorinks All right reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
"""

import serial
import sys
import configparser



class ArduinoSerial:
    """
     This class manages the serial port for Arduino serial communications
    """

    # class variables
    # Arduino port
    arduino = serial.Serial() 
    
    # line of data, '\n' terminated, read from Arduino
    read_string = ""
    
    port_id =""
    baud_rate = 0
    timeout = 0
    

    def __init__(self, config_file):
        """
        Get configuration values for the serial port
        """
        Config = configparser.ConfigParser()
        Config.read(config_file)
        
        self.port_id = Config.get("SerialPortSection", "ComPort")
        self.baud_rate = Config.get("SerialPortSection", "BaudRate")
        self. timeout = Config.get("SerialPortSection", "TimeOut")
        self.config_file = config_file
    
    def open(self):
        """ 
        open the serial port using the configuration data
        
        returns a reference to this instance
        """
        # open a serial port
        print('\nOpening Arduino Serial port %s ' % ( self.port_id ))

        try:
            self.arduino = serial.Serial(self.port_id, self.baud_rate, 
                                     timeout=int(self.timeout) )
        # in case the port is already open, let's close it and then
        #reopen it   
            self.arduino.close()    
            self.arduino.open()           
            return self.arduino
        except Exception:
           # opened failed - will report back to caller
           raise
           
    def close(self):
        """
            Close the serial port
            
            return: None
        """
        self.arduino.close()
                
    def write(self, data):
        """
            write the data to the serial port
            
            return: None
        """
        try:
            self.arduino.write(data.encode('utf8'))
        except Exception:
            raise

    # keep reading in data until a new line is found
    def read_line(self):
        """
            Read a line of data from the serial port
            
            return: a line of data
        """

        self.read_string = ""
        while 1:
            try:
                ch = self.arduino.read(1)
                if(ch == b'\n'):  
                    break
                self.read_string += ch.decode('utf-8')
                # check to make sure that we don't go on forever
                if len(self.read_string) > 80:
                    print("ArduinoSerial: read_line exceeded 80 characters")
                    raise EOFError
            except Exception:
               self.clean_up()
               raise
        return self.read_string

        
    def send_command(self, data):
        """
            Send a command to the Arduino and wait for the "{}" reply
            
            return: None
        """
        try:
            self.write(data)
            reply = self.read_line()
           
            if reply == "{}":
                pass
            else:
                print("send_command: received bad reply %s" % (reply))
                sys.exit(1)
        except Exception:
            raise
            
    def get_data(self, poll_req):
        """
            Send a request for reporter data to the Arduino
            
            return: reporter reply string
        """
        try:
            self.write(poll_req)
            reply = self.read_line()
            return reply
        except Exception:
            raise
            
    def clean_up(self):
        self.arduino.flushInput()
        self.arduino.flushOutput()


        
        
        
   
        

        
