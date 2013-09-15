# -*- coding: utf-8 -*-

"""
Created on Wed Sep  4 13:17:15 2013

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

import sys
import threading
from collections import deque
import arduino_serial
import arduino_translator
import scratch_translator



class Scratch2ArduinoExtension:
    """
    This is the main class for the Scratch extensions program.
    """
    def main():
        
        """ 
        This method:
        Instantiates shared data structures for communication
        between an Arduino microcontroller and Scratch2
                                   
        The serial port for Arduino communications is opened.
    
        An Arduino translation object is instantiated and its
        processing thread is started.
       
        An HTTP server is instantiated and started to handle
        all communications to and from Scratch.
       
        This method never ends unless an error is encountered or
        the user presses CTRL-C
        """
        
        # default config file if none specified when program invoked
        if len(sys.argv) == 2:  
            config_file = str(sys.argv[1])
        else:
            config_file = "s2e.cfg"
            
        print("\nUsing configuration file: %s" % config_file)
        print("If you wish to  use another configuration file, ")
        print("specify the configuration file name on the command line.")
        print("Example:")
        print("python scratch_extension.py my_own_config_file")
        # this is the data structure that allows the Arduino side of the 
        # of the interface to share reporter data with the Scratch side
        # in a safe and efficient manner        
        scratch_reporter_dict = {}        
        
        #establish a lock for the reporter dictionary to make it a safe
        # sharing data structure
        reporter_lock =  threading.Lock()

        #establish a deque for passing commands from Scratch to the 
        #arduino
    
        # Scratch will append to the right side and the Arduino side of this
        # extension will pop from the left, effectively making this a FIFO
        # data structure. This does not need a lock to be used safely.
    
        command_deque = deque() 
        
        # we use a seperate class for serial communication in anticipation
        # of WiFi in the very near future. This should allow us to adapt
        # quckly.
        
        arduino = arduino_serial.ArduinoSerial(config_file)
        
        # open communications to the arduino
        try:
            arduino.open()
        except Exception:
            print('Serial Port Open Failed:')
            print('            is the Arduino available and plugged in ?')
            sys.exit(1)

        # a class is used to translate commands to and from the arduino 
        # board.

        # we pass in some common data structures that it will share with the
        # scratch side of the house. 
        the_arduino_translator = arduino_translator.ArduinoTranslator( \
                                                     arduino, \
                                                     scratch_reporter_dict, \
                                                     reporter_lock,\
                                                     command_deque, \
                                                     config_file)                                                 
        # does the arduino have a responding json client available?                                                    
        # verify that json client is ready and then init all i/o pins                                              
        if the_arduino_translator.is_arduino_ready():
            # kick off the arduino thread
            the_arduino_translator.start()  
            print('Arduino interface is up and running.\n')
        else:
            print('Arduino JSON client does not respond')
            sys.exit(1)

        # send the initiazlization information for the scratch translator
        # to use. This will kick off the HTTP server and then we are off to
        # the races.
        try:                 
            scratch_translator.start_server(scratch_reporter_dict, 
                             reporter_lock, 
                             command_deque,
                             config_file)  
        except Exception:
            arduino.clean_up
            arduino.close()
            return                               
        except KeyboardInterrupt:
            # give control back to the shell that started us
            arduino.clean_up()
            return



   # this is the program "main" so we kick off from here 
    if __name__ == "__main__":
        main()

        
