#include "USBComposite.h"

#define DEFAULT_VENDOR_ID  0x1EAF
#define DEFAULT_PRODUCT_ID 0x0024

struct USBCompositeCallbackInfo {
    USBCompositePart_* part;
    USBCompositePartCallback cb;    
};

static USBCompositeCallbackInfo cb_in[7] {{NULL}}; 
static USBCompositeCallbackInfo cb_out[7] {{NULL}};

#define TRAMPOLINE(dir,num) \
    static void trampoline_ ## dir ## num() { \
        USBCompositePart_* part = (USBCompositePart_*)cb_ ## dir[ num ].part; \
        USBCompositePartCallback cb = cb_ ## dir[ num ].cb;\
        if (part != NULL) (*part.*cb)();\
    }
    
TRAMPOLINE(in,0)
TRAMPOLINE(in,1)
TRAMPOLINE(in,2)
TRAMPOLINE(in,3)
TRAMPOLINE(in,4)
TRAMPOLINE(in,5)
TRAMPOLINE(in,6)
TRAMPOLINE(out,0)
TRAMPOLINE(out,1)
TRAMPOLINE(out,2)
TRAMPOLINE(out,3)
TRAMPOLINE(out,4)
TRAMPOLINE(out,5)
TRAMPOLINE(out,6)

static void (*ep_int_in[7])(void) = {
    trampoline_in_0,trampoline_in_1,trampoline_in_2,trampoline_in_3,trampoline_in_4,
    trampoline_in_5,trampoline_in_6
};

static void (*ep_int_in[7])(void) = {
    trampoline_out_0,trampoline_out_1,trampoline_out_2,trampoline_out_3,trampoline_out_4,
    trampoline_out_5,trampoline_out_6
};

bool USBCompositeDevice_::SetParts(USBCompositePart_** _parts, unsigned _numParts) {
    parts = _parts;
    numParts = _numParts;
    unsigned numInterfaces = 0;
    unsigned numEndpoints = 1;
    uint16 usbDescriptorSize = 0;
    uint16 pmaOffset = USB_EP0_RX_BUFFER_ADDRESS + USB_EP0_BUFFER_SIZE;
    
    for (unsigned i = 0 ; i < 7 ; i++) {
        ep_int_in[i].part = NULL;
        ep_int_out[i].part = NULL;
    }
    
    usbDescriptorSize = 0;
    for (unsigned i = 0 ; i < _numParts ; i++ ) {
        parts[i]->setStartInterface(numInterfaces);
        numInterfaces += parts[i]->getNumInterfaces();
        unsigned partNumEndpoints = parts[i]->getNumEndpoints();
        if (numEndpoints + partNumEndpoints > 8) {
            return false;
		}
        unsigned partDescriptorSize = parts[i]->getDescriptorSize();
        if (usbDescriptorSize + partDescriptorSize > MAX_USB_DESCRIPTOR_DATA_SIZE) {
            return false;
		}
        parts[i]->setStartEndpoint(numEndpoints);
        USBEndpointInfo_* ep = parts[i]->getEndpoints();
        for (unsigned j = 0 ; j < partNumEndpoints ; j++) {
            if (ep[j].bufferSize + pmaOffset > PMA_MEMORY_SIZE) { 
                return false;
			}
            ep[j].pmaAddress = pmaOffset;
            pmaOffset += ep[j].bufferSize;
            ep[j].address = numEndpoints;
            if (ep[j].callback == NULL)
                ep[j].callback = NOP_Process;
            USBCompositeCallbackInfo* ci = ep[j].tx ? &ep_int_in[numEndpoints - 1] : &ep_int_out[numEndpoints - 1];
            if (ep[j].callback == NULL) {
                ci->part = NULL;
            }
            else {
                ci->part = parts[i];
                ci->cb = ep[j].callback;
            }
            numEndpoints++;
        }
        parts[i]->getPartDescriptor(usbConfig.descriptorData + usbDescriptorSize);
        usbDescriptorSize += partDescriptorSize;
    }
    
    usbConfig.Config_Header = Base_Header;    
    usbConfig.Config_Header.bNumInterfaces = numInterfaces;
    usbConfig.Config_Header.wTotalLength = usbDescriptorSize + sizeof(Base_Header);
    Config_Descriptor.Descriptor_Size = usbConfig.Config_Header.wTotalLength;
    
    my_Device_Table.Total_Endpoint = numEndpoints;
        
    return true;
}



