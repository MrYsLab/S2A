#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>
#include <Servo.h>
#include <stdio.h>

#define ENCODER_A A0
#define ENCODER_B A1
#define ENCODER_PORT PINC
#define SWITCH 13
#define BUTTON 12
#define RGB_RED 11
#define RGB_GREEN 10
#define RGB_BLUE 9
#define LED 6
#define SERVO 5
#define PIEZO 3
#define RELAY 2
#define POT 2
#define HALL 3
#define THERMISTOR 4
#define PHOTOCELL 5


// create an encoder instance and a global variable for position
Servo servo ;
Encoder encoder(A1, A0) ;
long encoderPosition = -999 ;

//create a servo instance and a global variable for its position
//Servo servo ;
int servoPosition = 0 ;

long position = -999 ;
/*
 * Simple Arduino proxy; does what it's told
 * via the USB connection, and reports back
 * the values of its inputs. Allows code on
 * the other end of the USB cable to do the
 * computation, so the Arduino is just an IO
 * device rather than a microcontroller.
 *
 * Some examples (note that the name:value
 * pairs can appear in any order):
 * {"mode":{"pin":1,"mode":"input"}}                  // Set pin 1 to be an input pin
 * {"mode":{"mode":"output","pin":8}}                 // Set pin 8 to be an output pin
 * {"write":{"pin":4,"type":"digital","value":1}}     // Set digital pin 4 to HIGH
 * {"write":{"pin":2,"type":"digital","value":0}}     // Set digital pin 2 to LOW
 * {"write":{"pin":5,"type":"analog","value":32}}     // Set analog pin 5 to 32 (range is 0-255)
 * {"read":{"pin":4,"type":"analog"}}                 // Return the value of analog pin 4 (0-1023)
 * {"write":{"type":"piezo","freq":440,"time":1000}} // set the piezo device for the time duaration and frequency
 * {"write":{"type":"servo","value":90, "pin":5}}      // set the servo position
 * {"read":{"encoder":100,"type":"analog"}}            // return encoder value - numerical parameter is not used but                                                      //                        needs to be filled in
 */

/* This is a modification of the work done by Chris Warburton http://chriswarbo.net/index.php?page=cedi&type=misc&id=1%2F3%2F6%2F10
 The original can be found here: https://gitorious.org/arduino-json-client#more
 
 Modifications by Alan Yorinks
 */

void setup()
{
  delay(1000);    // Keep this here so we don't flood the serial line
  Serial.begin(57600);    // Set up Serial library at 57600 bps
  Serial.println("{\"status\":\"ready\"}");
}

void loop()
{
  // Look for some commands in JSON
  char* input = 0;


  input = read_json();

  // See what we received...
  if (input==0) {
    //Serial.println("Unknown input") ;
    // Unknown input

  }
  else {
    // Perform whatever actions are defined for this input
    read_commands(input);
    free(input);
  }
}

char read_char()
{
  // Wait until some data becomes available
  // on the USB cable (this will loop
  // forever if you don't send it anything)
  char data = -1;
  while ((Serial.available() < 0) || (data < 0)) {
    delay(1);
    data = Serial.read();

    // get encoder data in this loop
    long newPos = encoder.read();
    if (newPos != encoderPosition) {
      encoderPosition = newPos;
    }

  }
  return data;
}


