/*
 * Copyright (C) 2009-2010 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of zeemoted.
 *
 * zeemoted is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * zeemoted is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with zeemoted.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

#include <linux/uinput.h>

#define ZEEMOTE_AXIS_UNKNOWN 0
#define ZEEMOTE_AXIS_X       1
#define ZEEMOTE_AXIS_Y       2

#define ZEEMOTE_BUTTONS 7
#define ZEEMOTE_STICK   8
#define ZEEMOTE_BATTERY 17

#define ZEEMOTE_BUTTON_NONE  (0xfe)

#define ZEEMOTE_BUTTON_A 0
#define ZEEMOTE_BUTTON_B 1
#define ZEEMOTE_BUTTON_C 2
#define ZEEMOTE_BUTTON_D 3

#define ZEEMOTE_CLASS  0x000584   // Peripheral, Pointing device/Joystick

/* Byte order conversions */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htozs(d)  bswap_16(d)
#define ztohs(d)  bswap_16(d)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define htozs(d)  (d)
#define ztohs(d)  (d)
#else
#error "Unknown byte order"
#endif

/* Create uinput output */
int do_uinput(int fd, unsigned short key, int pressed, 
	      unsigned short event_type) {
  struct input_event event;
  memset(&event, 0 , sizeof(event));

  gettimeofday(&event.time, NULL);
  event.type = event_type;
  event.code = key;
  event.value = pressed;
  
  if(write(fd,&event,sizeof(event)) != sizeof(event))
    perror("Writing event");

  return 1;
}

static int keys[] = {
  KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, 
  KEY_A, KEY_B, KEY_C, KEY_D, 
  0 };

