/*
* The GENT: Good Enough Network Tool
 * Built from the foundation laid down by the Portable Arduino Network Tool
 * PANT made by the following:
 * Mike Cherry: mcherry@inditech.org
 * Eric Brundick: spirilis@linux.com
 * Blake Foster: blfoster@vassar.edu
 * 
 * GENT tweaked pretty hard by David McGuire (FTPMonster on Arduino forum)
 * See README for more information or email dmcguire@mxmtechnologies.com
 */

#include <LiquidCrystal.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Dns.h>
#include <utility/w5100.h>
#include <Keypad.h>
#include <Wire.h>

// sourced from http://forum.arduino.cc/index.php/topic,161155.0.html and https://github.com/BlakeFoster/Arduino-Ping
// written by Blake Foster - http://www.blake-foster.com/contact.php
#include <ICMPPing.h>

#define GENT_VERSION "1.0"
#define INFO_URL "coming soon"


#define LONG_DELAY 1750
#define MEDIUM_DELAY 500
#define SHORT_DELAY 250
#define MICRO_DELAY 150

#define PROMPT ">"
#define CANCEL "Canceled        "
#define STATUS_OK "."
#define STATUS_GO "o"
#define STATUS_FAIL "x"
#define BLANK "                "

//#define buttonUp 0
#define buttonUp 2
//#define buttonDown 1
#define buttonDown 8
//#define buttonSelect 2
#define buttonSelect 6
//#define buttonBack 3
#define buttonBack 4

// #define EEPROM_PAGESIZE 16

boolean buttons[4];

// Ethernet mac address
byte mac[6] = { 
  0x90, 0xA2, 0xDA, 0x00, 0xC1, 0x92 };

// internet server to ping and another to perform dns lookup
byte internetIp[4] = { 
  4, 2, 2, 2 };
byte defaultIp[4] = { 
  1, 1, 1, 1 };
char internetServer[11] = "google.com";

int ethernetActive = 0;
int ethernetFailed = 0;
char pingBuffer[20];

// buffers to be reused for misc message printing
char line0[17];
char line1[17];

// for the current on-screen menu
int CurrentPage = 0;
int CurrentMenuItem = 0;
int CursorPosition = 0;

//int lastPingTotalPackets = 0;

unsigned long netsize;
unsigned long current;
byte subnet[4];

// keep track of a few metrics
IPAddress myLocalIp;
IPAddress mySubnetMask;

// the icmp ping library seems to not be reliable if using any other socket
// No longer the case. Resetting to 0 as per examples
SOCKET pingSocket = 0;

// initialize lcd & ethernet
LiquidCrystal lcd(22, 23, 24, 25, 26, 27);
//LiquidTWI lcd(0);
EthernetClient client;

