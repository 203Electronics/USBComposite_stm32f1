#ifndef _USBCOMPOSITE_H_
#define _USBCOMPOSITE_H_

#include <boards.h>
#include "Stream.h"
#include "usb_generic.h"
//#include <libmaple/usb.h>

#include <USBCompositeSerial.h>
#include <USBHID.h>
#include <USBXBox360.h>
#include <USBMassStorage.h>
#include <USBMIDI.h>
//#include <usb_core.h>?

#define USB_MAX_PRODUCT_LENGTH 32
#define USB_MAX_MANUFACTURER_LENGTH 32
#define USB_MAX_SERIAL_NUMBER_LENGTH  20


// You could use this for a serial number, but you'll be revealing the device ID to the host,
// and hence burning it for cryptographic purposes.
const char* getDeviceIDString();

#define USB_COMPOSITE_MAX_PARTS 6

class USBCompositeDevice;

#define DEFAULT_SERIAL_STRING "00000000000000000001"

typedef bool(*USBPartInitializer)(void*);
typedef void(*USBPartStopper)(void*);

class USBCompositeDevice {
private:
	bool enabled = false;
    bool haveSerialNumber = false;
    uint8_t iManufacturer[USB_DESCRIPTOR_STRING_LEN(USB_MAX_MANUFACTURER_LENGTH)];
    uint8_t iProduct[USB_DESCRIPTOR_STRING_LEN(USB_MAX_PRODUCT_LENGTH)];
    uint8_t iSerialNumber[USB_DESCRIPTOR_STRING_LEN(USB_MAX_SERIAL_NUMBER_LENGTH)]; 
    uint16 vendorId;
    uint16 productId;
    USBCompositePart* parts[USB_COMPOSITE_MAX_PARTS];
	USBPartInitializer init[USB_COMPOSITE_MAX_PARTS];
	USBPartStopper stop[USB_COMPOSITE_MAX_PARTS];
	void* plugin[USB_COMPOSITE_MAX_PARTS];
    uint32 numParts;
public:
	USBCompositeDevice(void); 
    void setVendorId(uint16 vendor=0);
    void setProductId(uint16 product=0);
    void setManufacturerString(const char* manufacturer=NULL);
    void setProductString(const char* product=NULL);
    void setSerialString(const char* serialNumber=DEFAULT_SERIAL_STRING);
    bool begin(void);
    void end(void);
    void clear();
    bool isReady() {
        return enabled && usb_is_connected(USBLIB) && usb_is_configured(USBLIB);    
    }
    bool add(USBCompositePart* part, void* plugin, USBPartInitializer init = NULL, USBPartStopper stop = NULL);
};

class USBCompositeDevice_ {
private:
    USBCompositePart_** parts;
    unsigned numParts;
public:
    bool SetParts(USBCompositePart_** _parts, unsigned _numParts);
};

typedef void (USBCompositePart_::*USBCompositePartCallback)();

class USBEndpointInfo_ {
    USBCompositePartCallback callback;
    uint16_t bufferSize;
    uint16_t type; // bulk, interrupt, etc.
    uint8_t tx; // 1 if TX, 0 if RX
    uint8_t address;    
    uint16_t pmaAddress;
}

class USBCompositePart_ {
public:
    virtual void init() = 0;
    virtual void stop() = 0;
    virtual uint8_t getNumInterfaces() = 0;
    virtual uint8_t getNumEndpoints() = 0;
    virtual USBEndpointInfo_* getEndpoints() = 0;
    virtual uint16_t getDescriptorSize() = 0;
    virtual uint8_t setStartInterface(uint8_t s) = 0;
    virtual uint8_t setStartEndpoint(uint8_t s) = 0;
    virtual void getDescriptor(void* descriptor) = 0;
    virtual void usbInit() = 0;
    virtual void usbReset() = 0;
    virtual void usbSetConfiguration() = 0;
    virtual void usbClearFeature() = 0;
    virtual RESULT usbDataSetup(uint8_t request);
    virtual RESULT usbNoDataSetup(uint8_t request);
}

extern USBCompositeDevice USBComposite;
#endif
        