char* read_json()
{
  // This will wait for some input, then
  // read it to make sure it is an open
  // brace "{" (discarding it if not) and
  // reading all of the input up to a
  // corresponding close brace "}".
  // Nested sets of braces are allowed.
  // Returns a pointer to whatever it's
  // read.

  // Wait for some serial input and grab it
  char this_value;

  this_value = read_char();

  // See if we should continue reading
  while (this_value != '{')
  {
    // Uh oh, this isn't JSON
    // Discard it and try again...
    this_value = read_char();
  }
  int nested_count = 1;    // Keep track of how deeply nested our braces are
  int pointer_size = 2;      // The size of our char pointer (must be >= 2, for '{' and '}')
  int read_so_far = 1;     // How much data we've read (used to ensure our pointer is big enough)
  char* result = (char*) malloc(sizeof(char)*pointer_size);    // This pointer will be our return value
  char* new_result;    // Used during pointer reallocation
  result[0] = this_value;    // Set the first value to the '{' that we found

  // There are a few exceptions to the simple braced structure...
  short in_quote = 0;    // "strings"
  short in_escape = 0;   // \escaped characters

  while (nested_count > 0)    // Loop until we've closed that first brace
  {
    // Wait for input then read it
    this_value = read_char();

    // See if we've got enough room to store it
    read_so_far++;
    if (read_so_far > pointer_size)
    {
      // Try to increase the size of our JSON pointer
      // NOTE: There are various strategies we could use here
      // depending on what we want to conserve. Since we only
      // build one JSON string at a time, and free it after
      // we're done with it, we don't need to be massively
      // conservative in our memory usage
      char* new_result = (char*) realloc(result, (pointer_size*2));
      if (new_result)
      {
        // We succeeded in allocating enough memory. Let's use it.
        result = new_result;
        pointer_size = pointer_size * 2;
      }
      else
      {
        // Out of memory. Abort.
        free(result);
        return 0;
      }
    }
    // Store this character
    result[read_so_far-1] = this_value;

    // Handle this character
    if (in_quote) {
      // String semantics: read in everything up to a non-escaped '"'
      if (in_escape) {
        // Apply escaping; ie. ignore the character, just unset in_escape
        in_escape = 0;
      }
      else {
        if (this_value == '"') {
          in_quote = 0;    // Our matching close quote
        }
        if (this_value == '\\') {
          in_escape = 1;   // Ignore whatever the next character is
        }
      }
    }
    else {
      // Object semantics: Read in everything up to a non-matched '}'

      if (this_value == '{') {
        nested_count++;    // Recurse down a level
      }
      else {
        if (this_value == '}') {
          nested_count--;  // Come back up a level
        }
        else {
          if (this_value == '"') {
            in_quote = 1;  // Start a string
          }
          else {
            // Some other character
          }
        }
      }
    }
  }
  return result;
}

int json_length(char* json) {
  // Give this a pointer to some JSON data and it will
  // return the length of that JSON.

  if (json == 0) {
    return 0;    // Null pointer
  }

  if (json[0] != '{') {
    return 0;    // Not JSON
  }

  // Now that we know we have a JSON object, we defer
  // the actual calculation to value_length
  return value_length(json);
}

int value_length(char* json) {
  // This is given a fragment of JSON and returns how
  // many characters it contains. This fragment might
  // be an object, a number, a string , etc.
  if (json == 0) {
    return 0;    // Null pointer
  }

  // Switch over each possibility
  int index = 0;
  switch (json[index]) {
  case '{':
    // This is a JSON object. Find the matching '}'
    do {
      index++;    // Read ahead
      if (json[index] == '"') {
        // Skip strings, as they may contain unwanted '}'
        index = index + value_length(json+index);
      }
      if (json[index] == '{') {
        // Recurse past nested objects
        index = index + value_length(json+index);
      }
    } 
    while (json[index] != '}');
    return index + 1;    // Include the '{' and '}' in the length
  case '"':
    // This is a string. Scan ahead to the first unescaped '"'
    do {
      if (json[index] == '\\') {
        index++; // Skip escaped quotes
      }
      index++;    // Read ahead
    } 
    while (json[index] != '"');
    return index+1;    // Include the quotes in the string's length
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-':
    // We're a number. Loop forever until we find a non-number character.
    // Note, this is a simplistic parser that is equivalent to the regex
    // [0123456789-][0123456789.eE]* This allows malformed numbers like
    // 0.0.0.0e0.0e.0
    do {
      index++;
      switch (json[index]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '.':
      case 'e':
      case 'E':
        break;    // Numeric
      default:
        return index;    // Non-numeric. Stop counting.
      }
    } 
    while (1);
  default:
    // Unknown. Ignore it.
    return 0;
  }
}