// initialize keypad 
const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns
// Define the Keymap
char keys[ROWS][COLS] = {
  {
    '1','2','3'              }
  ,
  {
    '4','5','6'              }
  ,
  {
    '7','8','9'              }
  ,
  {
    '.','0','E'              }
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte rowPins[ROWS] = { 
  17, 18, 19, 20 };
// Connect keypad COL0, COL1 and COL2 to these Arduino pins.
byte colPins[COLS] = { 
  16, 15, 14 }; 

// Create the Keypad
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
// for portScanner
int portcount[1024]; 
// for HostDiscovery
byte PingedHostList[256][4];
// for backlight
int BacklightPin = 46;
// for power
int PowerPin = 48;
// for battery indicator
int BatteryPin1 = 50;
int BatteryPin2 = 52;
// for battery monitoring
int BatteryMonitorPin = 0;


// print a message to a given column and row, optionally clearing the screen
void lcdPrint(int column, int row, char message[], boolean clrscreen = false)
{
  if (clrscreen == true) lcd.clear();

  lcd.setCursor(column, row);
  lcd.print(message);
} // lcdprint

// BatteryCheck returns the assumed percentage left on the battery.
// for testing, I am using 8 AA batteries (don't get me started), for 12V.
// The Mega stops working at 7V, and I have a voltage divider in place, returning 6V.
// Let's see how this works.
// Returns the actual number, not multiplied by 100
double BatteryCheck()
{
  int val = 0;  
  val = analogRead(BatteryMonitorPin);
  float percentage = (float)val / 1023;
  return percentage;
}
// Sets backlight. Backlight is controlled by a specific pin 
// in our demo, it's Pin 30.
void ChangeBacklight()
{
  char buffer0[17];
  int val=0;
  val = digitalRead(BacklightPin);
  if (val == 1) {
    digitalWrite(BacklightPin, LOW);
  }
  else {
    digitalWrite(BacklightPin, HIGH);
  }
  return;
} // ChangeBacklight
// Powers down system. Power is controlled by a specific pin 
// in our demo, it's Pin 31.
// rest of circuit is a Pololu switch
void ShutDown()
{
    digitalWrite(PowerPin, HIGH);
  return;
} // ShutDown

// This will show the items in "portcount", an array for ports.
// Ports were determined by portScanner()
void showPortcount(int menuItems, int offset = 0)
{
  int menuPosition = 0;
  int buttonClick = 0;
  char buffer0[17];
  char buffer1[17];

  while (1)
  {
    if (buttonClick == 1) buttonClick = 0;
    sprintf(buffer0, "%d", portcount[menuPosition]);
    lcdPrint(offset, 0, buffer0, true);

    if ((menuPosition+1) < menuItems)
    {
      sprintf(buffer1, "%d", portcount[menuPosition+1]);
      lcdPrint(offset, 1, buffer1);
    }

    //    delay(SHORT_DELAY);

    while (buttonClick == 0)
    {
      readButtons();

      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }

      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }

      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
} //showPortcount

// This will show the items in "PingedHostList", an array of strings for pinged addresses.
// Ports were determined by hostDiscovery()
void showHostcount(int menuItems, int offset = 0)
{
  int menuPosition = 0;
  int buttonClick = 0;
  char buffer0[17];
  char buffer1[17];
  while (1)
  {
    if (buttonClick == 1) buttonClick = 0;
    sprintf(line0, "%d.%d.%d.%d", PingedHostList[menuPosition][0], PingedHostList[menuPosition][1], PingedHostList[menuPosition][2], PingedHostList[menuPosition][3]);
    sprintf(buffer0, "%-15s", line0);
    lcdPrint(offset, 0, buffer0, true);

    if ((menuPosition+1) < menuItems)
    {
      sprintf(line1, "%d.%d.%d.%d", PingedHostList[menuPosition+1][0], PingedHostList[menuPosition+1][1], PingedHostList[menuPosition+1][2], PingedHostList[menuPosition+1][3]);
      sprintf(buffer1, "%-15s", line1);
      lcdPrint(offset, 1, buffer1);
    }

    while (buttonClick == 0)
    {
      readButtons();

      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }

      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }

      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
} //showHostcount


// SetIPAddress will config the device's IP. There's a series of sub-functions
// required as well. 
void SetIPAddress()
// this takes input from the keypad and sets the IP address of the Arduino.

{
  // set variables
  boolean IPAddressCheck = false;
  boolean SubnetAddressCheck = false;
  boolean GatewayAddressCheck = false;
  boolean DNSAddressCheck = false;
  byte NewIP[4];
  byte NewSubnet[4];
  byte NewGateway[4];
  byte NewDNS[4];
  String enteredIPAddressString;
  String enteredSubnetAddressString;
  String enteredGatewayAddressString;
  String enteredDNSAddressString;
  // clear the screen
  lcd.clear();
  do {
    lcdPrint(0,0,"Enter IP:",true);
    lcd.setCursor(0, 1);
    enteredIPAddressString = readKeypad("Enter IP:");
    IPAddressCheck = IPValidator(enteredIPAddressString);
  } 
  while (IPAddressCheck == false);
  ByteToArray(enteredIPAddressString,NewIP);
  do {
    lcdPrint(0,0,"Enter Subnet:",true);
    lcd.setCursor(0, 1);
    enteredSubnetAddressString = readKeypad("Enter Subnet:");
    SubnetAddressCheck = SubnetValidator(enteredSubnetAddressString);
  } 
  while (SubnetAddressCheck == false);
  ByteToArray(enteredSubnetAddressString,NewSubnet);
  do {
    lcdPrint(0,0,"Enter Gateway:",true);
    lcd.setCursor(0, 1);
    enteredGatewayAddressString = readKeypad("Enter Gateway:");
    GatewayAddressCheck = IPValidator(enteredGatewayAddressString);
  } 
  while (GatewayAddressCheck == false);
  ByteToArray(enteredGatewayAddressString,NewGateway);
  do {
    lcdPrint(0,0,"Enter DNS:",true);
    lcd.setCursor(0, 1);
    enteredDNSAddressString = readKeypad("Enter DNS:");
    DNSAddressCheck = IPValidator(enteredDNSAddressString);
  } 
  while (GatewayAddressCheck == false);
  ByteToArray(enteredDNSAddressString,NewDNS);
  lcdPrint(0,0,"Changing IP...",true);
  Ethernet.begin(mac,NewIP,NewDNS,NewGateway,NewSubnet);
  delay(MEDIUM_DELAY);
  lcdPrint(0,0,"IP Address",true);
  lcdPrint(0,1,"Changed Successfully",false);
  delay(MEDIUM_DELAY);
  delay(MEDIUM_DELAY);
  myLocalIp = Ethernet.localIP();
  mySubnetMask = Ethernet.subnetMask();


} // SetIPAddress
String readKeypad(String FirstLine)
// Takes a string for the first line of the LCD. Needed in case of overrun.
// Returns a string taken from the keypad. 
{
  String ReturnedInfo;
  char enteredinfo[16];
  int count = 0;
  while(1){
    char key = kpd.getKey();  
    if (key != NO_KEY)
    {
      switch(key){
      case 'E': 
        ReturnedInfo = enteredinfo;
        // a little cleanup
        memset(enteredinfo, 0, (sizeof(enteredinfo)/sizeof(enteredinfo[0])));
        count = 0;
        ReturnedInfo.trim();
        return ReturnedInfo;
        break;
      default:
        if (key != 'E') {
          lcd.print(key);
          enteredinfo[count] = key;
          enteredinfo[count+1] = '\0';
        }
        if (count == 17)
        {
          lcd.clear();
          count=-1;
          memset(enteredinfo, 0, (sizeof(enteredinfo)/sizeof(enteredinfo[0])));
          lcd.print(FirstLine);
          lcd.setCursor(0, 1);
        } // nested if
        count++;
      } // switch
    } // char key
  } // while

} // readKeypad
boolean IPValidator(String IPAddress)
// This will confirm the validity of a given IP address.
// It checks for octets and if the number is under 255.
// returns false if a problem, true if OK
{
  // first, look for octets. Distinguished by a period.
  // if we don't have three periods, bail. 
  // If we do, record location. We'll need it later.
  int FirstOctetLoc = IPAddress.indexOf('.');
  int SecondOctetLoc = IPAddress.indexOf('.', FirstOctetLoc+1);
  int ThirdOctetLoc = IPAddress.indexOf('.',SecondOctetLoc+1);
  if (FirstOctetLoc == -1 || SecondOctetLoc == -1 || ThirdOctetLoc == -1) {
    return false;
  }
  char IPAddressBuffer[IPAddress.length()];
  IPAddress.toCharArray(IPAddressBuffer,IPAddress.length());
  // First Octet
  String FirstOctetString = IPAddress.substring(0,FirstOctetLoc);
  char FirstOctetChar[4];
  FirstOctetString.toCharArray(FirstOctetChar,6);
  int FirstOctet=atoi(FirstOctetChar);
  // Second Octet
  String SecondOctetString = IPAddress.substring(FirstOctetLoc+1,SecondOctetLoc);
  char SecondOctetChar[4];
  SecondOctetString.toCharArray(SecondOctetChar,6);
  int SecondOctet=atoi(SecondOctetChar);
  // Third Octet
  String ThirdOctetString = IPAddress.substring(SecondOctetLoc+1,ThirdOctetLoc);
  char ThirdOctetChar[4];
  ThirdOctetString.toCharArray(ThirdOctetChar,6);
  int ThirdOctet=atoi(ThirdOctetChar);
  // Fourth Octet
  String FourthOctetString = IPAddress.substring(ThirdOctetLoc+1,IPAddress.length());
  char FourthOctetChar[4];
  FourthOctetString.toCharArray(FourthOctetChar,6);
  int FourthOctet=atoi(FourthOctetChar);

  if (FirstOctet > 255 || SecondOctet > 255 || ThirdOctet > 255 || FourthOctet > 255){
    return false;
  }
  else if (FirstOctetLoc == 0 || SecondOctetLoc-FirstOctetLoc == 1 || ThirdOctetLoc-SecondOctetLoc == 1 || IPAddress.length() - ThirdOctetLoc == 1) {
    return false;
  }
  else {
    return true;
  }
} // IPValidator()
boolean SubnetValidator(String SubnetMask)
{
  // Subnet checking
  // true if OK, false if not
  // First see if we have 4 octets of information.
  int FirstSubnetOctetLoc = SubnetMask.indexOf('.');
  int SecondSubnetOctetLoc = SubnetMask.indexOf('.', FirstSubnetOctetLoc+1);
  int ThirdSubnetOctetLoc = SubnetMask.indexOf('.',SecondSubnetOctetLoc+1);
  if (FirstSubnetOctetLoc == -1 || SecondSubnetOctetLoc == -1 || ThirdSubnetOctetLoc == -1) {
    return false;
  }
  char SubnetMaskBuffer[SubnetMask.length()];
  SubnetMask.toCharArray(SubnetMaskBuffer,SubnetMask.length());
  // First Octet
  String FirstSubnetOctetString = SubnetMask.substring(0,FirstSubnetOctetLoc);
  char FirstSubnetOctetChar[4];
  FirstSubnetOctetString.toCharArray(FirstSubnetOctetChar,4);
  int FirstSubnetOctet=atoi(FirstSubnetOctetChar);
  // Second Octet
  String SecondSubnetOctetString = SubnetMask.substring(FirstSubnetOctetLoc+1,SecondSubnetOctetLoc);
  char SecondSubnetOctetChar[4];
  SecondSubnetOctetString.toCharArray(SecondSubnetOctetChar,4);
  int SecondSubnetOctet=atoi(SecondSubnetOctetChar);
  // Third Octet
  String ThirdSubnetOctetString = SubnetMask.substring(SecondSubnetOctetLoc+1,ThirdSubnetOctetLoc);
  char ThirdSubnetOctetChar[4];
  ThirdSubnetOctetString.toCharArray(ThirdSubnetOctetChar,6);
  int ThirdSubnetOctet=atoi(ThirdSubnetOctetChar);
  // Fourth Octet
  String FourthSubnetOctetString = SubnetMask.substring(ThirdSubnetOctetLoc+1,SubnetMask.length());
  char FourthSubnetOctetChar[4];
  FourthSubnetOctetString.toCharArray(FourthSubnetOctetChar,6);
  int FourthSubnetOctet=atoi(FourthSubnetOctetChar);
  // check against subnet list
  int firstsubnetreturnvalue = subnetchecker(FirstSubnetOctet);
  int secondsubnetreturnvalue = subnetchecker(SecondSubnetOctet);
  int thirdsubnetreturnvalue = subnetchecker(ThirdSubnetOctet);
  int fourthsubnetreturnvalue = subnetchecker(FourthSubnetOctet);
  if (firstsubnetreturnvalue == 1 || secondsubnetreturnvalue == 1 || thirdsubnetreturnvalue == 1 || fourthsubnetreturnvalue == 1) {
    return false;
  }
  else if (FirstSubnetOctetLoc == 0 || SecondSubnetOctetLoc-FirstSubnetOctetLoc == 1 || ThirdSubnetOctetLoc-SecondSubnetOctetLoc == 1 || SubnetMask.length() - ThirdSubnetOctetLoc == 1) {
    return false;
  }
  else {
    return true;
  }

} // SubnetValidator()
int subnetchecker(int testNumber) 
// 0 if OK, 1 if failed
{
  if (testNumber >= 0 && testNumber < 0x100)  // optionally check in 8 bit unsigned range
  {
    byte n = (byte) testNumber ;
    if( (byte)((n & -n) + n) == 0) // Take 3
    {
      return 0;
    }
    else return 1;
  }
} //subnetchecker

void ByteToArray (String inputstring, byte inputarray[])
{
  int FirstOctetLoc = inputstring.indexOf('.');
  int SecondOctetLoc = inputstring.indexOf('.', FirstOctetLoc+1);
  int ThirdOctetLoc = inputstring.indexOf('.',SecondOctetLoc+1);
  char InputStringBuffer[inputstring.length()];
  inputstring.toCharArray(InputStringBuffer,inputstring.length());
  // First Octet
  String FirstOctetString = inputstring.substring(0,FirstOctetLoc);
  char FirstOctetChar[4];
  FirstOctetString.toCharArray(FirstOctetChar,6);
  int FirstOctet=atoi(FirstOctetChar);
  // Second Octet
  String SecondOctetString = inputstring.substring(FirstOctetLoc+1,SecondOctetLoc);
  char SecondOctetChar[4];
  SecondOctetString.toCharArray(SecondOctetChar,6);
  int SecondOctet=atoi(SecondOctetChar);
  // Third Octet
  String ThirdOctetString = inputstring.substring(SecondOctetLoc+1,ThirdOctetLoc);
  char ThirdOctetChar[4];
  ThirdOctetString.toCharArray(ThirdOctetChar,6);
  int ThirdOctet=atoi(ThirdOctetChar);
  // Fourth Octet
  String FourthOctetString = inputstring.substring(ThirdOctetLoc+1,inputstring.length());
  char FourthOctetChar[4];
  FourthOctetString.toCharArray(FourthOctetChar,6);
  int FourthOctet=atoi(FourthOctetChar);
  inputarray[0] = FirstOctet;
  inputarray[1] = SecondOctet; 
  inputarray[2] = ThirdOctet; 
  inputarray[3] = FourthOctet;  
} // ByteToArray


// ping a host and return number of packets lost
// send 10 pings by default
// updated to reflect newest ICMPPing header
int pingHost(IPAddress ip, char label[], int pings = 10)
{
  // initialize variables
  char buffer [256];
  ICMPPing ping(pingSocket, (uint16_t)random(0, 255));

  byte thisIp[] = { 
    ip[0], ip[1], ip[2], ip[3]       };
  // Need to be able to put in our own IP address for testing.
  int packetLoss = 0;
  int packets = 0;
  int pingNo = 0;
  int hasHadPacketLoss = 0;
  int pingloop = 0;
  int linePos = 0;
  int sentPackets = 0;
  // needed for IP validation
  boolean IPAddressCheck = false;
  String enteredIPAddressString;
  byte NewIP[4];
  union ArrayToInteger {
    byte array[4];
    uint32_t integer;
  };
  if (ip[0] == 1 && ip[1] == 1 && ip[2]== 1 && ip[3] == 1) {
    //readKeypad("Enter IP to ping:")
    do {
      lcdPrint(0,0,"Enter ping IP:",true);
      lcd.setCursor(0, 1);
      enteredIPAddressString = readKeypad("Enter ping IP:");
      IPAddressCheck = IPValidator(enteredIPAddressString);
    } 
    while (IPAddressCheck == false);
    ByteToArray(enteredIPAddressString,NewIP);
    ArrayToInteger converter; //Create a converter
    converter.array[0] = NewIP[0] ; //save something to each byte in the array
    converter.array[1] = NewIP[1] ; //save something to each byte in the array
    converter.array[2] = NewIP[2] ; //save something to each byte in the array
    converter.array[3] = NewIP[3] ; //save something to each byte in the array
    IPAddress convertedIP(converter.integer);
    thisIp[0] = convertedIP[0];
    thisIp[1] = convertedIP[1];
    thisIp[2] = convertedIP[2];
    thisIp[3] = convertedIP[3];
  }

  // show what you're pinging
  lcdPrint(0, 0, label, true);
  sprintf(line1, "%d.%d.%d.%d", thisIp[0], thisIp[1], thisIp[2], thisIp[3]);
  lcdPrint(0, 1, line1);
  // clear line 1 after giving the user time to read which IP is being pinged
  delay(LONG_DELAY);
  lcdPrint(0, 1, BLANK);
  // allows us to do a quick ping
  if (pings == 0)
  {
    pings = 2;
    pingloop = 1;
  }
  // the big 'for' loop
  for (pingNo = 0; pingNo < pings; pingNo++)
  {
    // make sure that if we have a 'cancel' command,
    // actually cancel it out
    readButtons();
    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);
      delay(MEDIUM_DELAY);
      if (pingloop == 1)
      {
        break;
      }
      return packetLoss;
    }
    // send a single ping with a 4 second timeout, returning the result in pingBuffer

    ICMPEchoReply echoReply = ping(thisIp, 4);
    if (echoReply.status != SUCCESS)
    {
      //     packet timeout results in printing a 'x' on-screen
      lcdPrint(linePos, 1, STATUS_FAIL);
      packetLoss++;
      hasHadPacketLoss = 1;
    }
    else
    {
      // successful packet results in printing a '.' on-screen
      if (pingloop == 1)
      {
        lcdPrint(linePos, 1, STATUS_GO);
        hasHadPacketLoss = 0;
      }
      else
      {
        lcdPrint(linePos, 1, STATUS_OK);
      }
    }

    // wait minimum between pings
    delay(MEDIUM_DELAY);

    if (pingloop == 1)
    {
      if (hasHadPacketLoss == 0) lcdPrint(linePos, 1, STATUS_OK);

      pingNo = 0;
    }

    linePos++;
    sentPackets++;
    if (linePos == 16) linePos = 0;
  } // end of for loop

  // successful number of packets received
  packets = (sentPackets - packetLoss);

  sprintf(line1, "%d/%d received", packets, sentPackets);
  lcdPrint(0, 1, line1);

  delay(LONG_DELAY);

  return packetLoss;
} // pinghost()
// test if name resolution works using dns server obtained via dhcp
int dnsTest()
{
  DNSClient dnsLookup;
  IPAddress ipResolved;

  dnsLookup.begin(Ethernet.dnsServerIP());
  int err = dnsLookup.getHostByName(internetServer, ipResolved);

  if (err == 1)
  {
    sprintf(line0, "%d.%d.%d.%d", ipResolved[0], ipResolved[1], ipResolved[2], ipResolved[3]);

    lcdPrint(0, 0, internetServer, true);
    lcdPrint(0, 1, line0);
  }
  else
  {
    lcdPrint(0, 0, "DNS failed", true);
  }

  delay(LONG_DELAY);

  return err;
}

