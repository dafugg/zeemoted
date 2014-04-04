Zeemoted                  (c) 2009 by Till Harbaum <till@harbaum.org>
---------------------------------------------------------------------

The zeemoted is a daemon that translates zeemote proprietary data as
received from a zeemote into standard linux input events. It allows to
use a zeemote either as a regular joystick. Furthermore the zeemote
input can be translated into keyboard events for applications that
don't support joysticks.

Legal
-----

This software comes under GPL since it was completely written from
scratch. No documentation from zeemote or a third party was used and
no reverse engeneering has been required. The device responses to a so
called bluetooth rfcomm connection on channel 1 with short and easy to
understand data packages.

Limitations
-----------

Because no free documentation for the zeemote communication is
available only the obvious aspects of the zeemote communication are
being used in the zeemoted. This includes the necessary rfcomm channel
to connect to and the interpretation of the messages including the
sticks and buttons states as well as the battery state.

Currently unknown are the exact meaning of some initial messages the
zeemote sends and which seem to contain e.g. a description of the
exact number and types of buttons available on the zeemote. Also
unknown is the 128 bit service id the zeemote offers its services on
and which is required to connect to the zeemote in more elegant and
future proof way.  This means that vital parts of the zeemote may
change in the future that may make the devices incompatible to the
current zeemoted. However, no such incopatibilities are known.

Compilation/Installation
------------------------

To use a zeemote under linux just build and install the zeemoted by
entering "make" and "make install" on the console. You need to be root
in order to call "make install".

You also need to make sure the the uinput kernel modules are available
on your system. Most modern linux distributions have them installed by
default.

Running
-------

Make sure the the uinput kernel modules is loaded. This can be
manually done by entering "modprobe uinput" on the command line.  You
need to be root for this.

Then either run "zeemoted" without any parameters. The zeemoted will
then search for zeemotes and use the first one found. Or you can
specify a bluetooth device address as a command line parameter to
zeemoted.

Once zeemoted has connected successfully to the zeemote it will give
you a new joystick input device. If there's no joystick present in the
system this will probably be /dev/js0 or /dev/input/js0.  Most
standard linux games will this way work with the zeemote as with any
other ordinary joystick.

Keyboard operation
------------------

In its default configuration, zeemoted emulates a joystick. When given
the -k option, zeemoted instead emulates a keyboard with the two
joystick axes mapped to the cursor keys and the four fire buttons to
the keys A to D.

The sensivity for the analog axes can be adjusted using the -s option.
A low value (min 1) means that the stick works with maximum sensivity
which in trurn means that the stick doesn't have to be moved very far
to generate the assigned key event. A high value (max 127) means that
the analog stick has to be moved to full extent in order to generate
the assigned key event, The default setting is 64 which means that key
events are generated once the analog stick is moved half way from its
center position.

Also configurable are the key events generated. The -c option is being
used with a parameter of the form X:NUM to change the reported key for
the function with the index X to the keycode NUM. The function indices
are:

function | meaning
--------------------
0        | stick left
1        | stick right
2        | stick up
3        | stick down
4        | button a
5        | button b
6        | button c
7        | button d
 
The NUM part of the parameter specifies the keycode to send for this
function. See the keycode table below for a list of possible values.

Example: -c 4:28 makes the A button to act as the keyboards main enter
key.


Keycode table
-------------
(find more codes in /usr/include/linux/input.h)

Main keys
---------
ESC                 1
1                   2
2                   3
3                   4
4                   5
5                   6
6                   7
7                   8
8                   9
9                   10
0                   11
MINUS               12
EQUAL               13
BACKSPACE           14
TAB                 15
Q                   16
W                   17
E                   18
R                   19
T                   20
Y                   21
U                   22
I                   23
O                   24
P                   25
LEFTBRACE           26
RIGHTBRACE          27
ENTER               28
LEFTCTRL            29
A                   30
S                   31
D                   32
F                   33
G                   34
H                   35
J                   36
K                   37
L                   38
SEMICOLON           39
APOSTROPHE          40
GRAVE               41
LEFTSHIFT           42
BACKSLASH           43
Z                   44
X                   45
C                   46
V                   47
B                   48
N                   49
M                   50
COMMA               51
DOT                 52
SLASH               53
RIGHTSHIFT          54
LEFTALT             56
SPACE               57
CAPSLOCK            58
102ND               86
RIGHTCTRL           97
RIGHTALT            100

Function keys
-------------
F1                  59
F2                  60
F3                  61
F4                  62
F5                  63
F6                  64
F7                  65
F8                  66
F9                  67
F10                 68
F11                 87
F12                 88
SYSRQ               99
SCROLLLOCK          70
PAUSE               119

Cursor block keys
-----------------
LEFT                105
RIGHT               106
UP                  103
DOWN                108
PAGEUP              104
PAGEDOWN            109
HOME                102
END                 107
INSERT              110
DELETE              111


Keypad keys
-----------
NUMLOCK             69
KP0                 82
KP1                 79
KP2                 80
KP3                 81
KP4                 75
KP5                 76
KP6                 77
KP7                 71
KP8                 72
KP9                 73
KPPLUS              78
KPMINUS             74
KPDOT               83
KPASTERISK          55
KPENTER             96
KPSLASH             98