void read_commands(char* json) {
  // Takes a JSON string and looks for any commands it
  // contains. These are "key":value pairs, which are
  // sent as arguments to the "run_command" function as
  // they are encountered.
  int length = json_length(json);
  int index = 0;    // Used to loop through the contents
  int temp;    // Our parsing uses lookahead, this stores how far we've gone






  // Only bother doing something if json has some contents.
  // When this condition is false, it's essentially the
  // escape clause of our recursion.
  if (length > 2) {    // 2 == empty, since we have '{' and '}'
    index++;    // Skip past the '{' to get at the contents
    while (index < length) {
      switch (json[index]) {
      case ' ':
        // Whitespace is insignificant
        index++;
        break;
      case '{':
        // We have an object in an object, let's recurse
        read_commands(json+index);
        index = index + json_length(json+index);
        break;
      case '"':
        // A string. This should be part of a key:value pair
        if (index + 2 >= length) {
          // JSON can't end with an opening quote. Bail out.
          break;
        }

        // Look one character ahead, then keep going until
        // we find our matching close quote
        temp = index+1;
        while ((json[temp] != '"') && (temp < length)) {
          // We've not found our close quote, so look ahead
          if (json[temp] == '\\') {
            // Increment twice to skip over escaped characters
            temp++;
          }
          temp++;
        }
        if (temp >= length-2) {
          // We've reached the end of the JSON without finding
          // a close quote. Bail out.
          break;
        }

        // Now we've read our name, find our associated value
        temp++;    // It must start after the close quote
        while ((json[temp] == ' ') && (temp < length)) {
          temp++;    // Skip whitespace
        }
        if (json[temp] != ':') {
          // We must have a colon between the name and the value
          // Bail out if not
          break;
        }
        temp++;    // We don't need the colon, skip it
        while ((json[temp] == ' ') && (temp < length)) {
          temp++;    // Skip whitespace
        }

        // Wherever we are, we must have found our value
        // Tell run_command what we've found
        run_command(json+index, json+temp);

        // Now let's get our parser ready for the next value
        index = temp + value_length(json+temp);    // Skip the value
        while ((json[index] == ' ') && (index < length)) {
          index++;    // Skip whitespace
        }
        if (json[index] == ',') {
          // Skip commas between name:value pairs
          index++;
        }
        break;    // Done
      default:
        // Unknown input. Oops.
        index++;
      }
    }
  }
  else {
    // Our JSON is empty
    return;
  }
}

short compare_strings(char* string1, char* string2) {
  // Compare the first character array, which is not
  // null-terminated and is in quotes, to the second
  // which can be null-terminated and without quotes
  int first_size = value_length(string1);
  int second_size;
  for (second_size = 0; string2[second_size] != '\0'; second_size++) {
    // Do nothing. The loop parameters count the string for us.
  }

  // first_size includes quotes, so we don't include them
  // in our check
  if (first_size - 2 != second_size) {
    // The size is different, so the strings are different
    return 0;
  }

  // Now do a lexicographical comparison
  int index;
  for (index = 0; index < first_size - 2; index++) {
    if (string1[index+1] != string2[index]) {
      return 0;    // Mismatch
    }
  }

  // If we're here then our tests couldn't find any different
  return 1;
}