// run all available tests
void testNetwork()
{
  int gwPingLoss, dnsPingLoss, extPingLoss, err;
  int dnsFailed, gwFailed, extFailed;

  gwPingLoss = pingHost(Ethernet.gatewayIP(), "Ping gateway"); 
  dnsPingLoss = pingHost(Ethernet.dnsServerIP(), "Ping DNS");
  err = dnsTest();
  extPingLoss = pingHost(internetIp, "Ping ext host");

  // basic test only sends 10 pings. if number of lost packets is 10, 100% packet loss
  if (gwPingLoss == 10) gwFailed = 1;
  if (dnsPingLoss == 10) dnsFailed = 1;
  if (extPingLoss == 10) extFailed = 1;

  // figure out if we are missing any packets and show the user
  if ((gwPingLoss > 0) || (dnsPingLoss > 0) || (extPingLoss > 0))
  {
    int line = 0;

    lcdPrint(0, 0, "Network OK", true);
    lcdPrint(0, 1, "w/packet loss");

    delay(LONG_DELAY);

    if (gwPingLoss > 0)
    {
      sprintf(line0, "GW %d0%% Loss", gwPingLoss);
      lcdPrint(0, line, line0);
      line = 1;
    }

    if (dnsPingLoss > 0)
    {
      sprintf(line0, "DNS %d0%% Loss", dnsPingLoss);
      lcdPrint(0, line, line0);
      line = 1;
    }

    if (extPingLoss > 0)
    {
      if (line > 1)
      {
        delay(LONG_DELAY);
        lcd.clear();

        line = 0;
      }

      sprintf(line0, "EXT %d0%% Loss", extPingLoss);
      lcdPrint(0, line-1, line0);
    }

    delay(LONG_DELAY);
  } 
  else {
    lcdPrint(0, 0, "Network OK", true);
    delay(LONG_DELAY);
  }
} // testnetwork

