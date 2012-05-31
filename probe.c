#include "hidbp_mod.h"


/* Initialize the global keyboard status info structure for associated
 * keyboard */
static int __initialize_hidbp_keyboard_info(
    struct hidbp_keyboard *kbd, 
    struct usb_device *dev, struct input_dev *input_dev)
{
    if (create_hidbp_keyboard(dev, kbd) < 0) return -1;

    kbd->usbdev   = dev;
    kbd->inputdev = input_dev;

    /* kbd->name should be ("%s %s").(manufacturer, product) */
    strlcpy(kbd->name, dev->manufacturer, sizeof(kbd->name));
    strlcat(kbd->name, " ", sizeof(kbd->name));
    strlcat(kbd->name, dev->product, sizeof(kbd->name));

    usb_make_path(dev, kbd->phys, sizeof(kbd->phys)); /* create a path for kbd */
    strlcat(kbd->phys, "/input0", sizeof(kbd->phys)); /* and save the created
                                                       * path in kbd->phys */
    return 0;
}

/* Configure input subsystem attached to this device to accept certain kinds of
 * inputs/events */
static void __input_device_set_key(struct input_dev *input_dev)
{
    int i = 0;   /* loop counter for keycodes */

    /* Define what kinds of events does the input subsystem expect: 
       EV_KEY: keys and buttons
       EV_LED: LEDs (detailed definition is specified by ledbit[0])
       EV_REP: Autorepeat values */
    input_dev->evbit[0]  = BIT_MASK(EV_KEY) | BIT_MASK(EV_LED) | BIT_MASK(EV_REP);
    input_dev->ledbit[0] = 
        BIT_MASK(LED_NUML) | BIT_MASK(LED_CAPSL) | BIT_MASK(LED_SCROLLL) | 
        BIT_MASK(LED_COMPOSE) | BIT_MASK(LED_KANA);

    /* tell the input subsystem which keys does the device support */
    for ( ; i < 255; i++)
        set_bit(hidbp_keyboard_keycode[i], input_dev->keybit);
    clear_bit(0, input_dev->keybit);     /* Zeros in hidbp_keyboard_keycode[] means
                                          * "not supported", so comes this
                                          * small fix */
}

/* Initialize the input subsystem driver and attach it to the usb device */
static void __initialize_input_device(
    struct hidbp_keyboard *kbd,
    struct usb_interface *iface, struct usb_device *dev, 
    struct input_dev *input_dev)
{
    /* info of usb keyboard is shared with the input subsystem */
    input_dev->name       =  kbd->name;
    input_dev->phys       =  kbd->phys;
    input_dev->dev.parent =  &iface->dev;

    /* copy USB device vendor, product, version information to input device
     * and integrate kbd into input subsystem driver. */
    usb_to_input_id(dev, &input_dev->id);
    input_set_drvdata(input_dev, kbd);

    __input_device_set_key(input_dev);  /* set acceptable keys for input_dev */

    input_dev->event = hidbp_keyboard_event;
    input_dev->open  = hidbp_keyboard_open;
    input_dev->close = hidbp_keyboard_close;
}

/* Initialize URBs for keystrokes irq and LEDs control */
static void __initialize_urb(
    struct hidbp_keyboard *kbd, struct usb_device *dev, 
    struct usb_host_interface *interface, struct usb_endpoint_descriptor *endpoint)
{
    int pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
    int maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

    /* initialize interrupt URB for keystrokes */
    usb_fill_int_urb(
        kbd->irq, dev, pipe, kbd->new, (maxp > 8 ? 8 : maxp),
        hidbp_keyboard_irq, kbd, endpoint->bInterval);
    kbd->irq->transfer_dma = kbd->new_dma;
    kbd->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    
    /* initialize control URB for LEDs control */
    kbd->cr->bRequestType =  USB_TYPE_CLASS | USB_RECIP_INTERFACE;
    kbd->cr->bRequest     =  0x09;
    kbd->cr->wValue       =  cpu_to_le16(0x200);
    kbd->cr->wIndex       =  cpu_to_le16(interface->desc.bInterfaceNumber);
    kbd->cr->wLength      =  cpu_to_le16(1);  /* how many bytes to transmit each time */

    usb_fill_control_urb(
        kbd->led, dev, usb_sndctrlpipe(dev, 0), (void*)kbd->cr, kbd->leds, 1,
        hidbp_keyboard_led, kbd);
    kbd->led->transfer_dma = kbd->leds_dma;
    kbd->led->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
}

/* The fix goes here: set protocol to basic simple boot protocol */
static void __fix__set_protocol(struct usb_device *dev, struct usb_host_interface *interface)
{
    /* set protocol */
    usb_control_msg(
        dev, usb_sndctrlpipe(dev, 0),
        0x0B,     /* SET_PROTOCOL */
        0x21,     /* bRequestType */
        0,        /* set to boot protocol */
        interface->desc.bInterfaceNumber, /* wIndex */
        NULL, 0, /* no data to send */
        0);

    /* set idle */
    usb_control_msg(dev, usb_sndctrlpipe(dev,0),
        0x0A,     /* SET_IDLE */
        0x21,     /* bRequestType */
        0,        /* inhibit forever */
        interface->desc.bInterfaceNumber, /* wIndex */
        NULL, 0,  /* no data to send */
        0);
}


/* The main probe routine */
int hidbp_keyboard_probe(
    struct usb_interface *iface, const struct usb_device_id *id)
{
    struct usb_device              *dev = interface_to_usbdev(iface);
    struct usb_host_interface      *interface = iface->cur_altsetting;
    struct usb_endpoint_descriptor *endpoint = &interface->endpoint[0].desc;
    struct hidbp_keyboard          *kbd;
    struct input_dev               *input_dev;

    if (interface->desc.bNumEndpoints != 1 ||   /* more than 1 EPs */
        !usb_endpoint_is_int_in(endpoint))      /* EP is not interrupt IN */
        return -ENODEV;

    kbd = kzalloc(sizeof(struct hidbp_keyboard), GFP_KERNEL);
    input_dev = input_allocate_device();

    if (kbd && input_dev &&
        (__initialize_hidbp_keyboard_info(kbd, dev, input_dev) == 0))
    {
        __initialize_input_device(kbd, iface, dev, input_dev);
        __initialize_urb(kbd, dev, interface, endpoint);

        /* register input device */
        input_register_device(kbd->inputdev);
        usb_set_intfdata(iface, kbd);
        device_set_wakeup_enable(&dev->dev, 1);

        /* HOTFIX GOES HERE */
        __fix__set_protocol(dev, interface);

        return 0;
    }
    else {
        input_free_device(input_dev);
        kfree(kbd);
        return -ENOMEM;
    }
    
    return 0;
}
