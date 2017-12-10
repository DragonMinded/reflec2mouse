# reflec2mouse

A utility that reads the serial data from a Reflec Beat cabinet touch panel and converts it to mouse input.

Includes a Visual Studio 2008 solution that compiles to a static executable which runs on XPE, suitable for use on a Reflec Beat cabinet.

## Protocol Documentation

The serial protocol is extremely simple. The game does not request anything from the panel. Every time there is a change to the touchscreen, the touch controller will send a new packet containing a bitfield of all of the X and Y sensors that are blocked by the touch. It is up to the cabinet to parse this into meaningful click actions. As long as you are moving your fingers, the packets will continue to stream to the PC from the touch controller. In order to allow the game to detect when nothing is being touched anymore, the touch controller will send one packet with all 0 bits for the X and Y mask after the last touch is lifted. If there is no change, there will be no packets sent.

From experimentation the screen has an effective resolution of 48 horizontal and 76 vertical sensors. The touch area extends past the LCD orizontally, so a fancy implementation of a mouse driver would take this into account and calibrate the range accordingly.

### Byte Layout

A complete packet is always 21 bytes long and has the following format:

`SSSSSSYYYYYYYYYYYYYYYYYYPPPPXXXXXXXXXXXXCC`
`55544C000000300000000000003000000003000002`

 - `SSSSSS` - Start of Message. Always contains the characters `UTL` or `55544C` in hex.
 - `YYYYYYYYYYYYYYYYYY` - Bits representing the vertical section. The game enumerates vertical sensors from bottom to top, so the 0th bit is for the very bottom and the 75th bit is for the very top.
 - `PPPP` - Padding. Always observed to be `0030` in practice. Possibly unused bits that are hardcoded, but it doesn't matter.
 - `XXXXXXXXXXXX` - Bits representing the horizontal section. The game enumerates left to right, so the 0th bit is for the very left and the 47th bit is for the very right.
 - `CC` - Checksum byte. I'm not sure what algorithm was being used, but in practice it can be recreated with a simple sum and addition. It checks all bytes except for the Start of Message bytes and itself. Start with an accumulator with the value 0xFF, add each of the bytes from position 3 through 20, and then add 0xA0 before anding with 0xFF to get the final checksum. If the value matches this byte value, the packet is valid.

Bit order matters because its a mask of where things are being touched! The 0th bit in the 0th byte corresponds to the very left/very bottom sensors for horizontal/vertical bitmasks. For example, if you wanted to look up the tenth sensor, counting from zero, in the X or Y plane, you would first look up the 1st byte (10 / 8 = 1 remainder 2), and then shift right by 2 places (10 % 8 = 2) and and with 0x1.