float compile_digits(char* value) {
  // Reads the JSON number in value and returns its value as
  // a float.
  // NOTE: This will overrun the digits by 1! Make sure that
  // the pointer doesn't end when the digits do, or this will
  // produce unspecified behaviour. This shouldn't be an
  // issue if your pointer is part of some JSON object, since
  // we can always overrun on to the '}' in the worst case.
  float result = 0.0;    // We build this up from the whole numbers
  float fraction_result = 0.0;    // We build this up from the fraction
  int value_size = value_length(value);
  int index;
  short fractional = 0;    // 0 = no decimal point found, 1 = found decimal point
  for (index = 0; index < value_size; index++) {
    result = result * 10.0;    // Bump up the magnitude for each digit
    switch (value[index]) {
      // Each digit falls through the appropriate number of
      // increments before breaking at 0.
    case '9':
      result = result + 1.0;
    case '8':
      result = result + 1.0;
    case '7':
      result = result + 1.0;
    case '6':
      result = result + 1.0;
    case '5':
      result = result + 1.0;
    case '4':
      result = result + 1.0;
    case '3':
      result = result + 1.0;
    case '2':
      result = result + 1.0;
    case '1':
      result = result + 1.0;
    case '0':
      break;
    case '-':
      result = result * -1.0;
      break;
    case '.':
      fractional = 1;    // Remember the decimal
      result = result / 10.0;    // Undo the magnitude bump
      index = value_size;    // Break out of the for loop
      break;
    case 'e':
    case 'E':
      // Not yet handled
    default:
      break;
    }
  }
  if (fractional) {
    // We found a decimal point, so we need to build up
    // the fraction too. Easiest way is right-to-left.
    for (index = value_size; index >= 0; index--) {
      fraction_result = fraction_result / 10.0;    // Shift everything to be < 1
      switch (value[index]) {
      case '9':
        fraction_result = fraction_result + 1.0;
      case '8':
        fraction_result = fraction_result + 1.0;
      case '7':
        fraction_result = fraction_result + 1.0;
      case '6':
        fraction_result = fraction_result + 1.0;
      case '5':
        fraction_result = fraction_result + 1.0;
      case '4':
        fraction_result = fraction_result + 1.0;
      case '3':
        fraction_result = fraction_result + 1.0;
      case '2':
        fraction_result = fraction_result + 1.0;
      case '1':
        fraction_result = fraction_result + 1.0;
      case '0':
        break;
      case '.':
        fraction_result = fraction_result * 10.0;    // Undo the magnitude shift
        index = 0;    // Break out of the for loop
        break;
      case 'e':
      case 'E':
        // Not yet handled
      default:
        break;
      }
    }
  }
  if (fractional) {
    result = result + fraction_result;
  }
  return result;
}

int skip_space(char* value) {
  // Skips whitespace and commas. Returns the number of
  // characters that were skipped.
  if (value == 0) {
    return 0;    // Null pointer
  }

  int offset = 0;
  while ((value[offset] == ' ') | (value[offset] == ',')) {
    offset++;
  }
  return offset;
}

void run_command(char* name, char* value) {
  // This is called for each "name":value pair found in the
  // incoming JSON. This is where you should put your handler
  // code.
  // There are a few important points to note:
  //  * This function, by default, will only be called for the
  //    top-level pairs, eg. given {"a":"b", "c":{"d":"e"}} it
  //    will be called with name="a", value="b" and name="c",
  //    value={"d":"e"}. It will not be called with name="d",
  //    value="e". If you want such recursion, add it yourself
  //    by calling read_commands on your JSON objects from
  //    somewhere within this function.
  //  * The name and value pointers will be free'd automatically
  //    after the JSON parser has finished. Thus, you should not
  //    store these pointers or any derived from them. If you
  //    want some data to persist, copy its values into some
  //    memory that you manage yourself.
  //  * Likewise, do not free these pointers yourself, as that
  //    will mangle the JSON reading.
  //  * The given pointers are not C-style strings (they are
  //    not terminated). Their length is implied by their JSON
  //    encoding. The variables "name_size" and "value_size"
  //    have been set up with the respective sizes for you if
  //    you need them.
  //  * The JSON formatting is still present in the pointers'
  //    values. For example, strings still contain their quotes.
  // Other than that, happy hacking!
  int name_size = value_length(name);
  int value_size = value_length(value);

  // We can't do simple string comparison since our pointers
  // are not null-terminated. Use compare_strings instead.
  if (compare_strings(name,"read")) {
    run_read(value);    // Read pin values
  }
  if (compare_strings(name,"write")) {
    run_write(value);   // Write pin values
  }
  if (compare_strings(name,"mode")) {
    run_mode(value);    // Set pin mode
  }
  if (compare_strings(name,"query")) {
    run_query(value);
  }
}


