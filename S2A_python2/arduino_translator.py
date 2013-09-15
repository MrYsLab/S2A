# -*- coding: utf-8 -*-
"""
Created on Wed Sep  4 15:17:20 2013

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

import threading
import json
import itertools
import ConfigParser


class ArduinoTranslator(threading.Thread):
   """
   This class is responsible for communicating with an Arduino using
   the JSON Arduino Sketch

   It translates to and from the Scratch view of data and control

   The class uses the configuration table data to determine Arduino control

   It shares the scratch reporter dictionary, reporter lock and command
    deque with the arduino_translator class       
   """
   
   # this map will be populated with reporter data
   # it is protected by a thread lock
   reporter_map = {}
   
   # a flag to provide special PWM pin processing affected by the servo
   # and tone libraries provided by Arduino.
   piezo_or_servo = False
   


        
   def __init__(self, arduino, scratch_reporter_dict,reporter_lock, 
                command_deque, config_file):
       """
       Constructor
       """                   
       self.arduino = arduino         
       self.scratch_reporter_dict = scratch_reporter_dict
       self.reporter_lock = reporter_lock
       self.command_deque = command_deque
       self.config_file = config_file

       threading.Thread.__init__(self)
       
       # we need to set this as a daemon so that when the user hits CNTRL-C
       # the thread dies cleanly and we exit back to the shell
       self.daemon = True

   # test if arduino json client is responding

   def is_arduino_ready(self):
       """
       This method determines if the Arduino JSON client is alive and well
       """
       #time.sleep(1)
        # attempt to communicate with arduino
        # try 5 times and if it fails, exit
       for attempt in range(5):
           self.arduino.write("{\"query\":\"status\"}")
           decoded = json.loads(self.arduino.read_line())
           arduino_status = decoded["status"]
           if arduino_status == "ready":
               self.__initialize_pin_io()
               return True
           else:
               return False
          
   def __initialize_pin_io(self):
       """
       This method establishes pin mode (INPUT or OUTPUT) 
       and initial values for output pin.
       """
       Config = ConfigParser.ConfigParser()
       Config.read(self.config_file)
 
        
       # read in the configuration file to setup pin direction
       arduino_pin_directions_dict = dict(Config.items("ArduinoPinDirection"))
       

       for pin, mode in arduino_pin_directions_dict.items():
          pin_direction = Config.get("JsonStringTemplateSection", \
                                                     "setPinDirection") 
          pin_direction = pin_direction.replace("PIN", pin)
          pin_direction = pin_direction.replace("MODE", mode)
          self.arduino.send_command(pin_direction)
                                                       

       #initialize output pin values
       Config = ConfigParser.ConfigParser()
       Config.read(self.config_file)
        
       #build a translator dictionary for the reporter items going
       #back to Scratch
       arduino_init_output_pin_values_dict = \
                       dict(Config.items("ArduinoInitialOutputPinValues"))
       
       for pin, type_value in arduino_init_output_pin_values_dict.items():
           initial_pin_out_value = Config.get("JsonStringTemplateSection", \
                                                         "writeValueToPin")
           index = type_value.find(',')
           type = type_value[0:index]
           value = type_value[index+1:]
           initial_pin_out_value = initial_pin_out_value.replace("PIN", \
                                                                  pin)
           initial_pin_out_value = initial_pin_out_value.replace("TYPE",\
                                                                  type)
           initial_pin_out_value = initial_pin_out_value.replace("VALUE", \
                                                                  value)
           self.arduino.send_command(initial_pin_out_value)

        # build the translation dictionary for polling
       self.reporter_map = dict(Config.items("ReporterMapSection"))
       for pin, scratch_label in self.reporter_map.items():
           self.scratch_reporter_dict[scratch_label] = 0

            
   # thread to continuously gather poll data

   def run(self):
       """
       This is the thread that continuously queries Arduino sensors,
       safely (with a thread lock), stores the values in the reporter map
       and look to see if Scratch has issued any actuator commands passed in
       through the deque.
       
       It also determines if either a Servo or Tone request was made,
       and if so, it treats PWM settings as strictly digital settings as
       a workaround for the CodeShield. This feature can be disabled
       in the configuration file.
       """

       Config = ConfigParser.ConfigParser()
       Config.read(self.config_file)
       reporter_pin_to_type_dict = dict(Config.items("ReporterPinToTypeMap"))
       
       # this is a workaround for Tone and Servo libraries affecting PWM
       # operation of certain pins

       special_led_processing = Config.get("SpecialProcessing", 
                                           "enable_special_LED_processing")
       
       if special_led_processing == "True":
           special_led_processing = True
       elif special_led_processing == "False":           
           special_led_processing = False 
       # in case user typed in the incorrect value in the config file
       # force it to True
       else:                                    
           special_led_processing = True
       
       # if there is nothing to report, just look for incoming commands
       # otherwise just keep on pollin'
       while True:
         # if we have things to report, then report then
         if len(reporter_pin_to_type_dict):
           for element in itertools.cycle(reporter_pin_to_type_dict.items()):
             read_reporter_data = Config.get("JsonStringTemplateSection", \
                                                             "readPinValue") 
             pin = element[0]

             type = element[1]
             if pin == "encoder":
                 read_reporter_data = read_reporter_data.replace( \
                                                        "\"pin\":PIN", \
                                                        "\"encoder\":100")
             else:
                 read_reporter_data = read_reporter_data.replace("PIN", \
                                                                   pin)
                                                                   
             read_reporter_data = read_reporter_data.replace("TYPE", \
                                                                   type)                                                                   

             # serialize the json reply string so we can parse out
             # the juicy bits
             try:
                 jreply = json.loads(self.arduino.get_data(read_reporter_data))
             except Exception:
                 raise

             # the info we want is in the pinValue json object
             jpinval = jreply["pinValue"]
             
             #parse out the pinValue object for pin and value
             r_pin = str(jpinval["pin"])
             r_value = str(jpinval["value"])
             scratch_type = self.reporter_map[r_pin]
             
             # now update the scratch reporting dictionary with the latest
             # value for this item
             self.reporter_lock.acquire(True)
             self.scratch_reporter_dict[scratch_type] = r_value
             self.reporter_lock.release()
             self.do_command(special_led_processing)
         # else nothing to report so just look for commands
         else:
             self.do_command(special_led_processing)
         

                 
   def do_command(self, special_led_processing):
     """
     This method processes Scratch commands. It checks to see if
     any special processing is necessary as a result of calls to 
     Tone or Servo
     """
     # check to see if there is a command to send
     if self.command_deque:
              
         # get the command
         command = self.command_deque.popleft()
         Config = ConfigParser.ConfigParser()
         Config.read(self.config_file)
    
         command_pin_map_dict  =  \
                     dict(Config.items("CommandPinMapSection"))                        
         
         # handle the special case commands
         
         # handle a Tone request
         if command[0] == "piezo_tone":
             ArduinoTranslator.piezo_or_servo = True
             cmd_string = Config.get("JsonStringTemplateSection", \
                                     "writePiezo")
             cmd_string = cmd_string.replace("FREQ", str(command[1]))
             cmd_string = cmd_string.replace("TIME", str(command[2]))
             
         #handle a servo request
         elif command[0] == "servo_degrees":
             ArduinoTranslator.piezo_or_servo = True
             cmd_string = Config.get("JsonStringTemplateSection", \
                                     "writeServo")
             cmd_string =cmd_string.replace("VALUE", str(command[1]))
         # now the default cases
         else:
             cmd_string = Config.get("JsonStringTemplateSection", \
                                     "writeValueToPin") 
             
             # here is the workaround for CodeShield LED PWM
             # control.  
             if ArduinoTranslator.piezo_or_servo and special_led_processing:
                 if str(command[1]) != "0":
                     cmd_string = cmd_string.replace("VALUE", \
                                               "1")
                 else:
                     cmd_string = cmd_string.replace("VALUE", \
                                               "0") 
             else:                         
                 cmd_string = cmd_string.replace("VALUE", \
                                               str(command[1]))
             # retieve pin and type from the table to complete
             # the cmd_string
             cmd_descriptor = command_pin_map_dict.get(command[0])
             cmd_elements = cmd_descriptor.split(',')
             
             # element 0 = pin
             # element 1 (not used here) is the number of parameters
             # element 2 = type
             cmd_string = cmd_string.replace("PIN", \
                                     str(cmd_elements[0]))
             if ArduinoTranslator.piezo_or_servo and special_led_processing:
                 cmd_string = cmd_string.replace("TYPE", "digital")
             else:
                 cmd_string = cmd_string.replace("TYPE", \
                                     str(cmd_elements[2]))
             
         #send the command string to the Arduino for processing 
         self.arduino.send_command(cmd_string) 

