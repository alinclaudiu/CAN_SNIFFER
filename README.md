# CAN_SNIFFER
CAN_SNIFFER is used to conveniently analyse your cars CAN Bus

I created this little tool for my own when trying to get some information off of my Audi's ComfortCAN-Bus.
(Audi A3 8p)

It takes CAN-Messages in the following format:

##START##<CAN_ID>%<DLC>%<BIT1>%<BIT2>%<BIT3>%<BIT4>%<BIT5>%<BIT6>%<BIT7>%<BIT8>##END## (you can see this in Engine.cs)
  
I personally use an Arduino Nano in combination with a breakout board for the MCP2515 microcontroller. The arduino reads the data from the microcontroller via a SPI interface and then sends it over the Arduino's SerialPort to the WPF Application which processes the data.

You don't have to use this very combination though.
As long as you manage to send the CAN-Data in the format mentioned above, you should be totally fine.

For everybody who is going to use an Arduino as well: I added my .ino file as well for orientation.
