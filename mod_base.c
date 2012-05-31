#include "hidbp_mod.h"


/* allocate memory for hidbp_keyboard */
int create_hidbp_keyboard(struct usb_device *dev, struct hidbp_keyboard *kbd)
{
    if (!(kbd->irq = usb_alloc_urb(0, GFP_KERNEL)))    return -1;
    if (!(kbd->led = usb_alloc_urb(0, GFP_KERNEL)))    return -1;
    if (!(kbd->new = usb_alloc_coherent(dev, 8, GFP_ATOMIC, &kbd->new_dma)))   return -1;
    if (!(kbd->cr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL)))      return -1;
    if (!(kbd->leds = usb_alloc_coherent(dev, 1, GFP_ATOMIC, &kbd->leds_dma))) return -1;

    return 0;
}

/* free allocated memory of kbd */
void destroy_hidbp_keyboard(struct usb_device *dev, struct hidbp_keyboard *kbd)
{
    usb_free_urb(kbd->irq);
    usb_free_urb(kbd->led);
    usb_free_coherent(dev, 8, kbd->new, kbd->new_dma);
    kfree(kbd->cr);
    usb_free_coherent(dev, 1, kbd->leds, kbd->leds_dma);
}


/* callback for input_dev->open */
int hidbp_keyboard_open(struct input_dev *dev)
{
    struct hidbp_keyboard *kbd = input_get_drvdata(dev);

    kbd->irq->dev = kbd->usbdev;
    if (usb_submit_urb(kbd->irq, GFP_KERNEL))
        return -EIO;

    return 0;
}

/* callback for input_dev->close */
void hidbp_keyboard_close(struct input_dev *dev)
{
    struct hidbp_keyboard *kbd = input_get_drvdata(dev);
    usb_kill_urb(kbd->irq);
}

void hidbp_keyboard_disconnect(struct usb_interface *intf)
{
    struct hidbp_keyboard *kbd = usb_get_intfdata (intf);

    usb_set_intfdata(intf, NULL);
    if (kbd) {
        usb_kill_urb(kbd->irq);
        input_unregister_device(kbd->inputdev);
        usb_kill_urb(kbd->led);
        destroy_hidbp_keyboard(interface_to_usbdev(intf), kbd);
        kfree(kbd);
    }
}