// determine how many pages based on how many menu items we have
int PageCount(int MenuItems)
{
  int pages = 1;

  if (MenuItems > 2)
  {
    pages = (MenuItems / 2);

    if (MenuItems % 2 != 0) pages++;
  }

  return pages;
}

// display the first page of a menu
void printMenu(char *Menu[], int MenuItems)
{
  CursorPosition = 0;
  CurrentPage = 0;
  CurrentMenuItem = 0;

  lcd.clear();

  lcdPrint(0, CursorPosition, PROMPT);
  lcdPrint(2, 0, Menu[CurrentMenuItem]);

  if ((CurrentMenuItem + 1) < (MenuItems)) lcdPrint(2, 1, Menu[CurrentMenuItem + 1]);

  delay(SHORT_DELAY);
}

// advance the cursor (PROMPT) to the next menu item
void CursorNext(char *Menu[], int MenuItems)
{
  if ((CursorPosition == 0))
  {
    // we are at the begining of a menu
    if ((CurrentMenuItem + 1) < (MenuItems))
    {
      lcdPrint(0, 0, " ");
      lcdPrint(0, 1, PROMPT);

      CursorPosition++;
      CurrentMenuItem++;
    }
  }
  else
  {
    // we are already on line 1 of the currently displayed menu or
    // we are displaying pages if info
    // so we need to advance to the next page
    if (CurrentPage < (PageCount(MenuItems) - 1))
    {
      lcd.clear();

      CurrentPage++;
      CurrentMenuItem++;

      CursorPosition = 0;

      lcdPrint(0, CursorPosition, PROMPT);
      lcdPrint(2, 0, Menu[CurrentMenuItem]);

      // more than 1 item to display on this page
      if ((CurrentMenuItem + 1) < (MenuItems)) lcdPrint(2, 1, Menu[CurrentMenuItem + 1]);
    }
  }

  delay(SHORT_DELAY);
}