static char* putSerialNumber(char* out, int nibbles, uint32 id) {
    for (int i=0; i<nibbles; i++, id >>= 4) {
        uint8 nibble = id & 0xF;
        if (nibble <= 9)
            *out++ = nibble + '0';
        else
            *out++ = nibble - 10 + 'a';
    }
    return out;
}

const char* getDeviceIDString() {
    static char string[80/4+1];
    char* p = string;
    
    uint32 id = (uint32) *(uint16*) (0x1FFFF7E8+0x02);
    p = putSerialNumber(p, 4, id);
    
    id = *(uint32*) (0x1FFFF7E8+0x04);
    p = putSerialNumber(p, 8, id);
    
    id = *(uint32*) (0x1FFFF7E8+0x08);
    p = putSerialNumber(p, 8, id);
    
    *p = 0;
    
    return string;
}

USBCompositeDevice::USBCompositeDevice(void) {
        vendorId = 0;
        productId = 0;
        numParts = 0;
        setManufacturerString(NULL);
        setProductString(NULL);
        setSerialString(DEFAULT_SERIAL_STRING);
        iManufacturer[0] = 0;
        iProduct[0] = 0;
        iSerialNumber[0] = 0;
    }

void USBCompositeDevice::setVendorId(uint16 _vendorId) {
    if (_vendorId != 0)
        vendorId = _vendorId;
    else
        vendorId = DEFAULT_VENDOR_ID;
}

void USBCompositeDevice::setProductId(uint16 _productId) {
    if (_productId != 0)
        productId = _productId;
    else
        productId = DEFAULT_PRODUCT_ID;
}

void setString(uint8* out, const usb_descriptor_string* defaultDescriptor, const char* s, uint32 maxLength) {
    if (s == NULL) {
        uint8 n = defaultDescriptor->bLength;
        uint8 m = USB_DESCRIPTOR_STRING_LEN(maxLength);
        if (n > m)
            n = m;
        memcpy(out, defaultDescriptor, n);
        out[0] = n;
    }
    else {
        uint32 n = strlen(s);
        if (n > maxLength)
            n = maxLength;
        out[0] = (uint8)USB_DESCRIPTOR_STRING_LEN(n);
        out[1] = USB_DESCRIPTOR_TYPE_STRING;
        for (uint32 i=0; i<n; i++) {
            out[2 + 2*i] = (uint8)s[i];
            out[2 + 1 + 2*i] = 0;
        }
    }
}

void USBCompositeDevice::setManufacturerString(const char* s) {
    setString(iManufacturer, &usb_generic_default_iManufacturer, s, USB_MAX_MANUFACTURER_LENGTH);
}

void USBCompositeDevice::setProductString(const char* s) {
    setString(iProduct, &usb_generic_default_iProduct, s, USB_MAX_PRODUCT_LENGTH);
}

void USBCompositeDevice::setSerialString(const char* s) {
    if (s == NULL)
        haveSerialNumber = false;
    else {
        haveSerialNumber = true;
        setString(iSerialNumber, NULL, s, USB_MAX_SERIAL_NUMBER_LENGTH);
    }
}

bool USBCompositeDevice::begin() {
   if (enabled)
        return true;
    for (uint32 i = 0 ; i < numParts ; i++) {
		if (init[i] != NULL && !init[i](plugin[i]))
			return false;
	}
	usb_generic_set_info(vendorId, productId, iManufacturer[0] ? iManufacturer : NULL, iProduct[0] ? iProduct : NULL, 
        haveSerialNumber ? iSerialNumber : NULL);
    if (! usb_generic_set_parts(parts, numParts))
        return false;
    usb_generic_enable();
    enabled = true;  
    return true;
}

void USBCompositeDevice::end() {
    if (!enabled)
        return;
    usb_generic_disable();
    for (uint32 i = 0 ; i < numParts ; i++)
		if (stop[i] != NULL)
			stop[i](plugin[i]);
    enabled = false;
}

void USBCompositeDevice::clear() {
    numParts = 0;
}

bool USBCompositeDevice::add(USBCompositePart* part, void* _plugin, USBPartInitializer _init, USBPartStopper _stop) {
    unsigned i;
    
    for (i = 0; i<numParts; i++) 
        if (plugin[numParts] == _plugin && parts[i] == part)
            break;
    if (i >= USB_COMPOSITE_MAX_PARTS)
        return false;
    parts[i] = part;
    init[i] = _init;
    stop[i] = _stop;
	plugin[i] = _plugin;
    if (i >= numParts)
        numParts++;
    return true;
}

USBCompositeDevice USBComposite;
