#include "hidbp_mod.h"


/* callback routine for input->event to handle (LEDs) events */
int hidbp_keyboard_event(struct input_dev *dev, unsigned int type,
    unsigned int code, int value)
{
    struct hidbp_keyboard *kbd = input_get_drvdata(dev);

    if (type != EV_LED) return -1;   /* LED event is not supported */

    /* get new led status */
    kbd->newleds =
        (!!test_bit(LED_KANA,    dev->led) << 3) |
        (!!test_bit(LED_COMPOSE, dev->led) << 3) |
        (!!test_bit(LED_SCROLLL, dev->led) << 2) |
        (!!test_bit(LED_CAPSL,   dev->led) << 1) |
        (!!test_bit(LED_NUML,    dev->led));

    if (kbd->led->status == -EINPROGRESS ||  /* previous message is on the way */
        *(kbd->leds) == kbd->newleds)        /* current LED status is just fine
                                              * (same with newleds) */
        return 0;    /* do nothing */

    /* or we need to change it to the new state */
    *(kbd->leds) = kbd->newleds;
    kbd->led->dev = kbd->usbdev;

    /* submit the changes we made */
    if (usb_submit_urb(kbd->led, GFP_ATOMIC))
        pr_err("usb_submit_urb(leds) failed\n");
    
    return 0;
}

/* LEDs control urb callback routine, just for bulletproof */
void hidbp_keyboard_led(struct urb *urb)
{
    struct hidbp_keyboard *kbd = urb->context;

    if (urb->status)
        hid_warn(urb->dev, "led urb status %d received\n", urb->status);

    /* LED control message is already on the way */
    if (*(kbd->leds) == kbd->newleds) return;

    *(kbd->leds) = kbd->newleds;    
    kbd->led->dev = kbd->usbdev;
    if (usb_submit_urb(kbd->led, GFP_ATOMIC)){
        hid_err(urb->dev, "usb_submit_urb(leds) failed\n");
    }
}
