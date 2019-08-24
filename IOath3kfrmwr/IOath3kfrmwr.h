/* IOath3kfrmwr class */
#ifndef __IOATH3KFRMWR__
#define __IOATH3KFRMWR__

#ifdef DEBUG_MSG
#define DEBUG_LOG(args...)  do { IOLog(args); } while(0)
#else
#define DEBUG_LOG(args...)  do { } while (0)
#endif

#include <IOKit/usb/IOUSBHostDevice.h>

#define EXPORT __attribute__((visibility("default")))

#define local_IOath3kfrmwr org_rehabman_IOath3kfrmwr

class EXPORT local_IOath3kfrmwr : public IOService
{
    typedef IOService super;
    OSDeclareDefaultStructors(org_rehabman_IOath3kfrmwr)
    
protected:
    IOUSBHostDevice * m_usbHostDevice;
	IOUSBHostInterface *m_usbHostInterface;
	IOUSBHostPipe *m_bulkPipe;
        
public:
#ifdef DEBUG
    // this is from the IORegistryEntry - no provider yet
    virtual bool init(OSDictionary *propTable);
    
    // IOKit methods. These methods are defines in <IOKit/IOService.h>
    virtual IOService* probe(IOService *provider, SInt32 *score );
    
    virtual bool attach(IOService *provider);
    virtual void detach(IOService *provider);
#endif
    
    virtual bool start(IOService *provider);
#ifdef DEBUG
    virtual void stop(IOService *provider);
    
    virtual bool handleOpen(IOService *forClient, IOOptionBits options = 0, void *arg = 0 );
    virtual void handleClose(IOService *forClient, IOOptionBits options = 0 );
    
    virtual IOReturn message(UInt32 type, IOService *provider, void *argument);
    
    virtual bool terminate(IOOptionBits options = 0);
    virtual bool finalize(IOOptionBits options);
#endif
	virtual IOReturn initFirmwareTransfer(void *firmwareBuffer, uint32_t size);
	virtual bool findFirstInterface();
	virtual IOReturn getStatus(uint16_t *statusNumber);
	virtual bool findPipe(IOUSBHostPipe **pipe, uint8_t type, uint8_t direction);
};

#endif //__IOATH3KFRMWR__ 