void run_read(char* value) {
  // Reads the value of the specified pins and sends the
  // results back as JSON

  // We should have been given a JSON object containing
  // "pin":x and "type":"analog" or "type":"digital"
  int value_size = json_length(value);
  int index = 0;   // Loop index for walking the value
  int pin = -1;    // This tells us which pin to set. -1 means unknown
  int content_size = 0;    // This keeps track of the inner contents
  short type = 0;    // 0 = unknown, 1 = digital, 2 = analog
  if (value_size > 2) {    // We want some contents between our '{' and '}'
    index++;    // Skip the '{'
    // Loop until we reach the '}'
    while (index < value_size - 2) {

      if (value[index] == ' ') {
        continue;    // Whitespace is insignificant
      }
      if (value[index] == '"') {
        // We have a string. Let's see if it's what we're after

        // We have a string. Let's see if it's what we're after
        // return encoder value if that is what we need to read
        if (compare_strings(value+index, "encoder"))
        {



          // Send our result over USB
          // {"encoderValue":{"value":123}}
          // "{"pinValue":{"type":"analog", "pin":14, "value":706}}


          Serial.print("{\"pinValue\":{\"type\":\"encoder\",\"pin\":14,\"value\":");

          Serial.print(encoderPosition) ;
          Serial.print("}}\n"); // newline added by afy
          return ;
        }

        if (compare_strings(value+index, "pin")) {
          //Serial.println("Found pin");
          // This is the number of the pin to read
          // Find the associated digits
          index = index + value_length(value+index);    // Skip over the name
          while (value[index] == ' ') {       
            index++;    // Skip whitespace
          }
          if (value[index] == ':') {
            index++;    // Skip the colon
          }
          else {
            //Serial.println("...1...");
            return;     // No colon. Abort.
          }
          while (value[index] == ' ') {            
            index++;    // Skip whitespace
          }
          // Now we should be at the pin digits
          pin = (int) compile_digits(value+index);    // Turn them into an integer
          index = index + value_length(value+index);  // Skip over the digits
          while (value[index] == ' ') {            
            index++;    // Skip whitespace
          }
          if (value[index] == ',') {
            index++;    // Skip over comma separators
          }
          while (value[index] == ' ') {            
            index++;    // Skip whitespace
          }
          continue;    // Retest the while condition
        }
        if (compare_strings(value+index, "type")) {
          //Serial.println("Found type");
          // This is the type of pin to read.
          // Find out whether it's analog or digital.
          index = index + value_length(value+index);
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ':') {
            index++;    // Skip over colons
          }
          index = index+skip_space(value+index);    // Skip whitespace
          if (value[index] != '"') {
            return;    // We should have got a string. Bail out.
          }
          if (compare_strings(value+index, "digital")) {
            // Digital read
            type = 1;
          }
          if (compare_strings(value+index, "analog")) {
            // analog read
            type = 2;
          }
          index = index + value_length(value+index);    // Skip the value
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ',') {
            index++;    // Skip comma separators
          }
          index = index + skip_space(value+index);    // Skip whitespace
          continue;
        }
      }
    }
    if (pin >= 0) {
      // Send our result over USB
      // {"pinValue":{"type":"digital", "pin":123, "value":123}}
      Serial.print("{\"pinValue\":{\"type\":");
      if (type == 1) {
        Serial.print("\"digital\"");
      }
      else if (type == 2) {
        Serial.print("\"analog\"");
      }
      else {
        return;    // Error, bail out
      }
      Serial.print(", \"pin\":");
      Serial.print(pin);
      Serial.print(", \"value\":");
      if (type == 1) {
        Serial.print(digitalRead(pin));
      }
      else if (type == 2) {
        Serial.print(analogRead(pin));
      }
      else {
        return;    // Error, bail out
      }
      Serial.print("}}\n"); // newline added by afy
    }
  }
}

