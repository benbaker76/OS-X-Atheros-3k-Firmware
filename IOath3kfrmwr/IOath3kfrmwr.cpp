/* Disclaimer:
 This code is loosely based on the template of the class 
 in AnchorUSB Driver example from IOUSBFamily, 
 Open Source by Apple http://www.opensource.apple.com
 
 For information on driver matching for USB devices, see: 
 http://developer.apple.com/qa/qa2001/qa1076.html

 */
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>

#include <IOKit/usb/IOUSBHostDevice.h>
#include <IOKit/usb/IOUSBHostInterface.h>

#include "IOath3kfrmwr.h"

#ifndef IOATH3KNULL
#include "ath3k-1fw.h"
#endif

//REVIEW: avoids problem with Xcode 5.1.0 where -dead_strip eliminates these required symbols
#include <libkern/OSKextLib.h>
void* _org_rehabman_dontstrip_[] =
{
    (void*)&OSKextGetCurrentIdentifier,
    (void*)&OSKextGetCurrentLoadTag,
    (void*)&OSKextGetCurrentVersionString,
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

//rehabman:
// Note: mac4mat's original had this BULK_SIZE at 4096.  Turns out sending
// the firmware 4k at a time doesn't quite work in SL. And it seems
// sending it 1k at a time works in SL, Lion, and ML.
#define BULK_SIZE			1024
#define MAX_FILE_SIZE		2000000
#define USB_REQ_DFU_DNLOAD	1
#define FIRST_PACKET_SIZE	20

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

OSDefineMetaClassAndStructors(org_rehabman_IOath3kfrmwr, IOService)

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //

#ifdef DEBUG
bool local_IOath3kfrmwr::init(OSDictionary *propTable)
{
#ifdef DEBUG
    IOLog("org_rehabman_IOath3kfrmwr(%p): init (https://github.com/RehabMan/OS-X-Atheros-3k-Firmware.git)\n", this);
#else
    IOLog("IOath3kfrmwr: init (https://github.com/RehabMan/OS-X-Atheros-3k-Firmware.git)\n");
#endif
    return (super::init(propTable));
}

IOService* local_IOath3kfrmwr::probe(IOService *provider, SInt32 *score)
{
    DEBUG_LOG("%s(%p)::probe\n", getName(), this);
    return super::probe(provider, score);			// this returns this
}

bool local_IOath3kfrmwr::attach(IOService *provider)
{
    // be careful when performing initialization in this method. It can be and
    // usually will be called mutliple 
    // times per instantiation
    DEBUG_LOG("%s(%p)::attach\n", getName(), this);
    return super::attach(provider);
}

void local_IOath3kfrmwr::detach(IOService *provider)
{
    // Like attach, this method may be called multiple times
    DEBUG_LOG("%s(%p)::detach\n", getName(), this);
    return super::detach(provider);
}
#endif // DEBUG

//
// start
// when this method is called, I have been selected as the driver for this device.
// I can still return false to allow a different driver to load
//
bool local_IOath3kfrmwr::start(IOService *provider)
{
#ifdef DEBUG
    IOLog("%s(%p)::start - Version 1.2.1 starting\n", getName(), this);
#else
    IOLog("IOath3kfrmwr: Version 1.2.1 starting\n");
#endif
	
    IOReturn result;
	const StandardUSB::DeviceDescriptor *deviceDescriptor;
	const StandardUSB::ConfigurationDescriptor *configurationDescriptor;
    
    // Do all the work here, on an IOKit matching thread.
    
    // 0.1 Get my USB Device
    DEBUG_LOG("%s(%p)::start!\n", getName(), this);
    m_usbHostDevice = OSDynamicCast(IOUSBHostDevice, provider);
    if(!m_usbHostDevice)
    {
        IOLog("%s(%p)::start - Provider isn't a USB device!!!\n", getName(), this);
        return false;
    }

    // 0.2 Reset the device
    result = m_usbHostDevice->reset();
    if (result)
    {
        IOLog("%s(%p)::start - failed to reset the device\n", getName(), this);
        //return false;
    }
    else
        DEBUG_LOG("%s(%p)::start: device reset\n", getName(), this);
    
    // 0.3 Find the first config/interface
	deviceDescriptor = m_usbHostDevice->getDeviceDescriptor();
    int numconf = deviceDescriptor->bNumConfigurations;
    if (numconf < 1)
    {
        IOLog("%s(%p)::start - no composite configurations\n", getName(), this);
        return false;
    }
    DEBUG_LOG("%s(%p)::start: num configurations %d\n", getName(), this, numconf);
        
    // 0.4 Get first config descriptor
    configurationDescriptor = m_usbHostDevice->getConfigurationDescriptor(0);
    if (!configurationDescriptor)
    {
        IOLog("%s(%p)::start - no config descriptor\n", getName(), this);
        return false;
    }
	
    // 1.0 Open the USB device
    if (!m_usbHostDevice->open(this))
    {
        IOLog("%s(%p)::start - unable to open device for configuration\n", getName(), this);
        return false;
    }
    
    // 1.1 Set the configuration to the first config
    result = m_usbHostDevice->setConfiguration(configurationDescriptor->bConfigurationValue, true);
    if (result)
    {
        IOLog("%s(%p)::start - unable to set the configuration\n", getName(), this);
        m_usbHostDevice->close(this);
        return false;
    }
    
    // 1.2 Get the status of the USB device (optional, for diag.)
    uint16_t status;
    result = getStatus(&status);
    if (result)
    {
        IOLog("%s(%p)::start - unable to get device status\n", getName(), this);
        m_usbHostDevice->close(this);
        return false;
    }
    DEBUG_LOG("%s(%p)::start: device status %d\n", getName(), this, (int)status);

// rehabman:
// IOATH3KNULL can be used to create an IOath3kfrmwr.kext that effectively
// disables the device, so a 3rd party device can be used instead.
// To make this really work, there is probably additional device IDs that must be
// entered in the Info.plist
//
// Credit to mac4mat for this solution too...
    
#ifndef IOATH3KNULL
	// 2.0 Find the interface for bulk endpoint transfers
	if (!findFirstInterface())
	{
		IOLog("%s(%p)::probe - unable to find interface\n", getName(), this);
		m_usbHostDevice->close(this);
		return NULL;
	}
	
	// 2.1 Open the interface
	if (!m_usbHostInterface->open(this))
	{
		IOLog("%s(%p)::probe - unable to open interface\n", getName(), this);
		m_usbHostDevice->close(this);
		return NULL;
	}

	// 2.2 Get info on endpoints (optional, for diag.)
	const StandardUSB::InterfaceDescriptor *interfaceDescriptor = m_usbHostInterface->getInterfaceDescriptor();
	DEBUG_LOG("%s(%p)::probe - interface has %d endpoints\n", getName(), this, interfaceDescriptor->bNumEndpoints);
	
	// 2.3 Get the pipe for bulk endpoint 2 Out
	//result = findPipe(&m_intresultuptPipe, kEndpointTypeIntresultupt, kEndpointDirectionIn);
	result = findPipe(&m_bulkPipe, kEndpointTypeBulk, kEndpointDirectionOut);
	
	// 3.0 Send request to Control Endpoint to initiate the firmware transfer
	
#if 0  // Trying to troubleshoot the problem after Restart (with OSBundleRequired Root)
	for (uint32_t i = 0; i < 5; i++) { // retry on error
		result = initFirmwareTransfer((void *)firmware_buf, FIRST_PACKET_SIZE); // (synchronous, will block)
		if (result)
			IOLog("%s(%p)::probe - failed to initiate firmware transfer (%d), retrying (%d)\n", getName(), this, err, i + 1);
		else
			break;
	}
#else
	result = initFirmwareTransfer((void *)firmware_buf, FIRST_PACKET_SIZE); // (synchronous, will block)
#endif
	if (result)
	{
		IOLog("%s(%p)::probe - failed to initiate firmware transfer (%d)\n", getName(), this, result);
		m_usbHostInterface->close(this);
		m_usbHostDevice->close(this);
		return NULL;
	}
	
	// 3.1 Create IOMemoryDescriptor for bulk transfers
	char memoryBuffer[BULK_SIZE];
	IOMemoryDescriptor *dataBuffer = IOMemoryDescriptor::withAddress(&memoryBuffer, BULK_SIZE, kIODirectionNone);
	
	if (!dataBuffer)
	{
		IOLog("%s(%p)::probe - failed to map memory descriptor\n", getName(), this);
		m_usbHostInterface->close(this);
		m_usbHostDevice->close(this);
		return NULL;
	}
	
	result = dataBuffer->prepare();
	
	if (result)
	{
		IOLog("%s(%p)::probe - failed to prepare memory descriptor\n", getName(), this);
		m_usbHostInterface->close(this);
		m_usbHostDevice->close(this);
		return NULL;
	}
	
	// 3.2 Send the rest of firmware to the bulk pipe
	static const unsigned char *firmwareBuffer = firmware_buf;
	uint32_t totalBufferSize = sizeof(firmware_buf);
	
	firmwareBuffer += FIRST_PACKET_SIZE;
	totalBufferSize -= FIRST_PACKET_SIZE;
	
	uint32_t i = 1;
	
	while (totalBufferSize)
	{
		uint32_t dataBufferLength = (totalBufferSize < BULK_SIZE ? totalBufferSize : BULK_SIZE);
		uint32_t bytesTransferred = 0;
		
		memcpy(memoryBuffer, firmwareBuffer, dataBufferLength);
		result = m_bulkPipe->io(dataBuffer, dataBufferLength, bytesTransferred, 10000);
		
		if (result)
		{
			IOLog("%s(%p)::probe - failed to write firmware to bulk pipe (err:%d, block:%d, to_send:%d)\n", getName(), this, result, i, dataBufferLength);
			m_usbHostInterface->close(this);
			m_usbHostDevice->close(this);
			return NULL;
		}
		
		firmwareBuffer += dataBufferLength;
		totalBufferSize -= dataBufferLength;
		
		i++;
	}
	
	IOLog("%s(%p)::probe - firmware was sent to bulk pipe\n", getName(), this);
	
	result = dataBuffer->complete();
	
	if (result)
	{
		IOLog("%s(%p)::probe - failed to complete memory descriptor\n", getName(), this);
		m_usbHostInterface->close(this);
		m_usbHostDevice->close(this);
		return NULL;
	}
	
	// 4.0 Get device status (it fails, but somehow is important for operational device)
	result = getStatus(&status);
	if (result)
	{
		// this is the normal case...
		DEBUG_LOG("%s(%p)::probe - unable to get device status\n", getName(), this);
	}
	else
	{
		// this is more of an error case... after firmware load
		// device status shouldn't work, as the devices has changed IDs
		IOLog("%s(%p)::probe - device status %d\n", getName(), this, (int)status);
	}
	
	// Close the interface
	m_usbHostInterface->close(this);
	
	// Close the USB device
	m_usbHostDevice->close(this);
	return NULL;  // return false to allow a different driver to load
#else   // !IOATH3KNULL
	// Do not load the firmware, leave the controller non-operational
	
	// Do not close the USB device
	//m_usbHostDevice->close(this);
	return NULL;  // return true to retain exclusive access to USB device
#endif  // !IOATH3KNULL
}

#ifdef DEBUG

void local_IOath3kfrmwr::stop(IOService *provider)
{
    DEBUG_LOG("%s(%p)::stop\n", getName(), this);
    super::stop(provider);
}

bool local_IOath3kfrmwr::handleOpen(IOService *forClient, IOOptionBits options, void *arg )
{
    DEBUG_LOG("%s(%p)::handleOpen\n", getName(), this);
    return super::handleOpen(forClient, options, arg);
}

void local_IOath3kfrmwr::handleClose(IOService *forClient, IOOptionBits options )
{
    DEBUG_LOG("%s(%p)::handleClose\n", getName(), this);
    super::handleClose(forClient, options);
}

IOReturn local_IOath3kfrmwr::message(UInt32 type, IOService *provider, void *argument)
{
    DEBUG_LOG("%s(%p)::message\n", getName(), this);
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            if (m_usbHostDevice->isOpen(this))
            {
                IOLog("%s(%p)::message - service is terminated - closing device\n", getName(), this);
//                m_usbHostDevice->close(this);
            }
            break;
            
        case kIOMessageServiceIsSuspended:
        case kIOMessageServiceIsResumed:
        case kIOMessageServiceIsRequestingClose:
        case kIOMessageServiceWasClosed: 
        case kIOMessageServiceBusyStateChange:
        default:
            break;
    }
    
    return super::message(type, provider, argument);
}