/* Init uinput */
void init_uinput_keyboard_device(int fd) {
  int i = 0;

  if(ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror("setting EV_KEY");

  while(keys[i]) {
    if(ioctl(fd, UI_SET_KEYBIT, keys[i]) < 0) 
      fprintf(stderr, "setting key %d: %s\n", keys[i], strerror(errno));
    i++;
  }
}

void init_uinput_joystick_device(int fd) {
  
  if(ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) perror("setting EV_ABS");
  if(ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0) perror("setting ABS_X");
  if(ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0) perror("setting ABS_Y");
  
  if(ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) perror("setting EV_KEY");
  if(ioctl(fd, UI_SET_KEYBIT, BTN_0) < 0) perror("setting BTN_0");
  if(ioctl(fd, UI_SET_KEYBIT, BTN_1) < 0) perror("setting BTN_1");
  if(ioctl(fd, UI_SET_KEYBIT, BTN_2) < 0) perror("setting BTN_2");
  if(ioctl(fd, UI_SET_KEYBIT, BTN_3) < 0) perror("setting BTN_3");
}

int init_uinput_device(int kbd_mode) {
  struct uinput_user_dev dev;
  int fd = -1;

  fd = open("/dev/input/uinput", O_RDWR);
  
  if(fd < 0) {
    fd = open("/dev/uinput", O_RDWR);

    if(fd < 0) {
      printf("Unable to open uinput device.\n"
	     "Perhaps 'modprobe uinput' required?\n");
      return -1;
    }
  }
  
  memset(&dev,0,sizeof(dev));
  strncpy(dev.name, "Zeemote", UINPUT_MAX_NAME_SIZE);
  dev.id.bustype = BUS_BLUETOOTH;
  dev.id.version = 0x01;
  
  if(write(fd, &dev, sizeof(dev)) < sizeof(dev)) {
    printf("Registering device at uinput failed\n");
    return -1;
  }

  if(kbd_mode) init_uinput_keyboard_device(fd);
  else         init_uinput_joystick_device(fd);

  if(ioctl(fd, UI_DEV_CREATE) < 0)        perror("Create device");

  return fd;
}

int bluez_connect(bdaddr_t *bdaddr, int channel) {
  struct sockaddr_rc rem_addr;
  int bt;

  // bluez connects to BlueClient
  if( (bt = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0 ) {
    fprintf(stderr, "Can't create socket. %s(%d)\n", strerror(errno), errno);
    return -1;
  }

  /* connect on rfcomm */
  memset(&rem_addr, 0, sizeof(rem_addr));
  rem_addr.rc_family = AF_BLUETOOTH;
  rem_addr.rc_bdaddr = *bdaddr;
  rem_addr.rc_channel = channel;
  if( connect(bt, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0 ){
    fprintf(stderr, "Can't connect. %s(%d)\n", strerror(errno), errno);
    close(bt);
    return -1;
  }

  return bt;
}

#define MAGIC  0xa1

typedef struct {
  unsigned char length;
  unsigned char magic;
  unsigned char type;
} zeemote_hdr_t;

ssize_t read_num(int fd, void *data, size_t count) {
  ssize_t total = 0;

  while(count) {
    ssize_t rd = read(fd, data, count);
    if(rd < 0) {
      perror("read");
      return rd;
    } else {
      count -= rd;
      data += rd;
      total += rd;
    }
  }
  return total;
}

bdaddr_t *inquiry() {
  inquiry_info *info = NULL;
  uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
  int num_rsp, i, mote;
  bdaddr_t *result = NULL;

  num_rsp = hci_inquiry(-1, 8, 0, lap, &info, 0);
  if (num_rsp < 0) {
    perror("Inquiry failed.");
    exit(1);
  }

  for (mote=0, i = 0; i < num_rsp; i++) 
    if((info+i)->dev_class[0] == ((ZEEMOTE_CLASS >> 0) & 0xff) &&
       (info+i)->dev_class[1] == ((ZEEMOTE_CLASS >> 8) & 0xff) &&
       (info+i)->dev_class[2] == ((ZEEMOTE_CLASS >> 16) & 0xff)) 
      mote++;

  if(!mote) {
    printf("No zeemotes found\n");
    return result;
  }

  result = malloc(sizeof(bdaddr_t) * (mote + 1));

  for (mote=0, i = 0; i < num_rsp; i++) {
    if((info+i)->dev_class[0] == ((ZEEMOTE_CLASS >> 0) & 0xff) &&
       (info+i)->dev_class[1] == ((ZEEMOTE_CLASS >> 8) & 0xff) &&
       (info+i)->dev_class[2] == ((ZEEMOTE_CLASS >> 16) & 0xff)) {
      result[mote++] = (info+i)->bdaddr; 
    }
  }
  
  bacpy(&result[mote++], BDADDR_ANY);
  return result;
}

static void usage(void) {
  printf("Usage: zeemoted [OPTIONS] [DEVICE ADDRESS]\n");
  printf("Options:\n");
  printf("-k       emulate keyboard instead of joystick\n");
  printf("-s NUM   set axis sensivity in keyboard mode. Values range from\n");
  printf("         1 (very sensitive) to 127 (very insensitive), default is 64\n");
  printf("-c X:NUM set keycode to be returned in keyboard mode. X ist the axis/button to\n");
  printf("         be set: 0=left, 1=right, 2=up, 3=down, 4-7=buttons a-d. Valid keycodes\n");
  printf("         can e.g. be found in the file /usr/include/linux/input.h\n");
  printf("         Example: -c 4:28 makes the A button to act as the enter key.\n");
  printf("         Default is to map the axes to the cursor keys and the buttons the the\n");
  printf("         keys a to d.\n");
}

int main(int argc, char **argv) {
  int bt;
  bdaddr_t *bdaddr = NULL;
  int kbd_mode = 0;
  int c, sensivity = 64;

  printf("zeemoted V"VERSION" - "
	 "(c) 2009-2010 by Till Harbaum <till@harbaum.org>\n");

  /* parse options */
  opterr = 0;   
  while ((c = getopt(argc, argv, "ks:c:")) != -1) {
    switch(c) {

    case 'k':
      kbd_mode = 1;
      break;

    case 's':
      sensivity = atoi(optarg);
      if(sensivity < 1 || sensivity > 126) {
	printf("Sensivity out of bounds! Must be between 1 and 126\n");
	sensivity = 64;
      }
      break;

    case 'c': {
      int index, code;
      if((sscanf(optarg, "%d:%d\n", &index, &code) == 2) &&
	 index >= 0 && index <= 7) 
	keys[index] = code;
      else
	usage();
      } break;
      
    case '?':
      if(optopt == 's') 
	fprintf (stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint (optopt))
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
	fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);

      usage();
      return 1;

    default:
      abort();
    }
  }
  
  if(optind == argc) {
    printf("No device addresses given, trying to autodetect devices ...\n");
    bdaddr = inquiry();
    if(!bdaddr) {
      printf("No devices found\n");
      exit(0);
    }
  } else {
    int i = 0;
    bdaddr = malloc(sizeof(bdaddr_t) * (argc - optind + 1));
    while(optind < argc) 
      baswap(&bdaddr[i++], strtoba(argv[optind++]));

    bacpy(bdaddr, BDADDR_ANY);
  }

  // fork handler for each zeemote
  while(bacmp(bdaddr, BDADDR_ANY)) {
    char addr[18];
    ba2str(bdaddr, addr);

    // open bluetooth rfcomm
    if((bt = bluez_connect(bdaddr, 1)) >= 0) {

      int fd = init_uinput_device(kbd_mode);

      printf("Zeemote device %s connected for use as %s.\n",
	     addr, kbd_mode?"keyboard":"joystick");  

      pid_t pid = fork();
      if(pid < 0) {
        perror("fork() failed");
        exit(errno);
      }

      if(!pid) {
	while(1) {
	  zeemote_hdr_t hdr;
	  
	  union {
	    char axis[3];
	    unsigned char buttons[6];
	    unsigned short voltage;
	    char dummy[48];
	  } data;
	  
	  int rd = read_num(bt, &hdr, sizeof(hdr));
	  if(rd == sizeof(hdr)) {
	    if(hdr.magic == MAGIC && hdr.length >= 2) {
	      switch(hdr.type) {
		
	      case ZEEMOTE_STICK:
		if(hdr.length-2 == sizeof(data.axis)) {
		  if(read_num(bt, data.axis, sizeof(data.axis))) {
		    if(data.axis[ZEEMOTE_AXIS_UNKNOWN])
		      printf("WARNING: ZEEMOTE_STICK axis UNKNOWN != 0!\n");
		    
		    if(kbd_mode) {
		      static int old = 0;
		      
		      int state = 
			(data.axis[ZEEMOTE_AXIS_X] < -sensivity)?-1:
			((data.axis[ZEEMOTE_AXIS_X] > sensivity)?1:0);
		      
		      if(state != old) {
			if(old)
			  do_uinput(fd, (old<0)?keys[0]:keys[1], 0, EV_KEY);
			
			if(state)
			  do_uinput(fd, (state<0)?keys[0]:keys[1], 1, EV_KEY);
			
			old = state;
		      }
		    } else {
		      static char old = 0;
		      if(data.axis[ZEEMOTE_AXIS_X] != old) {
			do_uinput(fd, ABS_X, data.axis[ZEEMOTE_AXIS_X]*256u, EV_ABS);
			old = data.axis[ZEEMOTE_AXIS_X];
		      }
		    }
		    
		    if(kbd_mode) {
		      static int old = 0;
		      
		      int state = 
			(data.axis[ZEEMOTE_AXIS_Y] < -sensivity)?-1:
			((data.axis[ZEEMOTE_AXIS_Y] > sensivity)?1:0);
		      
		      if(state != old) {
			if(old)
			  do_uinput(fd, (old<0)?keys[2]:keys[3], 0, EV_KEY);
			
			if(state)
			  do_uinput(fd, (state<0)?keys[2]:keys[3], 1, EV_KEY);
			
			old = state;
		      }
		    } else {
		      static char old = 0;
		      if(data.axis[ZEEMOTE_AXIS_Y] != old) {
			do_uinput(fd, ABS_Y, 
				  data.axis[ZEEMOTE_AXIS_Y]*256u, EV_ABS);
			old = data.axis[ZEEMOTE_AXIS_Y];
		      }
		    }
		    
		  } else 
		    printf("ERROR: reading ZEEMOTE_STICK payload failed\n");
		} else {
		  printf("ERROR: unexpected length %d in ZEEMOTE_STICK\n", 
			 hdr.length);
		  read_num(bt, data.dummy, hdr.length - 2);
		}
		break;
		
	      case ZEEMOTE_BATTERY:
		if(hdr.length-2 == sizeof(data.voltage)) {
		  if(read_num(bt, &data.voltage, sizeof(data.voltage))) {
		    printf("ZEEMOTE_BATTERY: %d.%03u volts\n", 
			   ztohs(data.voltage)/1000, ztohs(data.voltage)%1000);
		    
		  } else 
		    printf("ERROR: reading ZEEMOTE_BATTERY payload failed\n");
		} else {
		  printf("ERROR: unexpected length %d in ZEEMOTE_BATTERY\n", 
			 hdr.length);
		  read_num(bt, data.dummy, hdr.length - 2);
		}
		break;
		
	      case ZEEMOTE_BUTTONS:
		if(hdr.length-2 == sizeof(data.buttons)) {
		  if(read_num(bt, data.buttons, sizeof(data.buttons))) {
		    int i, buttons = 0;
		    
		    for(i=0;i<sizeof(data.buttons);i++)
		      if(data.buttons[i] != ZEEMOTE_BUTTON_NONE)
			buttons |= 1<<data.buttons[i];
		    
		    static int old = 0;
		    for(i=0;i<4;i++) {
		      if((buttons & (1<<i)) != (old & (1<<i))) {
			do_uinput(fd, kbd_mode?keys[4+i]:(BTN_0 + i), 
				  (buttons & (1<<i))?1:0, EV_KEY);
		}
		    }
		    old = buttons;
		    
		    
		  } else 
		    printf("ERROR: reading ZEEMOTE_BUTTONS payload failed\n");
		} else {
		  printf("ERROR: unexpected length %d in ZEEMOTE_BUTTONS\n", 
			 hdr.length);
		  read_num(bt, data.dummy, hdr.length - 2);
		}
		break;
		
	      default:
		if(hdr.length - 2 > sizeof(data.dummy))
		  printf("%d bytes of unknown command %d exceeding limits\n", 
			 hdr.length-2, hdr.type);
		
		read_num(bt, data.dummy, hdr.length - 2);
		break;
	      }
	    }
	  }
	}
      }
    } else
      printf("connection to %s failed\n", addr);

    bdaddr++;
  }

  int status;
  wait(&status);
  printf("zeemoted terminated\n");

  return 0;
}