void run_write(char* value) {
  // Writes the specified value to the specified pin and sends
  // back some empty JSON. The type must be given, to keep the
  // code simple.

  // We should have been given a JSON object containing
  // "pin":x, "value":y and "type":"digital" or "type":"analog"
  int value_size = json_length(value);
  int index = 0;   // Loop index for walking the value
  int pin = -1;    // This tells us which pin to set. -1 means unknown
  int content_size = 0;    // This keeps track of the inner contents
  int pin_value = -1;    // The value to write. analog has a range 0-255, digital is 0 (LOW) or 1 (HIGH)
  short type = 0;    // 0 = unknown, 1 = digital, 2 = analog
  if (value_size > 2) {    // We want some contents between our '{' and '}'
    index++;    // Skip the '{'
    // Loop until we reach the '}'
    while (index < value_size - 2) {     
      if (value[index] == ' ') {
        continue;    // Whitespace is insignificant
      }
      if (value[index] == '"') {
        // We have a string. Let's see if it's what we're after
        if ((compare_strings(value+index, "pin") )||  (compare_strings(value+index, "time"))){
          // This is the number of the pin to write
          // Find the associated digits
          index = index + value_length(value+index);    // Skip over the name
          while (value[index] == ' ') {           
            index++;    // Skip whitespace
          }
          if (value[index] == ':') {
            index++;    // Skip the colon
          }
          else {
            return;     // No colon. Abort.
          }
          while (value[index] == ' ') {           
            index++;    // Skip whitespace
          }
          // Now we should be at the pin digits
          pin = (int) compile_digits(value+index);    // Turn them into an integer
          index = index + value_length(value+index);  // Skip over the digits
          while (value[index] == ' ') {           
            index++;    // Skip whitespace
          }
          if (value[index] == ',') {
            index++;    // Skip over comma separators
          }
          while (value[index] == ' ') {          
            index++;    // Skip whitespace
          }
          continue;    // Retest the while condition
        }
        if (compare_strings(value+index, "type")) {
          // This is the type of pin to write.
          // Find out whether it's analog or digital.
          index = index + value_length(value+index);  // Skip over "type"
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ':') {
            index++;    // Skip over colons
          }
          else {
            return;    // No colon. Bail out.
          }
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] != '"') {
            return;    // We should have got a string. Bail out.
          }
          if (compare_strings(value+index, "digital")) {
            // Digital read
            type = 1;
          }
          if (compare_strings(value+index, "analog")) {
            // analog read
            type = 2;
          }
          if (compare_strings(value+index, "piezo")) {
            // piezo out
            type = 3;
          }
          if (compare_strings(value+index, "servo")) {
            // servo
            type = 4;
          }

          index = index + value_length(value+index);    // Skip over the value string
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ',') {
            index++;    // Skip comma separators
          }
          index = index + skip_space(value+index);    // Skip whitespace
          continue;    // Retest the while condition
        }
        if ((compare_strings(value+index, "value") )|| (compare_strings(value+index, "freq")) ){
          // This is the value we should write to the pin.
          // We can't assume the order of our input, so
          // we just save the value without checking it
          // until later.
          index = index + value_length(value+index);  // Skip over "value"
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ':') {
            index++;    // Skip over colons
          }
          else {
            return;    // No colon. Bail out.
          }
          index = index + skip_space(value+index);    // Skip whitespace
          pin_value = (int)(compile_digits(value+index)+0.5);    // Grab the rounded value
          index = index + value_length(value+index);  // Skip over the digits
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ',') {
            index++;    // Skip commas
          }
          index = index + skip_space(value+index);    // Skip whitespace
          continue;    // Retest the while condition
        }
      }
    }
    if (((type == 1) || (type == 2) || (type == 3) || (type == 4))
      && (pin > 0) 
      && (pin_value >= 0)) {
      if (type == 1) {
        // Digital. Our value must be 0 or 1.
        if ((pin_value != 0) && (pin_value != 1)) {
          return;    // Bail out
        }
        switch (pin_value) {
        case 0:
          digitalWrite(pin,LOW);
          break;
        case 1:
          digitalWrite(pin,HIGH);
          break;
        default:
          return;    // Bail out.
        }
      }
      if (type == 2) {
        // analog. Our value must be from 0 to 255.
        if ((pin_value < 0) || (pin_value > 255)) {
          return;    // Bail out
        }
        analogWrite(pin,pin_value);
      }
      if(type == 3 )
      {
        // here we use pin for duration and  pin_value for frequency and 
        // play with the piezo
       // pin_value = 550 ;
        //pin = 1000 ;
        //Serial.println(pin) ;
        tone(3, pin_value, pin) ;
        delay(pin) ;
        noTone(3) ;

      }
      if( type == 4 ) // do servo motion
      {
     //   Servo servo ;

        servo.attach(SERVO) ;
 
        delay(100) ;
        servo.write(pin_value) ;
        delay(300) ;
        servo.detach() ;
        delay(300) ;
        //pinMode(HALL, INPUT) ;
      }

      Serial.print("{}\n");    // Indicates success newline added by afy

    }
  }
}