// move the cursor (PROMPT) to the previous menu item
void CursorPrevious(char *Menu[], int MenuItems)
{
  if ((CursorPosition == 1))
  {
    // we are already on line 1 of the display, so just move the cursor up 1 item
    if (CurrentMenuItem > 0)
    {
      lcdPrint(0, 1, " ");
      lcdPrint(0, 0, PROMPT);

      CursorPosition--;
      CurrentMenuItem--;
    }
  }
  else
  {
    if (CurrentPage > 0)
    {
      // we are already at line 0 but there is another page before this one
      // so clear the screen and move to the previous page
      lcd.clear();

      CurrentPage--;
      CurrentMenuItem--;

      CursorPosition = 1;

      lcdPrint(2, 0, Menu[CurrentMenuItem - 1]);
      lcdPrint(2, 1, Menu[CurrentMenuItem]);

      lcdPrint(0, 1, PROMPT);
    }
  }

  delay(SHORT_DELAY);
}

// display diagnostics menu items
void diagMenu()
{
  char *DiagMenu[5] = { 
    "All Tests", "Ping Ext Host", "Ping Gateway", "Ping DNS", "DNS Resolve"               };

  printMenu(DiagMenu, 5);

  while (1)
  {
    readButtons();

    // back button
    if (buttons[buttonBack] == HIGH) return;

    // up button  
    if (buttons[buttonUp] == HIGH) CursorPrevious(DiagMenu, 5);

    // down button
    if (buttons[buttonDown] == HIGH) CursorNext(DiagMenu, 5);

    // select button
    if (buttons[buttonSelect] == HIGH)
    {
      int gwPingLoss, dnsPingLoss, err, extPingLoss;
      switch (CurrentMenuItem)
      {
      case 0:
        testNetwork();
        break;

      case 1:
        extPingLoss = pingHost(defaultIp, "Ping ext host");
        break;

      case 2:
        gwPingLoss = pingHost(Ethernet.gatewayIP(), "Ping gateway");

        break;

      case 3:
        dnsPingLoss = pingHost(Ethernet.dnsServerIP(), "Ping DNS");
        break;

      case 4:
        err = dnsTest();
        break;
      }

      printMenu(DiagMenu, 5);
    }
  }
}