bool local_IOath3kfrmwr::terminate(IOOptionBits options)
{
    DEBUG_LOG("%s(%p)::terminate\n", getName(), this);
    return super::terminate(options);
}

bool local_IOath3kfrmwr::finalize(IOOptionBits options)
{
    DEBUG_LOG("%s(%p)::finalize\n", getName(), this);
    return super::finalize(options);
}

IOReturn local_IOath3kfrmwr::initFirmwareTransfer(void *firmwareBuffer, uint32_t size)
{
	StandardUSB::DeviceRequest request = { };
	request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionOut, kRequestTypeClass, kRequestRecipientDevice);
	request.bRequest      = USB_REQ_DFU_DNLOAD;
	request.wValue        = 0;
	request.wIndex        = 0;
	request.wLength       = size;
	uint32_t bytesTransfered = 0;
	return m_usbHostInterface->deviceRequest(request, firmwareBuffer, bytesTransfered, 0);
}

bool local_IOath3kfrmwr::findFirstInterface()
{
	DEBUG_LOG("%s(%p)::findFirstInterface\n", getName(), this);
	
	OSIterator* iterator = m_usbHostDevice->getChildIterator(gIOServicePlane);
	
	if (!iterator)
		return false;
	
	while (OSObject *candidate = iterator->getNextObject())
	{
		if ((m_usbHostInterface = OSDynamicCast(IOUSBHostInterface, candidate)))
		{
			//if (interface->getInterfaceDescriptor()->bInterfaceClass != kUSBHubClass)
			{
				break;
			}
		}
	}
	
	iterator->release();
	
	return (m_usbHostInterface != NULL);
}

