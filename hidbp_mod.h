/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __HIDBP_KEYBOARD_MODULE_HEADER__
#define __HIDBP_KEYBOARD_MODULE_HEADER__


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>


/* USB keyboard keycode table */
extern const __u8 hidbp_keyboard_keycode[256];


/* This struct maintains the status of associated keyboard */
struct hidbp_keyboard
{
    struct usb_device *usbdev;   /* the "actual" usb device pointer */
    struct input_dev  *inputdev; /* input subsystem associated with this
                                  * keyboard */

    __u8 prev[8];  /* key status of the previous moment, we can see key
                    * releases by comparing it with current key status. */
    __u8 newleds;  /* updated LEDs status */

    /* name and phys path of the keyboard */
    char name[128], phys[64];

    struct urb *irq;   /* URB for keystroke msgs */
    struct urb *led;   /* LED for emitting LEDs control msgs */
    struct usb_ctrlrequest *cr;   /* control request "template" for led URB */

    __u8 old[8];   /* keystroke data recieved in the last session */
    __u8 *new;     /* keystroke buffer for the irq URB */
    __u8 *leds;    /* led status buffer for the led URB */

    dma_addr_t new_dma, leds_dma;  /* DMA address for URBs */
};


int  create_hidbp_keyboard(struct usb_device *dev, struct hidbp_keyboard *kbd);
void destroy_hidbp_keyboard(struct usb_device *dev, struct hidbp_keyboard *kbd);


int  hidbp_keyboard_open(struct input_dev *dev);    /* callback for input->open */
void hidbp_keyboard_close(struct input_dev *dev);   /* callback for input->close */


/* driver probe routine */
int hidbp_keyboard_probe(
    struct usb_interface *iface, const struct usb_device_id *id);

/* keystroke handler */
void hidbp_keyboard_irq(struct urb *urb);

/* LEDs control routines */
int hidbp_keyboard_event(       /* callback for input->event */
    struct input_dev *dev, unsigned int type, unsigned int code, int value);
void hidbp_keyboard_led(struct urb *urb);

/* device disconnect callback routine */
void hidbp_keyboard_disconnect(struct usb_interface *intf);



#endif /* __HIDBP_KEYBOARD_MODULE_HEADER__ */