// display info screen
// can scroll through the info with up/down
void infoMenu()
{
  // info from DHCP
  IPAddress myGwIp = Ethernet.gatewayIP();
  IPAddress myDnsIp = Ethernet.dnsServerIP();
  myLocalIp = Ethernet.localIP();
  mySubnetMask = Ethernet.subnetMask();

  // starting out at the begining of the display
  int menuPosition = 0;
  int buttonClick = 0;
  int menuItems = 7;
  // initialize currentlevel
  double currentlevel;

  while (1)
  {
    // if a button was clicked previously, reset it to 'unclicked'
    if (buttonClick == 1) buttonClick = 0;

    switch (menuPosition)
    {
    case 0:
      sprintf(line0, "IP Address");
      sprintf(line1, "%d.%d.%d.%d", myLocalIp[0], myLocalIp[1], myLocalIp[2], myLocalIp[3]);
      break;

    case 1:
      sprintf(line0, "Gateway");
      sprintf(line1, "%d.%d.%d.%d", myGwIp[0], myGwIp[1], myGwIp[2], myGwIp[3]);
      break;

    case 2:
      sprintf(line0, "DNS");
      sprintf(line1, "%d.%d.%d.%d", myDnsIp[0], myDnsIp[1], myDnsIp[2], myDnsIp[3]);
      break;

    case 3:
      sprintf(line0, "Subnet Mask");
      sprintf(line1, "%d.%d.%d.%d", mySubnetMask[0], mySubnetMask[1], mySubnetMask[2], mySubnetMask[3]);
      break;

    case 4:
      sprintf(line0, "MAC Address");
      sprintf(line1, "%02x%02x.%02x%02x.%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      break;

    case 5:
      sprintf(line0, "Battery voltage");
      currentlevel = BatteryCheck();
      currentlevel = 5 * currentlevel;
      
      Serial.print("InfoMenu: Voltage is listed as ");
      Serial.println(currentlevel);
      char voltagebuffer[6];
      sprintf(line1, dtostrf(currentlevel, 4, 2, voltagebuffer));
      break;

    case 6:
      sprintf(line0, "Backlight");
      int val=0;
      val = digitalRead(BacklightPin);
      if (val == 1) {
        sprintf(line1,"Backlight on");
      }
      else {
        sprintf(line1,"Backlight off");
      }
      break;

    }

    // display output compiled above
    lcdPrint(0, 0, line0, true);
    lcdPrint(0, 1, line1);

    // small delay to prevent buttons from repeating too fast
    delay(SHORT_DELAY);

    while (buttonClick == 0)


    {
      readButtons();
      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }

      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }

      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
} // infomenu

// display about pages
void aboutMenu()
{
  int menuPosition = 0;
  int buttonClick = 0;
  //int menuItems = 7;
  int menuItems = 3;

  while (1)
  {
    if (buttonClick == 1) buttonClick = 0;

    switch (menuPosition)
    {
    case 0:
      sprintf(line0, "GENT Version");
      sprintf(line1, "%s", GENT_VERSION);
      break;

    case 1:
      sprintf(line0, "More Info");
      sprintf(line1, INFO_URL);
      break;

    case 2:
      sprintf(line0, "For Arnold H");
      sprintf(line1, "And Carol M");
      break;      
    }

    lcdPrint(0, 0, line0, true);
    lcdPrint(0, 1, line1);

    delay(SHORT_DELAY);

    while (buttonClick == 0)
    {
      readButtons();

      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }

      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }

      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
}

// calculate subnet based on subnet mask and ip address
// written by Eric Brundick <spirilis@linux.com>
// modified by Mike Cherry <mcherry@inditech.org>
void iplist_define(byte ipaddr[], byte subnetmask[])
{
  int i;

  for (i=31; i>=0; i--)
  {
    if (subnetmask[i/8] & (1 << (7-i%8)))
    {
      current = 0;

      netsize = ((unsigned long)1) << (31-i);

      for (int a = 0; a <= 3; a++) subnet[a] = ipaddr[a] & subnetmask[a];

      return;
    }
  }
} // iplist_define

// get the next IP address in the range
// written by Eric Brundick <spirilis@linux.com>
// modified by Mike Cherry <mcherry@inditech.org>
boolean iplist_next(byte nextip[])
{
  if (current < netsize) {
    for (int a = 0; a <= 3; a++) nextip[a] = subnet[a];

    if (current & 0x000000FF)
    {
      nextip[3] |= (byte) (current & 0x000000FF);
    }
    if (current & 0x0000FF00)
    {
      nextip[2] |= (byte) ((current & 0x0000FF00) >> 8);
    }
    if (current & 0x00FF0000)
    {
      nextip[1] |= (byte) ((current & 0x00FF0000) >> 16);
    }

    if (current & 0xFF000000)
    {
      nextip[0] |= (byte) ((current & 0xFF000000) >> 24);
    }

    current++;

    // dont want to ping the network or broadcast address
    if (nextip[3] == 0x00 || nextip[3] == 0xFF)
    {
      return iplist_next(nextip);
    }

    return true;
  }

  return false;
}


// discover hosts responding to ping on the network
// based on netmask and ip address
int hostDiscovery()
{
  boolean iptest, hostcheck;

  unsigned long hostcount = 0;
  unsigned long pingedhosts = 0;
  unsigned long startaddr = 0;

  byte validIp[4];

  char buffer0[17];
  //char buffer1[17];
  boolean SubnetAddressCheck = false;
  byte NewSubnet[4];
  String enteredSubnetAddressString;
  union ArrayToInteger {
    byte array[4];
    uint32_t integer;
  };

  // convert IPAddress's into byte arrays
  byte IPAsByte[] = { 
    myLocalIp[0], myLocalIp[1], myLocalIp[2], myLocalIp[3]               };
  //byte NMAsByte[] = { mySubnetMask[0], mySubnetMask[1], mySubnetMask[2], mySubnetMask[3] };
  byte NMAsByte[4];
  do {
    lcdPrint(0,0,"Enter Subnet:",true);
    lcd.setCursor(0, 1);
    enteredSubnetAddressString = readKeypad("Enter Subnet:");
    SubnetAddressCheck = SubnetValidator(enteredSubnetAddressString);
  } 
  while (SubnetAddressCheck == false);
  ByteToArray(enteredSubnetAddressString,NewSubnet);
  ArrayToInteger converter; //Create a converter
  converter.array[0] = NewSubnet[0] ; //save something to each byte in the array
  converter.array[1] = NewSubnet[1] ; //save something to each byte in the array
  converter.array[2] = NewSubnet[2] ; //save something to each byte in the array
  converter.array[3] = NewSubnet[3] ; //save something to each byte in the array
  IPAddress subnetmask_default(converter.integer);

  if (subnetmask_default[0] == 0)
  {
    return 0;
  }
  else
  {
    NMAsByte[0] = subnetmask_default[0];
    NMAsByte[1] = subnetmask_default[1];
    NMAsByte[2] = subnetmask_default[2];
    NMAsByte[3] = subnetmask_default[3];
  }


  // setup the netrange list
  iplist_define(IPAsByte, NMAsByte);

  lcdPrint(0, 0, "Found", true);

  // try to get the first ip in the range
  while (iplist_next(validIp) == true)
  {
    // process back button to cancel search
    readButtons();

    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);

      delay(MEDIUM_DELAY);

      break;
    }
    sprintf(line1, "%d.%d.%d.%d", validIp[0], validIp[1], validIp[2], validIp[3]);
    sprintf(buffer0, "%-16s", line1);
    lcdPrint(0, 1, buffer0);
    ICMPPing ping(pingSocket, (uint16_t)random(0, 255));
    // ping a host
    ICMPEchoReply echoReply = ping(validIp, 4);
    if (echoReply.status == SUCCESS)
    {
      PingedHostList[hostcount][0] = validIp[0];
      PingedHostList[hostcount][1] = validIp[1];
      PingedHostList[hostcount][2] = validIp[2];
      PingedHostList[hostcount][3] = validIp[3];
      hostcount++;
    }


    sprintf(line0, "%d", hostcount);
    lcdPrint(6, 0, line0);

    //sprintf(buffer0, "%d.%d.%d.%d", validIp[0], validIp[1], validIp[2], validIp[3]);
    //sprintf(buffer1, "%-16s", buffer0);

    delay(100);

    pingedhosts++;      
  }

  // show total number of found hosts
  sprintf(line0, "Found %d of", hostcount);
  sprintf(line1, "%d hosts", pingedhosts);

  lcdPrint(0, 0, line0, true);
  lcdPrint(0, 1, line1);

  delay(LONG_DELAY);
  return hostcount;
}