void run_mode(char* value) {
  // Sets the mode of a pin to input or output.

  // We should have been given a JSON object containing
  // "pin":x, "mode":"input" or "mode":"output"
  int value_size = json_length(value);
  int index = 0;   // Loop index for walking the value
  int pin = -1;    // This tells us which pin to set. -1 means unknown
  int content_size = 0;    // This keeps track of the inner contents
  short mode = 0;    // 0 for unknown, 1 for input and 2 for output
  if (value_size > 2) {    // We want some contents between our '{' and '}'
    index++;    // Skip the '{'
    // Loop until we reach the '}'
    while (index < value_size - 2) {      
      if (value[index] == ' ') {
        continue;    // Whitespace is insignificant
      }
      if (value[index] == '"') {
        // We have a string. Let's see if it's what we're after
        if (compare_strings(value+index, "pin")) {
          // This is the number of the pin to set
          // Find the associated digits
          index = index + value_length(value+index);    // Skip over the name
          while (value[index] == ' ') {            
            index++;    // Skip whitespace
          }
          if (value[index] == ':') {
            index++;    // Skip the colon
          }
          else {
            return;     // No colon. Abort.
          }
          while (value[index] == ' ') {            
            index++;    // Skip whitespace
          }
          // Now we should be at the pin digits
          pin = (int) (compile_digits(value+index)+0.5);    // Turn them into an integer
          index = index + value_length(value+index);  // Skip over the digits
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ',') {
            index++;    // Skip over comma separators
          }
          index = index + skip_space(value+index);    // Skip whitespace
          continue;    // Retest the while condition
        }
        if (compare_strings(value+index, "mode")) {
          // This is the mode to set.
          // Find out whether it's input or output.
          index = index + value_length(value+index);  // Skip over "mode"
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ':') {
            index++;    // Skip over colons
          }
          else {
            return;    // No colon. Bail out.
          }
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] != '"') {
            return;    // We should have got a string. Bail out.
          }
          if (compare_strings(value+index, "input")) {
            // Input mode
            mode = 1;
          }
          if (compare_strings(value+index, "output")) {
            // Output mode
            mode = 2;
          }
          index = index + value_length(value+index);    // Skip over the value string
          index = index + skip_space(value+index);    // Skip whitespace
          if (value[index] == ',') {
            index++;    // Skip comma separators
          }
          index = index + skip_space(value+index);    // Skip whitespace
          continue;    // Retest the while condition
        }
      }
    }
    if (((mode == 1) || (mode == 2)) && (pin > 0)) {
      if (mode == 1) {
        // Input
        pinMode(pin,INPUT);
      }
      if (mode == 2) {
        // Output
        pinMode(pin,OUTPUT);
      }
      Serial.print("{}\n");    // Indicates success newline added afy
    }
  }
}

void run_query(char* value) {
  // We use "query" as a generic name when all we want
  // to send is a value. We simply branch based on the
  // contents of value (we'd use a switch, but we would
  // have to overload comparison with compare_strings)
  if (compare_strings(value, "status")) {
    // This is a generic ping request. We just report
    // that we're ready so that whatever's on the other
    // end knows that it can send commands to us.
    Serial.println("{\"status\":\"ready\"}");
  }
}



