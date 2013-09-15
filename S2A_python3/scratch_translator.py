# -*- coding: utf-8 -*-
"""
Created on Sat Sep  7 14:45:49 2013

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

from http.server import BaseHTTPRequestHandler
from http.server import HTTPServer
import configparser


class GetHandler(BaseHTTPRequestHandler):
    """
    This class contains the HTTP server that Scratch2 will connect to
    sends HTTP GET requests to the Arduino microcontroller.
    
    It shares the scratch reporter dictionary, reporter lock and command
    deque with the arduino_translator class
    
    HTTP GET requests are accepted, verified and then passed on to the
    arduino translator via a shared deque.
    
    Reporter information is continously updated by the arduino_translator.
    When a poll request is received, the latest and greatest is reported 
    back to scratch in one big happy block.
    """
    
    # shared resources with arduino_translator 
    scratch_reporter_dict = None
    reporter_lock = None 
    command_deque = None
    
    # tcp server port read from config file
    port = None
    
    #command to pin map
    command_pin_map_dict = None
    
    #indicator so that we can tell user Scratch is ready to go
    waiting_for_first_scratch_poll = True
    
    # this is a classmethod because we need to set data before starting
    # the HTTP server.
    @classmethod
    def set_items(self, scratch_reporter_dict, 
                             reporter_lock, 
                             command_deque,
                             port,
                             config_file):
        """
        This method stores the input parameters for later use.
        It is a classmethod, because these values need to established
        prior to instantiating the class
        """
        self.scratch_reporter_dict = scratch_reporter_dict
        self.reporter_lock = reporter_lock
        self. command_deque = command_deque
        self.port = port
        self.config_file = config_file
        
        Config = configparser.ConfigParser()
        Config.read(self.config_file)
        self.command_pin_map_dict = dict(Config.items("CommandPinMapSection"))

    def do_GET(self):
        """
        Scratch2 only sends HTTP GET commands. This method processes them.
        It differentiates between a "normal" command request and a request
        to send policy information to keep Flash happy on Scratch.
        (This may change when Scratch is converted to HTML 5
        """
        
        # skip over the / in the command
        self.cmd =  self.path[1:]
        if self.cmd == 'crossdomain.xml':
          self.sendPolicy()
        else:
          self.do_ScratchCmd(self.cmd)
        return

    # this is to keep the Scratch flash stuff happy        
    def sendPolicy(self):
      """
      This method returns cross domain policy back to Scratch upon request.
      """
      policy = "<cross-domain-policy>\n"
      policy += "  <allow-access-from domain=\"*\" to-ports=\""
      policy += str(self.port)
      policy += "\"/>\n"
      policy += "</cross-domain-policy>\n\0"
      self.send_resp(policy)
      return
    
    # we can't use the standard send_respone since we don't conform to its 
    # standards, so we craft our own response handler here
    def send_resp(self, response):
      """
      This method sends Scratch an HTTP response to an HTTP GET command.
      """
      crlf = "\r\n"
      httpResponse = "HTTP/1.1 200 OK" + crlf
      httpResponse += "Content-Type: text/html; charset=ISO-8859-1" + crlf
      httpResponse += "Access-Control-Allow-Origin: *" + crlf
      httpResponse += crlf 
      # add the response to the nonsense above
      httpResponse += response + crlf

      # send it out the door to Scratch
      self.wfile.write(httpResponse.encode('utf-8') )  
      
    # handle all scratch commands
    # test only for known commands and throw out all others
    # silently, including poll commands
    def do_ScratchCmd(self, cmd):
        """
        This method processes scratch HTTP GET commands requesting reporter data
        in the form of a "poll" or a command request to affect an actuator.
        """
        if cmd == "poll":
            # if this the first poll received, let user know scratch
            # is now ready to interact
            if GetHandler.waiting_for_first_scratch_poll:
                GetHandler.waiting_for_first_scratch_poll = False
                print('Scratch is initialized and ready to Rock n Roll')
            else:
                pass

            s = ""
            self.reporter_lock.acquire(True)
            # check to see if there is something to report
            if len(self.scratch_reporter_dict):
                for reporter, data in list(self.scratch_reporter_dict.items()):
                    s += reporter
                    s += ' '
                    s += str(data)
                    s += '\n\r'
            #nothing there, just return OK
            else:
                s = "okay" 
             
            self.send_resp(s)
            self.reporter_lock.release()
        else:
            # check to if this is a valid command
            # split the command from any parameters - '/' is delimiter
            split_command = cmd.split('/')

            #check to see if this is a valid command
            if split_command[0] in self.command_pin_map_dict:
                # make sure the correct number of parameters were supplied
                # first get the value for the command out of the dictionary
                command_value = self.command_pin_map_dict.get(split_command[0])
                
                #now split the data using comma as the delimiter
                value_list = command_value.split(',')
                
                # compare the number of expected parameters and 
                # the number of parameters in the command
                if int(value_list[1])!= (len(split_command) - 1):
                    self.send_resp("wrong number of parameters: " + cmd)
                else:
                    self. command_deque.append(split_command)
                    self.send_resp("okay")
            # not a valid command
            else:
                self.send_resp("unknown command: " + cmd)

def start_server(scratch_reporter_dict, 
                 reporter_lock, 
                 command_deque,
                 config_file):
    """
       This function populates class variables with essential data and 
       instantiates the HTTP Server
    """
    Config = configparser.ConfigParser()
    Config.read(config_file)    
    port = Config.get("HTTPServerSection", "PORT")
    
    print("HTTP Serverport is initialized with port = %s\n" % (port))
    GetHandler.set_items(scratch_reporter_dict, 
                             reporter_lock, 
                             command_deque,
                             port,
                             config_file)
    try:
        server = HTTPServer(('localhost', int(port)), GetHandler)
        print('Starting Scratch HTTP Server!')
        print('Use <Ctrl-C> to exit the extension\n')
        print('Waiting for Scratch handshake ....')
    except Exception:
        print('HTTP Socket may already be in use - restart Scratch')
        raise
    try:
        #start the server
        server.serve_forever()
    except KeyboardInterrupt:
        print("Goodbye !")
        raise KeyboardInterrupt
    except Exception:
        raise