int portScanner()
{
  boolean IPAddressCheck = false;
  byte NewIP[4];
  String enteredIPAddressString;
  IPAddress ip;
  int addr = 0;
  int portCount = 0;
  do {
    lcdPrint(0,0,"Enter IP to scan",true);
    lcd.setCursor(0, 1);
    enteredIPAddressString = readKeypad("Enter IP to scan");
    IPAddressCheck = IPValidator(enteredIPAddressString);
  } 
  while (IPAddressCheck == false);
  ByteToArray(enteredIPAddressString,NewIP);
  sprintf(line0, "%d.%d.%d.%d", NewIP[0], NewIP[1], NewIP[2], NewIP[3]);
  lcdPrint(0, 0, line0, true);

  for (int a = 0; a <= 1023; a++)
  {
    readButtons();

    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);

      delay(MEDIUM_DELAY);

      break;
    }

    sprintf(line1, "Trying port %d", a+1);
    lcdPrint(0, 1, line1);

    if (client.connect(NewIP, a+1))
    {
      client.stop();

      sprintf(line0, "Port %d", a+1);
      sprintf(line1, "%-16s", line0);
      portcount[portCount] = a+1;
      portCount++;
    }
  }

  sprintf(line0, "Found %d", portCount, true);
  lcdPrint(0, 0, line0, true);
  lcdPrint(0, 1, "open ports");
  delay(LONG_DELAY);
  return portCount;
}

