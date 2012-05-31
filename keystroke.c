#include "hidbp_mod.h"


/* USB keyboard keycode table
   <taken from the code of usbkbd.c by Vojtech Pavlik>
*/
const __u8 hidbp_keyboard_keycode[256] = 
{
      0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
     50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
      4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
     27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
     65, 66, 67, 68, 87, 88, 99, 70,119,110,102,104,111,107,109,106,
    105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
     72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
    191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
    115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
    122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
    150,158,159,128,136,177,178,176,142,152,173,140
};

/* Interrupt handler routine to report keystrokes to input subsystem.  Scancode
   submitted to this irq is a 8 bytes packet, the first byte is key modifier
   flags, the second byte is reserved for special usage, the rest 6 bytes are
   keystoke buffer.

   packet format: [KEYMOD] [RESERVED] [KEY1][KEY2]...[KEY6]
*/
void hidbp_keyboard_irq(struct urb *urb)
{
    struct hidbp_keyboard *kbd = urb->context;
    int i = 0;    /* loop counter */

    switch (urb->status) {
    case 0:            break;
    case -ECONNRESET:  /* unlink */
    case -ENOENT:
    case -ESHUTDOWN:   return;

        /* -EPIPE:  should clear the halt */
    default:  goto resubmit;
    }

    /* report the status of 8 key modifiers: 
       KEY_LEFTCTRL, KEY_LEFTSHIFT, KEY_LEFTALT, KEY_LEFTMETA, 
       KEY_RIGHTCTRL, KEY_RIGHTSHIFT, KEY_RIGHTALT, KEY_RIGHTMETA */
    for (i = 0; i < 8; i++)
    {
        input_report_key(
            kbd->inputdev, hidbp_keyboard_keycode[i + 224], (kbd->new[0] >> i) & 1);
    }

    /* Detect key releases and presses from these 6 bytes keystroke buffer */
    for (i = 2; i < 8; i++)
    {
        /* detect keyreleases */
        if (kbd->old[i] > 3 && 
            memscan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8)
        {
            if (hidbp_keyboard_keycode[kbd->old[i]])
                input_report_key(kbd->inputdev, hidbp_keyboard_keycode[kbd->old[i]], 0);
            else
                hid_info(urb->dev,
                    "Unknown key (scancode %#x) released.\n",
                    kbd->old[i]);
        }

        /* detect keypresses */
        if (kbd->new[i] > 3 && 
            memscan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8)
        {
            if (hidbp_keyboard_keycode[kbd->new[i]])
                input_report_key(kbd->inputdev, hidbp_keyboard_keycode[kbd->new[i]], 1);
            else
                hid_info(urb->dev,
                    "Unknown key (scancode %#x) pressed.\n",
                    kbd->new[i]);
        }
    }

    input_sync(kbd->inputdev);
    memcpy(kbd->old, kbd->new, 8);

resubmit:
    i = usb_submit_urb (urb, GFP_ATOMIC);
    if (i) {
        hid_err(urb->dev, "can't resubmit intr, %s-%s/input0, status %d",
            kbd->usbdev->bus->bus_name,
            kbd->usbdev->devpath, i);
    }
}