IOReturn local_IOath3kfrmwr::getStatus(uint16_t *statusNumber)
{
	uint16_t status       = 0;
	StandardUSB::DeviceRequest request = { };
	request.bmRequestType = makeDeviceRequestbmRequestType(kRequestDirectionIn, kRequestTypeStandard, kRequestRecipientDevice);
	request.bRequest      = kDeviceRequestGetStatus;
	request.wValue        = 0;
	request.wIndex        = 0;
	request.wLength       = sizeof(status);
	uint32_t bytesTransfresulted = 0;
	IOReturn result = m_usbHostDevice->deviceRequest(this, request, &status, bytesTransfresulted, kUSBHostStandardRequestCompletionTimeout);
	*statusNumber = status;
	return result;
}

bool local_IOath3kfrmwr::findPipe(IOUSBHostPipe **pipe, uint8_t type, uint8_t direction)
{
	DEBUG_LOG("%s(%p)::findPipe direction = %d, type = %d\n", getName(), this, direction, type);
	const StandardUSB::ConfigurationDescriptor *configDesc = m_usbHostInterface->getConfigurationDescriptor();
	const StandardUSB::InterfaceDescriptor *ifaceDesc = m_usbHostInterface->getInterfaceDescriptor();
	if (!configDesc || !ifaceDesc)
	{
		DEBUG_LOG("%s(%p)::findPipe configDesc = %p, ifaceDesc = %p\n", getName(), this, configDesc, ifaceDesc);
		return false;
	}
	const EndpointDescriptor *ep = NULL;
	while ((ep = StandardUSB::getNextEndpointDescriptor(configDesc, ifaceDesc, ep)))
	{
		// check if endpoint matches type and direction
		uint8_t epDirection = StandardUSB::getEndpointDirection(ep);
		uint8_t epType = StandardUSB::getEndpointType(ep);
		DEBUG_LOG("%s(%p)::findPipe endpoint found: epDirection = %d, epType = %d\n", getName(), this, epDirection, epType);
		if (direction == epDirection && type == epType)
		{
			DEBUG_LOG("%s(%p)::findPipe found matching endpoint\n", getName(), this);
			
			// matches... try to make a pipe from the endpoint address
			*pipe = m_usbHostInterface->copyPipe(StandardUSB::getEndpointAddress(ep));
			if (*pipe == NULL)
			{
				DEBUG_LOG("%s(%p)::findPipe copyPipe failed\n", getName(), this);
				return false;
			}
			return true;
		}
	}
	DEBUG_LOG("%s(%p)::findPipe no matching endpoint found\n", getName(), this);
	return false;
}

#endif