// display main menu
void mainMenu()
{
  Serial.println("mainMenu loop");
  BatteryMonitor();
  char *MainMenu[8] = { 
    "Information", "Set IP Address", "Diagnostics", "Host Discovery", "Port Scanner", "Backlight", "Power Down", "About"      };
  unsigned long hostcount = 0;
  unsigned long portCount = 0;

  printMenu(MainMenu, 8);

  while (1)
  {
    readButtons();

    // up button
    if (buttons[buttonUp] == HIGH) CursorPrevious(MainMenu, 8);

    // down button
    if (buttons[buttonDown] == HIGH) CursorNext(MainMenu, 8);

    // select button
    if (buttons[buttonSelect] == HIGH)
    {
      switch (CurrentMenuItem)
      {
      case 0:
        infoMenu();
        break;

      case 1:
        SetIPAddress();
        break;

      case 2:
        diagMenu();
        break;

      case 3:
        hostcount = hostDiscovery();

        if (hostcount > 0)
        {
          showHostcount(hostcount);
        }

        break;

      case 4:
        portCount = portScanner();

        if (portCount > 0)
        {
          showPortcount(portCount);
        }

        break;

      case 5:
        ChangeBacklight();
        break;

      case 6:
        ShutDown();
        break;
  
      case 7:
        aboutMenu();
        break;
      }

      printMenu(MainMenu, 8);
    }
  }
}

// display main menu

// read the state of the buttons, HIGH == pressed
void readButtons()
{
  // Clear the button states before checking for a new keypress.
  buttons[buttonUp] = LOW;
  buttons[buttonDown] = LOW;
  buttons[buttonSelect] = LOW;
  buttons[buttonBack] = LOW;
  char key = kpd.getKey();

  switch (key) {
  case '2':       // <-- Change all these to match your keypad characters.
    buttons[buttonUp] = HIGH;
    break;
  case '8':
    buttons[buttonDown] = HIGH;
    break;
  case 'E':
    buttons[buttonSelect] = HIGH;
    break;
  case '4':
    buttons[buttonBack] = HIGH;
    break;
  }
}
void BatteryMonitor()
{
  double BatCheck;
  Serial.println("inside BatteryMonitor");
  // we already have a function for checking battery, so use it
  BatCheck = BatteryCheck();
  // If below 4V, put the LED to red. Otherwise green.
  // 4V = .8 of our setup
  Serial.print("BatCheck is: ");
  Serial.println(BatCheck);
  if (BatCheck <= .8) {
    digitalWrite(BatteryPin1, HIGH);
    digitalWrite(BatteryPin2, LOW);
  }
  else {
    digitalWrite(BatteryPin1, LOW);
    digitalWrite(BatteryPin2, HIGH);
  }
}
    
    
  
void setup()
{
  Serial.begin(19200);
  // set backlight pin
  pinMode(BacklightPin, OUTPUT);
  digitalWrite(BacklightPin, HIGH);
  // set power shutdown pin
  pinMode(PowerPin, OUTPUT);
  digitalWrite(PowerPin, LOW);
  // set the button pins to input
  pinMode (14, INPUT);//Column 1
  pinMode (15, INPUT);//Column 2
  pinMode (16, INPUT);//Column 3
  pinMode (17, INPUT);//Row 1
  pinMode (18, INPUT);//Row 2
  pinMode (19, INPUT);//Row 3
  pinMode (20, INPUT);//Row 4
  pinMode(BatteryPin1, OUTPUT);
  pinMode(BatteryPin2, OUTPUT);
// 1 low, 2 high is green.
// 1 high, 2 low is red.

  // initialize 16x2 lcd
  lcd.begin(16, 2);
// originally in loop
  if (ethernetActive == 0)
  {
    // ethernet isnt active so lets get this show on the road
    lcdPrint(0, 0, "Requesting IP", true);
    Serial.println("Now checking for IP");
    if (!Ethernet.begin(mac))
    {
      Serial.println("Ethernet not working");
      // ethernet failed to initialize, we will keep trying
      lcdPrint(0, 1, "No DHCP");
    }
    else
    {
      Serial.println("Ethernet Active");
      // ethernet is active now
      ethernetActive = 1;

      // waiting 1 second to let ethernet completely initialize
      // ive seen this in other code so i put it here to be safe
      delay(1000);

      myLocalIp = Ethernet.localIP();
      mySubnetMask = Ethernet.subnetMask();

      // set connection timeout and retry count
      // 0x07D0 == 2000
      // 0x320  == 800
      // 0x1F4  == 500
      W5100.setRetransmissionTime(0x1F4);
      W5100.setRetransmissionCount(1);

    }

  }
} // setup

void loop()
{ 
/*
  if (ethernetActive == 0)
  {
    // ethernet isnt active so lets get this show on the road
    lcdPrint(0, 0, "Requesting IP", true);
    Serial.println("Now checking for IP");
    if (!Ethernet.begin(mac))
    {
      Serial.println("Ethernet not working");
      // ethernet failed to initialize, we will keep trying
      lcdPrint(0, 1, "No DHCP");
    }
    else
    {
      Serial.println("Ethernet Active");
      // ethernet is active now
      ethernetActive = 1;

      // waiting 1 second to let ethernet completely initialize
      // ive seen this in other code so i put it here to be safe
      delay(1000);

      myLocalIp = Ethernet.localIP();
      mySubnetMask = Ethernet.subnetMask();

      // set connection timeout and retry count
      // 0x07D0 == 2000
      // 0x320  == 800
      // 0x1F4  == 500
      W5100.setRetransmissionTime(0x1F4);
      W5100.setRetransmissionCount(1);

    }
  }
  else
  {
*/
    // if everything seems good to go, display the main menu
    mainMenu(); 
//  }
} // loop










