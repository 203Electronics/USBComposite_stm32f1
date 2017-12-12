/* Copyright (c) 2011, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

/**
 * @brief USB HID Keyboard device 
 */
 

#include "usb_hid_device.h"

#include <string.h>
#include <stdint.h>
#include <libmaple/nvic.h>
#include "usb_composite.h"
#include <libmaple/usb.h>
#include <string.h>

#include <wirish.h>

/*
 * USB HID interface
 */

template<int... args> static constexpr int countIntegers() {
    return sizeof...(args);
}

static uint8_t reportDescriptors[] = {
        USB_HID_MOUSE_REPORT_DESCRIPTOR(USB_HID_MOUSE_REPORT_ID),
        USB_HID_KEYBOARD_REPORT_DESCRIPTOR(USB_HID_KEYBOARD_REPORT_ID),
        USB_HID_JOYSTICK_REPORT_DESCRIPTOR(USB_HID_JOYSTICK_REPORT_ID)
};

static const uint16_t mouseSize = countIntegers<USB_HID_MOUSE_REPORT_DESCRIPTOR(0)>();
static const uint16_t keyboardSize = countIntegers<USB_HID_KEYBOARD_REPORT_DESCRIPTOR(0)>();
static const uint16_t joystickSize = countIntegers<USB_HID_JOYSTICK_REPORT_DESCRIPTOR(0)>();
static const uint16_t mouseOffset = 0;
static const uint16_t keyboardOffset = mouseOffset+mouseSize;
static const uint16_t joystickOffset = keyboardOffset+keyboardSize;

static_assert(mouseSize+keyboardSize+joystickSize == sizeof(reportDescriptors), "sizes match!");

static const HIDReportDescriptor _hidMouse = { reportDescriptors+mouseOffset, mouseSize };
static const HIDReportDescriptor _hidKeyboard = { reportDescriptors+keyboardOffset, keyboardSize };
static const HIDReportDescriptor _hidJoystick = { reportDescriptors+joystickOffset, joystickSize };
static const HIDReportDescriptor _hidKeyboardMouse = { reportDescriptors+mouseOffset, mouseSize+keyboardSize };
static const HIDReportDescriptor _hidKeyboardJoystick = { reportDescriptors+keyboardOffset, keyboardSize+joystickSize };
static const HIDReportDescriptor _hidKeyboardMouseJoystick = { reportDescriptors+mouseOffset, mouseSize+keyboardSize+joystickSize };

const HIDReportDescriptor* hidReportMouse = &_hidMouse;
const HIDReportDescriptor* hidReportKeyboard = &_hidKeyboard;
const HIDReportDescriptor* hidReportJoystick = &_hidJoystick;
const HIDReportDescriptor* hidReportKeyboardMouse = &_hidKeyboardMouse;
const HIDReportDescriptor* hidReportKeyboardJoystick = &_hidKeyboardJoystick;
const HIDReportDescriptor* hidReportKeyboardMouseJoystick = &_hidKeyboardMouseJoystick;

#define USB_TIMEOUT 50

HIDDevice::HIDDevice(void) {
}

static void generateUSBDescriptor(uint8* out, int maxLength, const char* in) {
    int length = strlen(in);
    if (length > maxLength)
        length = maxLength;
    int outLength = USB_DESCRIPTOR_STRING_LEN(length);
    
    out[0] = outLength;
    out[1] = USB_DESCRIPTOR_TYPE_STRING;
    
    for (int i=0; i<length; i++) {
        out[2+2*i] = in[i];
        out[3+2*i] = 0;
    }    
}

void HIDDevice::begin(const uint8_t* report_descriptor, uint16_t report_descriptor_length, uint16_t idVendor, uint16_t idProduct,
        const char* manufacturer, const char* product) {
            
    uint8_t* manufacturerDescriptor;
    uint8_t* productDescriptor;
    
	if(!enabled) {        
        if (manufacturer != NULL) {
            generateUSBDescriptor(iManufacturer, USB_HID_MAX_MANUFACTURER_LENGTH, manufacturer);
            manufacturerDescriptor = iManufacturer;
        }
        else {
            manufacturerDescriptor = NULL;
        }

        if (product != NULL) {
            generateUSBDescriptor(iProduct, USB_HID_MAX_PRODUCT_LENGTH, product);
        }
        else {
            productDescriptor = NULL;
        }

		usb_composite_enable(BOARD_USB_DISC_DEV, BOARD_USB_DISC_BIT, 
            report_descriptor, report_descriptor_length,
            idVendor, idProduct, manufacturerDescriptor, productDescriptor);
            
		enabled = true;
	}
}

void HIDDevice::begin(const HIDReportDescriptor* report, uint16_t idVendor, uint16_t idProduct,
        const char* manufacturer, const char* product) {
    begin(report->descriptor, report->length, idVendor, idProduct, manufacturer, product);
}

void HIDDevice::setFeatureBuffers(volatile HIDFeatureBuffer_t* fb, int count) {
    usb_hid_set_feature_buffers(fb, count);
}

void HIDDevice::end(void){
	if(enabled){
	    usb_composite_disable(BOARD_USB_DISC_DEV, BOARD_USB_DISC_BIT);
		enabled = false;
	}
}

void HIDReporter::sendReport() {
//    while (usb_is_transmitting() != 0) {
//    }

    usb_hid_tx(buffer, bufferSize);
    
//    while (usb_is_transmitting() != 0) {
//    }
    /* flush out to avoid having the pc wait for more data */
    usb_hid_tx(NULL, 0);
}
        
HIDReporter::HIDReporter(uint8_t* _buffer, unsigned _size, uint8_t _reportID) {
    buffer = _buffer;
    bufferSize = _size;
    memset(buffer, 0, _size);
    reportID = _reportID;
    if (_size > 0)
        buffer[0] = _reportID;
}

void HIDReporter::setFeature(uint8_t* in) {
    return usb_hid_set_feature(reportID, in);
}

uint8_t HIDReporter::getFeature(uint8_t* out, uint8_t poll) {
    return usb_hid_get_feature(reportID, out, poll);
}

HIDDevice HID;
