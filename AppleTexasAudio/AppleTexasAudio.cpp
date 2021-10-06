/*
 *	AppleTexasAudio.cpp
 *	AppleOnboardAudio
 *
 *	Created by nthompso on Tue Jul 03 2001.
 *
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
 * License.	 Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * This file contains a template for an AppleOnboardAudio based driver.
 * The driver is derived from the AppleOnboardAudio class.
 *
 */
 
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <UserNotification/KUNCUserNotifications.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>

#include "AppleTexasAudio.h"

#include "AppleTexasEQPrefs.cpp"

#define super AppleOnboardAudio
#define durationMillisecond 1000	// number of microseconds in a millisecond

OSDefineMetaClassAndStructors(AppleTexasAudio, AppleOnboardAudio)

// Globals in this file
EQPrefsPtr		gEQPrefs = &theEQPrefs;
extern uid_t	console_user;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark +UNIX LIKE FUNCTIONS

// init, free, probe are the "Classical Unix driver functions" that you'll like as not find
// in other device drivers.	 Note that there are no start and stop methods.	 The code for start 
// effectively moves to the sndHWInitialize routine.  Also note that the initHardware method 
// effectively does very little other than calling the inherited method, which in turn calls
// sndHWInitialize, so all of the init code should be in the sndHWInitialize routine.

// ::init()
// call into superclass and initialize.
bool AppleTexasAudio::init(OSDictionary *properties)
{
	debugIOLog("+ AppleTexasAudio::init\n");
	if (!super::init(properties))
		return false;

	gVolLeft = 0;
	gVolRight = 0;
	gVolMuteActive = false;
	gModemSoundActive = false;

	debugIOLog("- AppleTexasAudio::init\n");
	return true;
}

// ::free
// call inherited free
void AppleTexasAudio::free()
{
	IOWorkLoop				*workLoop;

	debugIOLog("+ AppleTexasAudio::free\n");

	CLEAN_RELEASE(hdpnMuteRegMem);
	CLEAN_RELEASE(ampMuteRegMem);
	CLEAN_RELEASE(hwResetRegMem);
	CLEAN_RELEASE(headphoneExtIntGpioMem);
	CLEAN_RELEASE(dallasExtIntGpioMem);

	workLoop = getWorkLoop();
	if (NULL != workLoop) {
		if (NULL != headphoneIntEventSource && NULL != headphoneIntProvider)
			workLoop->removeEventSource (headphoneIntEventSource);
		if (NULL != dallasIntEventSource && NULL != dallasIntProvider)
			workLoop->removeEventSource (dallasIntEventSource);
		if (NULL != dallasHandlerTimer)
			workLoop->removeEventSource (dallasHandlerTimer);
		if (NULL != notifierHandlerTimer)
			workLoop->removeEventSource (notifierHandlerTimer);
	}

	CLEAN_RELEASE(headphoneIntEventSource);
	CLEAN_RELEASE(dallasIntEventSource);

	super::free();
	debugIOLog("- AppleTexasAudio::free\n");
}

// ::probe
// called at load time, to see if this driver really does match with a device.	In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleTexasAudio::probe(IOService *provider, SInt32 *score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
	IORegistryEntry *sound = 0;
	debugIOLog("+ AppleTexasAudio::probe\n");

	super::probe(provider, score);
	*score = kIODefaultProbeScore;
	sound = provider->childFromPath("sound", gIODTPlane);
	//we are on a new world : the registry is assumed to be fixed
	if(sound) {
		OSData *tmpData;

		tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
		if(tmpData) {
			if(tmpData->isEqualTo(kTexasModelName, sizeof(kTexasModelName) -1)) {
				*score = *score+1;
				debugIOLog("++ AppleTexasAudio::probe increasing score\n");
				return(this);
			} else {
				debugIOLog ("++ AppleTexasAudio::probe, didn't find what we were looking for\n");
			}
		}
		sound->release ();
	}

	debugIOLog("- AppleTexasAudio::probe\n");
	return (0);
}

// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.	 All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleTexasAudio::initHardware(IOService *provider)
{
	bool myreturn = true;

	DEBUG_IOLOG("+ AppleTexasAudio::initHardware\n");
	
	// calling the superclasses initHarware will indirectly call our
	// sndHWInitialize() method.  So we don't need to do a whole lot 
	// in this function.
	super::initHardware(provider);
	
	DEBUG_IOLOG("- AppleTexasAudio::initHardware\n");
	return myreturn;
}

// --------------------------------------------------------------------------
// Method: timerCallback
//
// Purpose:
//		  This is a static method that gets called from a timer task at regular intervals
//		  Generally we do not want to do a lot of work here, we simply turn around and call
//		  the appropriate method to perform out periodic tasks.

void AppleTexasAudio::timerCallback(OSObject *target, IOAudioDevice *device)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexasAudio::timerCallback\n");
	AppleTexasAudio *			templateDriver;
	
	templateDriver = OSDynamicCast (AppleTexasAudio, target);

	if (templateDriver) {
		templateDriver->checkStatus(false);
	}
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexasAudio::timerCallback\n");
	return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//		 poll the detects, note this should prolly be done with interrupts rather
//		 than by polling if interrupts are supported

void AppleTexasAudio::checkStatus(
	bool force)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexasAudio::checkStatus\n");

// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexasAudio::checkStatus\n");


}

/*************************** sndHWXXXX functions******************************/
/*	 These functions should be common to all Apple hardware drivers and be	 */
/*	 declared as virtual in the superclass. This is the only place where we	 */
/*	 should manipulate the hardware. Order is given by the UI policy system	 */
/*	 The set of functions should be enough to implement the policy			 */
/*****************************************************************************/


#pragma mark +HARDWARE REGISTER MANIPULATION
/************************** Hardware Register Manipulation ********************/
// Hardware specific functions : These are all virtual functions and we have to 
// implement these in the driver class

// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
void	AppleTexasAudio::sndHWInitialize(IOService *provider)
{
	IOReturn				err;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2c;
	IORegistryEntry			*deq;
	IORegistryEntry			*intSource;
	IORegistryEntry			*headphoneMute;
	IORegistryEntry			*ampMute;
	IORegistryEntry			*hardwareReset;
	OSData					*tmpData;
	IOMemoryMap				*map;
	UInt32					*hdpnMuteGpioAddr;
	UInt32					*ampMuteGpioAddr;
	UInt32					*hwResetGpioAddr;
	UInt32					*headphoneExtIntGpioAddr;
	UInt32					*dallasExtIntGpioAddr;
	UInt32					*i2cAddrPtr;
	UInt32					*tmpPtr;
	UInt32					loopCnt;
	UInt32					myFrameRate;
	UInt8					data[kBIQwidth];						// space for biggest register size
	UInt8					curValue;
	bool					hasInput;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWInitialize\n");

	ourProvider = provider;
	hasInput = (bool) HasInput ();
	savedNanos = 0;

	// Sets the frame rate:
	myFrameRate = frameRate (0);

	i2s = NULL;
	macio = NULL;
	gpio = NULL;
	i2c = NULL;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	macio = i2s->getParentEntry (gIODTPlane);
	FailIf (!macio, Exit);
	gpio = macio->childFromPath (kGPIODTEntry, gIODTPlane);
	FailIf (!gpio, Exit);
	i2c = macio->childFromPath (kI2CDTEntry, gIODTPlane);
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	//	Determine which systems to exclude from the default behavior of releasing the headphone
	//	mute after 200 milliseconds delay [2660341].
	tmpData = OSDynamicCast (OSData, macio->getProperty (kDeviceIDPropName));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32)tmpData->getBytesNoCopy ();
	ExcludeHPMuteRelease (deviceID);

	// get the physical address of the i2c cell that the sound chip (Digital EQ, "deq") is connected to.
	deq = i2c->childFromPath (kDigitalEQDTEntry, gIODTPlane);
	FailIf (!deq, Exit);
	tmpData = OSDynamicCast (OSData, deq->getProperty (kI2CAddress));
	deq->release ();
	FailIf (!tmpData, Exit);
	i2cAddrPtr = (UInt32*)tmpData->getBytesNoCopy ();
	DEQAddress = *i2cAddrPtr;
	DEQAddress = DEQAddress >> 1;	// have to shift down because I2C driver will shift up on writes

	// get the physical address of the gpio pin for setting the headphone mute
	headphoneMute = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneAmpEntry);
	FailIf (!headphoneMute, Exit);
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hdpnMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	hdpnActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	hdpnMuteRegMem = IODeviceMemory::withRange (*hdpnMuteGpioAddr, sizeof (UInt8));
	map = hdpnMuteRegMem->map (0);
	hdpnMuteGpio = (UInt8*)map->getVirtualAddress ();

	intSource = 0;

	// get the interrupt provider for the Dallas speaker insertion interrupt
	intSource = FindEntryByProperty (gpio, kOneWireBus, kSpeakerID);

	if (NULL != intSource) {
		dallasIntProvider = OSDynamicCast (IOService, intSource);
		FailIf (!dallasIntProvider, Exit);

		// get the active state of the dallas speaker inserted pin
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
		if (!tmpData) {
			dallasInsertedActiveState = 1;
		} else {
			tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
			dallasInsertedActiveState = *tmpPtr;
		}

		// get the physical address of the pin for detecting the dallas speaker insertion/removal
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
		FailIf (!tmpData, Exit);
		dallasExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

		// take the hard coded memory address that's in the boot rom, and convert it a virtual address
		dallasExtIntGpioMem = IODeviceMemory::withRange (*dallasExtIntGpioAddr, sizeof (UInt8));
		map = dallasExtIntGpioMem->map (0);
		dallasExtIntGpio = (UInt8*)map->getVirtualAddress ();

		curValue = *dallasExtIntGpio;
		curValue = curValue | (1 << 7);
		*dallasExtIntGpio = curValue;
	} else {
		debugIOLog ("!!!!Couldn't find a dallas speaker interrupt source!!!!\n");
	}

	intSource = 0;

	// get the interrupt provider for the headphone insertion interrupt
	intSource = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneDetectInt);
	if (!intSource)
		intSource = FindEntryByProperty (gpio, kCompatible, kKWHeadphoneDetectInt);

	FailIf (!intSource, Exit);
	headphoneIntProvider = OSDynamicCast (IOService, intSource);
	FailIf (!headphoneIntProvider, Exit);

	// We only want to publish the jack state if this hardware has video on its headphone connector
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kVideoPropertyEntry));
	debug2IOLog ("kVideoPropertyEntry = %p\n", tmpData);
	if (NULL != tmpData) {
		hasVideo = TRUE;
		gAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
		debugIOLog ("has video in headphone\n");
	}

	// get the active state of the headphone inserted pin
	// This should really be gotten from the sound-objects property, but we're not parsing that yet.
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
	if (NULL == tmpData) {
		headphoneInsertedActiveState = 1;
	} else {
		tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
		headphoneInsertedActiveState = *tmpPtr;
	}

	// get the physical address of the pin for detecting the headphone insertion/removal
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	headphoneExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	headphoneExtIntGpioMem = IODeviceMemory::withRange (*headphoneExtIntGpioAddr, sizeof (UInt8));
	map = headphoneExtIntGpioMem->map (0);
	headphoneExtIntGpio = (UInt8*)map->getVirtualAddress ();
	
	curValue = *headphoneExtIntGpio;
	curValue = curValue | (1 << 7);
	*headphoneExtIntGpio = curValue;

	// get the physical address of the gpio pin for setting the amplifier mute
	ampMute = FindEntryByProperty (gpio, kAudioGPIO, kAmpEntry);
	FailIf (!ampMute, Exit);
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	ampMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	ampActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	ampMuteRegMem = IODeviceMemory::withRange (*ampMuteGpioAddr, sizeof (UInt8));
	map = ampMuteRegMem->map (0);
	ampMuteGpio = (UInt8*)map->getVirtualAddress ();

	// get the physical address of the gpio pin for setting the hardware reset
	hardwareReset = FindEntryByProperty (gpio, kAudioGPIO, kHWResetEntry);
	FailIf (!hardwareReset, Exit);
	tmpData = OSDynamicCast (OSData, hardwareReset->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hwResetGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, hardwareReset->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	hwResetActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	hwResetRegMem = IODeviceMemory::withRange (*hwResetGpioAddr, sizeof (UInt8));
	map = hwResetRegMem->map (0);
	hwResetGpio = (UInt8*)map->getVirtualAddress ();

	FailIf (!findAndAttachI2C (provider), Exit);

	layoutID = GetDeviceID ();
	debug2IOLog ("layoutID = %ld\n", layoutID);

	i2sSerialFormat = 0x41190000;

	drc.compressionRatioNumerator		= kDrcRatioNumerator;
	drc.compressionRatioDenominator		= kDrcRationDenominator;
	drc.threshold						= kDrcThresholdMax;
	drc.maximumVolume					= kDefaultMaximumVolume;
	drc.enable							= false;

	//	Initialize the TAS3001C as follows:
	//		Mode:						normal
	//		SCLK:						64 fs
	//		input serial mode:			i2s
	//		output serial mode:			i2s
	//		serial word length:			16 bits
	//		Dynamic range control:		disabled
	//		Volume (left & right):		muted
	//		Treble / Bass:				unity
	//		Biquad filters:				unity
	data[0] = ( kNormalLoad << kFL ) | ( k64fs << kSC ) | TAS_I2S_MODE | ( TAS_WORD_LENGTH << kW0 );
	TAS3001C_WriteRegister( kMainCtrlReg, data, kUPDATE_SHADOW );	//	default to normal load mode, 16 bit I2S

	data[0] = ( kDrcDisable << kEN ) | ( kCompression3to1 << kCR );
	data[1] = kDefaultCompThld;
	TAS3001C_WriteRegister( kDynamicRangeCtrlReg, data, kUPDATE_SHADOW );

	for( loopCnt = 0; loopCnt < kVOLwidth; loopCnt++ )				//	init to volume = muted
		data[loopCnt] = 0;
	TAS3001C_WriteRegister( kVolumeCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x72;													//	treble = unity 0.0 dB
	TAS3001C_WriteRegister( kTrebleCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x3E;													//	bass = unity = 0.0 dB
	TAS3001C_WriteRegister( kBassCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x10;													//	output mixer to unity = 0.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	TAS3001C_WriteRegister( kMixer1CtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x00;													//	call progress mixer to mute = -70.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	TAS3001C_WriteRegister( kMixer2CtrlReg, data, kUPDATE_SHADOW );

	for( loopCnt = 1; loopCnt < kBIQwidth; loopCnt++ )				//	all biquads to unity gain all pass mode
		data[loopCnt] = 0x00;
	data[0] = 0x10;

	TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad0CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad1CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad2CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad3CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad4CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad5CtrlReg, data, kUPDATE_SHADOW );

	// All this config should go in a single method: 
	map = provider->mapDeviceMemoryWithIndex (AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
	FailIf (!map, Exit);
	soundConfigSpace = (UInt8 *)map->getPhysicalAddress ();
	FailIf (!soundConfigSpace, Exit);

	// sets the clock base address figuring out which I2S cell we're on
	if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) {
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
		i2SInterfaceNumber = 0;
	} else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) {
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
		i2SInterfaceNumber = 1;
	} else {
		FailIf("AppleTexasAudio::start failed to setup ioBaseAddress and ioClockBaseAddress\n", Exit);
	}

	// This is the keylargo FC1 (Feature configuration 1)
	ioClockBaseAddress = (void *)((UInt32)ioBaseAddress + kI2SClockOffset);

	// Enables the I2S Interface:
	debug2IOLog ("KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable = 0x%8.0lX\n", KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable);

	// This call will set the next of the frame parameters
	// (clockSource, mclkDivisor,  sclkDivisor)
	FailIf (!setSampleParameters(myFrameRate, 256), Exit);
	FailIf (!setHWSampleRate(myFrameRate), Exit);
	setSerialFormatRegister(clockSource, mclkDivisor, sclkDivisor, serialFormat);

	err = TAS3001C_Initialize( kFORCE_RESET_SETUP_TIME );			//	reset the TAS3001C and flush the shadow contents to the HW

	dallasDriver = NULL;
	dallasDriverNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleDallasDriver"), (IOServiceNotificationHandler)&DallasDriverPublished, this);
	if (NULL != dallasDriver)
		dallasDriverNotifier->remove ();

Exit:
	if (NULL != gpio)
		gpio->release ();
	if (NULL != i2c)
		i2c->release ();

	DEBUG_IOLOG("- AppleTexasAudio::sndHWInitialize\n");
	return;
}

void AppleTexasAudio::sndHWPostDMAEngineInit (IOService *provider) {
	IOWorkLoop				*workLoop;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWPostDMAEngineInit\n");

	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);

	if (NULL != headphoneIntProvider) {
		headphoneIntEventSource = IOInterruptEventSource::interruptEventSource (this,
																			headphoneInterruptHandler,
																			headphoneIntProvider,
																			0);
		FailIf (NULL == headphoneIntEventSource, Exit);
		workLoop->addEventSource (headphoneIntEventSource);
	}

	// Create a (primary) device interrupt source for dallas and attach it to the work loop
	if (NULL != dallasIntProvider) {
		// Create a timer event source
		dallasHandlerTimer = IOTimerEventSource::timerEventSource (this, DallasInterruptHandlerTimer);
		if (NULL != dallasHandlerTimer) {
			workLoop->addEventSource (dallasHandlerTimer);
		}

 		notifierHandlerTimer = IOTimerEventSource::timerEventSource (this, DisplaySpeakersNotFullyConnected);
		if (NULL != notifierHandlerTimer) {
			workLoop->addEventSource (notifierHandlerTimer);
		}

		dallasIntEventSource = IOFilterInterruptEventSource::filterInterruptEventSource (this,
																			dallasInterruptHandler, 
																			interruptFilter,
																			dallasIntProvider, 
																			0);
		FailIf (NULL == dallasIntEventSource, Exit);
		workLoop->addEventSource (dallasIntEventSource);
	}

	if (FALSE == IsHeadphoneConnected ()) {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
		if (TRUE == hasVideo) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			dallasInterruptHandler (this, 0, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			RealHeadphoneInterruptHandler (0, 0);
		}
	}

	if (NULL != headphoneIntEventSource)
		headphoneIntEventSource->enable ();
	if (NULL != dallasIntEventSource)
		dallasIntEventSource->enable ();

Exit:
	DEBUG_IOLOG("- AppleTexasAudio::sndHWPostDMAEngineInit\n");
	return;
}

UInt32	AppleTexasAudio::sndHWGetInSenseBits(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetInSenseBits\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetInSenseBits\n");
	return 0;	  
}

// we can't read the registers back, so return the value in the shadow reg.
UInt32	AppleTexasAudio::sndHWGetRegister(
	UInt32 regNum)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetRegister\n");
	
	UInt32	returnValue = 0;
	
	 return returnValue;
}

// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleTexasAudio::sndHWSetRegister(
	UInt32 regNum, 
	UInt32 val)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetRegister\n");
	IOReturn myReturn = kIOReturnSuccess;
	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetRegister\n");
	return(myReturn);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32	AppleTexasAudio::sndHWGetActiveOutputExclusive(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetActiveOutputExclusive\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetActiveOutputExclusive\n");
	return 0;
}

IOReturn   AppleTexasAudio::sndHWSetActiveOutputExclusive(
	UInt32 outputPort )
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetActiveOutputExclusive\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetActiveOutputExclusive\n");
	return(myReturn);
}

UInt32	AppleTexasAudio::sndHWGetActiveInputExclusive(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetActiveInputExclusive\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetActiveInputExclusive\n");
	return 0;
}

IOReturn   AppleTexasAudio::sndHWSetActiveInputExclusive(
	UInt32 input )
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetActiveInputExclusive\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetActiveInputExclusive\n");
	return(myReturn);
}

#pragma mark +CONTROL FUNCTIONS
// control function
bool AppleTexasAudio::sndHWGetSystemMute(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetSystemMute\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetSystemMute\n");
	return (gVolMuteActive);
}

IOReturn AppleTexasAudio::setModemSound(bool state) {
	UInt8 data[3];
	IOReturn myReturn = kIOReturnSuccess;

	if(gModemSoundActive == state) 
		goto EXIT;

	if(state) {	   //we turned it on
		data[0] = 0x10;
	} else {
		data[0] = 0x00;
	}					  
																							
	data[1] = 0x00;
	data[2] = 0x00;
	myReturn = TAS3001C_WriteRegister( kMixer2CtrlReg, data, kUPDATE_HW );
	gModemSoundActive = state;

EXIT:
	return(myReturn);
}
/* I'm not sure if this is still needed or not...
IOReturn AppleTexasAudio::callPlatformFunction( const OSSymbol * functionName,bool
			waitForFunction,void *param1, void *param2, void *param3, void *param4 ){
			
	if(functionName->isEqualTo("setModemSound")) {
		return(setModemSound((bool)param1));
	}		

	return(super::callPlatformFunction(functionName,
			waitForFunction,param1, param2, param3, param4));
}
*/

IOReturn AppleTexasAudio::sndHWSetSystemMute(bool mutestate)
{
	IOReturn						result;

	result = kIOReturnSuccess;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetSystemMute\n");

	if (true == mutestate) {
		if (false == gVolMuteActive) {
			// mute the part
			gVolMuteActive = mutestate ;
			result = SetVolumeCoefficients (0, 0);
		}
	} else {
		// unmute the part
		gVolMuteActive = mutestate ;
		result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);
	}

	DEBUG_IOLOG ("- AppleTexasAudio::sndHWSetSystemMute\n");
	return (result);
}

bool AppleTexasAudio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool					result;

	result = false;

	debug3IOLog ("+ AppleTexasAudio::sndHWSetSystemVolume (left: %ld, right %ld)\n", leftVolume, rightVolume);
	gVolLeft = leftVolume;
	gVolRight = rightVolume;
	result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetSystemVolume\n");
	return (result == kIOReturnSuccess);
}

IOReturn AppleTexasAudio::sndHWSetSystemVolume(UInt32 value)
{
	DEBUG2_IOLOG("+ AppleTexasAudio::sndHWSetSystemVolume (vol: %ld)\n", value);
	
	IOReturn myReturn = kIOReturnError;
		
	// just call the default function in this class with the same val for left and right.
	if( true == sndHWSetSystemVolume( value, value ))
	{
		myReturn = kIOReturnSuccess;
	}

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetSystemVolume\n");
	return(myReturn);
}

IOReturn AppleTexasAudio::sndHWSetPlayThrough(bool playthroughstate)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetPlayThrough\n");
	return(myReturn);
}

IOReturn AppleTexasAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetPlayThrough\n");
	return(myReturn);
}

IOReturn AppleTexasAudio::AdjustControls (void) {
	IOAudioEngine *					audioEngine;
	IOFixed							mindBVol;
	IOFixed							maxdBVol;

	FailIf (NULL == audioEngines, Exit);

	audioEngine = OSDynamicCast (IOAudioEngine, audioEngines->getObject (0));
	FailIf (NULL == audioEngine, Exit);

	mindBVol = volumedBTable[minVolume];
	maxdBVol = volumedBTable[maxVolume];
	debug3IOLog ("AdjustControls: mindBVol = %lx, maxdBVol = %lx\n", mindBVol, maxdBVol);

	if (NULL != outVolLeft && NULL != outVolRight) {
		audioEngine->pauseAudioEngine ();
		audioEngine->beginConfigurationChange ();
		outVolLeft->setMinValue (minVolume);
		outVolLeft->setMinDB (mindBVol);
		outVolLeft->setMaxValue (maxVolume);
		outVolLeft->setMaxDB (maxdBVol);
		outVolRight->setMinValue (minVolume);
		outVolRight->setMinDB (mindBVol);
		outVolRight->setMaxValue (maxVolume);
		outVolRight->setMaxDB (maxdBVol);
		audioEngine->completeConfigurationChange ();
		audioEngine->resumeAudioEngine ();
	}

Exit:
	return kIOReturnSuccess;
}

#pragma mark +INDENTIFICATION

// ::sndHWGetType
//Identification - the only thing this driver supports is the DACA3550 part, return that.
UInt32 AppleTexasAudio::sndHWGetType(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetType\n");
 
	// in AudioHardwareConstants.h need to set up a constant for the hardware type.
	returnValue = kSndHWTypeTumbler;

	DEBUG_IOLOG ("- AppleTexasAudio::sndHWGetType\n");
	return returnValue ;
}

// ::sndHWGetManufactuer
// return the detected part's manufacturer.	 I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleTexasAudio::sndHWGetManufacturer(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetManufacturer\n");

	// in AudioHardwareConstants.h need to set up a constant for the part manufacturer.
	returnValue = kSndHWManfTI ;
	
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetManufacturer\n");
	return returnValue ;
}

#pragma mark +DETECT ACTIVATION & DEACTIVATION
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleTexasAudio::setDeviceDetectionActive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::setDeviceDetectionActive\n");
	mCanPollStatus = true ;
	
	DEBUG_IOLOG("- AppleTexasAudio::setDeviceDetectionActive\n");
	return ;
}

// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleTexasAudio::setDeviceDetectionInActive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::setDeviceDetectionInActive\n");
	mCanPollStatus = false ;
	
	DEBUG_IOLOG("- AppleTexasAudio::setDeviceDetectionInActive\n");
	return ;
}

#pragma mark +POWER MANAGEMENT
//Power Management

IOReturn AppleTexasAudio::sndHWSetPowerState (IOAudioDevicePowerState theState) {
	IOReturn							result;

	debug2IOLog ("+ AppleTexasAudio::sndHWSetPowerState (%d)\n", theState);

	result = kIOReturnSuccess;
	switch (theState) {
		case kIOAudioDeviceActive:
			result = performDeviceWake ();
			completePowerStateChange ();
			break;
		case kIOAudioDeviceIdle:
		case kIOAudioDeviceSleep:
			result = performDeviceSleep ();
			break;
	}

	debugIOLog ("- AppleTexasAudio::sndHWSetPowerState\n");
	return result;
}

IOReturn AppleTexasAudio::performDeviceSleep () {
    IOService *							keyLargo;

	debugIOLog ("+ AppleTexasAudio::performDeviceSleep\n");

	keyLargo = NULL;

	// Mute the amps to avoid pops and clicks...
	SetActiveOutput (kSndHWOutputNone, kTouchBiquad);

	// ...then hold the RESET line...
	GpioWrite (hwResetGpio, ASSERT_GPIO (hwResetActiveState));

    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		// ...and turn off the i2s clocks...
		keyLargo->callPlatformFunction (OSSymbol::withCString ("keyLargo_powerI2S"), false, (void *)false, (void *)0, 0, 0);
	}

	debugIOLog ("- AppleTexasAudio::performDeviceSleep\n");
	return kIOReturnSuccess;
}
	
IOReturn AppleTexasAudio::performDeviceWake () {
    IOService *							keyLargo;
	IOReturn							err;

	debugIOLog ("+ AppleTexasAudio::performDeviceWake\n");

	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		// Turn on the i2s clocks...
		keyLargo->callPlatformFunction (OSSymbol::withCString ("keyLargo_powerI2S"), false, (void *)true, (void *)0, 0, 0);
	}

	// ...and then release the RESET line...
	GpioWrite (hwResetGpio, NEGATE_GPIO (hwResetActiveState));

	// ...then bring everything back up the way it should be.
	err = TAS3001C_Initialize (kFORCE_RESET_SETUP_TIME);			//	reset the TAS3001C and flush the shadow contents to the HW

	if (FALSE == IsHeadphoneConnected ()) {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
		if (TRUE == hasVideo) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			dallasInterruptHandler (this, 0, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			RealHeadphoneInterruptHandler (0, 0);
		}
	}

	debugIOLog ("- AppleTexasAudio::performDeviceWake\n");
	return err;
}

// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleTexasAudio::sndHWGetConnectedDevices(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetConnectedDevices\n");
   UInt32	returnValue = currentDevices;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetConnectedDevices\n");
	return returnValue ;
}

UInt32 AppleTexasAudio::sndHWGetProgOutput(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetProgOutput\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetProgOutput\n");
	return 0;
}

IOReturn AppleTexasAudio::sndHWSetProgOutput(
	UInt32 outputBits)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetProgOutput\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetProgOutput\n");
	return(myReturn);
}

#pragma mark +INTERRUPT HANDLERS
Boolean AppleTexasAudio::IsSpeakerConnected (void) {
	UInt8						dallasSenseContents;
	Boolean						connection;

	connection = FALSE;
	if (NULL != dallasIntProvider) {
		dallasSenseContents = *(dallasExtIntGpio);

		debug3IOLog ("dallasExtIntGpio = %p, dallasSenseContents = 0x%X\n", dallasExtIntGpio, dallasSenseContents);
		if ((dallasSenseContents & (1 << 1)) == (dallasInsertedActiveState << 1)) {
			debugIOLog ("dallas speakers are connected\n");
			connection = TRUE;
		} else {
			debugIOLog ("dallas speakers are NOT connected\n");
			connection = FALSE;
		}
	}

	return connection;
}

void AppleTexasAudio::DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
    AppleTexasAudio *			device;
	AbsoluteTime				currTime;

	device = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (NULL == device, Exit);

	device->dallasSpeakersConnected = device->IsSpeakerConnected ();
#if DEBUGLOG
	IOLog ("dallas speakers connected = %d\n", device->dallasSpeakersConnected);
#endif

	if (FALSE == device->IsHeadphoneConnected ()) {
		// Set the proper EQ
		device->DeviceInterruptService ();

		clock_get_uptime (&currTime);
		absolutetime_to_nanoseconds (currTime, &device->savedNanos);
	}

	if (NULL != device->dallasIntEventSource) {
		device->dallasIntEventSource->enable();
	}

Exit:
	return;
}

// Set a flag to say if the dallas speakers are plugged in or not so we know which EQ to use.
void AppleTexasAudio::RealDallasInterruptHandler (IOInterruptEventSource *source, int count) {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	// call DallasInterruptHandlerTimer in a bit to check for the dallas rom (and complete speaker insertion).
	if (NULL != dallasHandlerTimer) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kInsertionDelayNanos;	// Schedule 4s in the future...

		nanoseconds_to_absolutetime (nanos, &fireTime);
		dallasHandlerTimer->wakeAtTime (fireTime);
	}

	return;
}

// Put changes in RealDallasInterruptHandler, not in here
void AppleTexasAudio::dallasInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count) {
	AbsoluteTime 			currTime;
	UInt64 					currNanos;
	AppleTexasAudio *		appleTexasAudio;

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	// Need this disable for when we call dallasInterruptHandler instead of going through the interrupt filter
	appleTexasAudio->dallasIntEventSource->disable();

	clock_get_uptime (&currTime);
	absolutetime_to_nanoseconds (currTime, &currNanos);

	if ((currNanos - appleTexasAudio->savedNanos) > 10000000) {
		appleTexasAudio->RealDallasInterruptHandler (source, count);
	} else { 
		appleTexasAudio->dallasIntEventSource->enable();
	}

Exit:
	return;
}

// static "action" function to connect to our object
// return TRUE if you want the handler function to be called, or FALSE if you don't want it to be called.
bool AppleTexasAudio::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleTexasAudio* self = (AppleTexasAudio*) owner;

	self->dallasIntEventSource->disable();
	return (true);
}

// This is called to tell the user that they may not have plugged their speakers in all the way.
void AppleTexasAudio::DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender) {
	AppleTexasAudio *		appleTexasAudio;
	AbsoluteTime			currTime;
	UInt32					deviceID;
	UInt8					bROM[8];
	UInt8					bEEPROM[32];
	UInt8					bAppReg[8];
	Boolean					result;

	debugIOLog ("+ DisplaySpeakersNotFullyConnected\n");

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	if (0 == console_user) {
		appleTexasAudio->notifierHandlerTimer->setTimeoutMS (kNotifyTimerDelay);	//No one logged in yet (except maybe root) reset the timer to fire later. 
	} else {
		if (appleTexasAudio->doneWaiting == FALSE) { 
			// The next time this function is called we'll check the state and display the dialog as needed
			appleTexasAudio->notifierHandlerTimer->setTimeoutMS (kUserLoginDelay);	// Someone has logged in. Delay the notifier so it does not apear behind the login screen.
			appleTexasAudio->doneWaiting = TRUE;
		} else {
			deviceID = appleTexasAudio->GetDeviceMatch ();
			if (kExternalSpeakersActive == deviceID) {
				if (NULL != appleTexasAudio->dallasDriver) {
					appleTexasAudio->dallasIntEventSource->disable ();
					result = appleTexasAudio->dallasDriver->getSpeakerID (bROM, bEEPROM, bAppReg);
					appleTexasAudio->dallasIntEventSource->enable ();
					clock_get_uptime (&currTime);
					absolutetime_to_nanoseconds (currTime, &appleTexasAudio->savedNanos);

					if (TRUE == result) {	// TRUE == failure for DallasDriver
						KUNCUserNotificationDisplayNotice (
						0,		// Timeout in seconds
						0,		// Flags (for later usage)
						"",		// iconPath (not supported yet)
						"",		// soundPath (not supported yet)
						"/System/Library/Extensions/AppleOnboardAudio.kext",		// localizationPath
						"HeaderOfDallasPartialInsert",		// the header
						"StringOfDallasPartialInsert",
						"ButtonOfDallasPartialInsert"); 
	
						IOLog ("The device plugged into the Apple speaker mini-jack cannot be recognized.\n");
						IOLog ("Remove the plug from the jack. Then plug it back in and make sure it is fully inserted.\n");
					} else {
						// Speakers are fully plugged in now, so load the proper EQ for them
						appleTexasAudio->dallasIntEventSource->disable ();
						appleTexasAudio->DeviceInterruptService ();
						appleTexasAudio->dallasIntEventSource->enable ();
						clock_get_uptime (&currTime);
						absolutetime_to_nanoseconds (currTime, &appleTexasAudio->savedNanos);
					}
				}
			}
		}
	}

Exit:
	debugIOLog ("- DisplaySpeakersNotFullyConnected\n");
    return;
}

Boolean AppleTexasAudio::IsHeadphoneConnected (void) {
	UInt8				headphoneSenseContents;
	Boolean				connection;

	// check the state of the extint-gpio15 pin for the actual state of the headphone jack
	// do this because we get a false interrupt when waking from sleep that makes us think
	// that the headphones were removed during sleep, even if they are still connected.

	connection = FALSE;
	if (NULL != headphoneIntEventSource) {
		headphoneSenseContents = *headphoneExtIntGpio;

		debug3IOLog ("headphoneExtIntGpio = %p, headphoneSenseContents = 0x%X\n", headphoneExtIntGpio, headphoneSenseContents);
		if ((headphoneSenseContents & (1 << 1)) == (headphoneInsertedActiveState << 1)) {
			// headphones are inserted
			debugIOLog ("Headphones are inserted\n");
			connection = TRUE;
		} else {
			// headphones are not inserted
			debugIOLog ("Headphones are not inserted\n");
			connection = FALSE;
		}
	}

	return connection;
}

void AppleTexasAudio::RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count) {
	headphonesConnected = IsHeadphoneConnected ();

	if (TRUE == headphonesConnected) {
		SetActiveOutput (kSndHWOutput1, kTouchBiquad);
	} else {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
	}

	if (TRUE == hasVideo) {
		// Tell the video driver about the jack state change in case a video connector was plugged in
		publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
	}

	DeviceInterruptService ();
}

void AppleTexasAudio::headphoneInterruptHandler (OSObject *owner, IOInterruptEventSource *source, int count) {
	AppleTexasAudio *		appleTexasAudio;

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	appleTexasAudio->RealHeadphoneInterruptHandler (source, count);

Exit:
	return;
}

#pragma mark +DIRECT HARDWARE MANIPULATION
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioDDR' bit is non-zero.
UInt8	AppleTexasAudio::GpioGetDDR( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = gpioDDR_INPUT;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDDR ) ) ) {
			result = gpioDDR_OUTPUT;
		}
#if DEBUGLOG
		IOLog( "***** GPIO DDR RD 0x%8.0X = 0x%2.0X returns %d\n", (unsigned int)gpioAddress, gpioData, result );
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioData' bit is non-zero.  This function does not
//	return the state of the pin.
Boolean AppleTexasAudio::GpioRead( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDATA ) ) ) {
			result = 1;
		}
#if DEBUGLOG
		IOLog( "GpioRead( 0x%8.0X ) result %d, *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, result, *gpioAddress );
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
void	AppleTexasAudio::GpioWrite( UInt8* gpioAddress, UInt8 data )
{
	UInt8		gpioData;
	
	if( NULL != gpioAddress )
	{
		if( 0 == data )
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		else
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );
		*gpioAddress = gpioData;
#if DEBUGLOG
		IOLog( "GpioWrite( 0x%8.0X, 0x%2.0X ), *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, gpioData, *gpioAddress );
#endif
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal)
{
	IOReturn		err;
	UInt8			initData;
	UInt8			previousData;
	
	debug3IOLog ("AppleTexasAudio::InitEQSerialMode (%8lX, %d)\n", mode, restoreOnNormal);
	initData = (kNormalLoad << kFL);
	if (kSetFastLoadMode == mode)
		initData = (kFastLoad << kFL);
		
	err = TAS3001C_ReadRegister (kMainCtrlReg, &previousData);
	initData |= (k64fs << kSC) | TAS_I2S_MODE | (TAS_WORD_LENGTH << kW0);
	err = TAS3001C_WriteRegister (kMainCtrlReg, &initData, kUPDATE_ALL);
	
	//	If restoring to normal load mode then restore the settings of all
	//	registers that have been corrupted by entering fast load mode (i.e.
	//	volume, bass, treble, mixer1 and mixer2).  Restoration only occurs
	//	if going from a previous state of Fast Load to a new state of Normal Load.
	if (kRestoreOnNormal == restoreOnNormal && ((kFastLoad << kFL) == (kFastLoad << kFL) & previousData)) {
		if ((kNormalLoad << kFL) == (initData & (kFastLoad << kFL))) {
			TAS3001C_WriteRegister (kVolumeCtrlReg, (UInt8*)shadowRegs.sVOL, kUPDATE_HW);
			TAS3001C_WriteRegister (kMixer1CtrlReg, (UInt8*)shadowRegs.sMX1, kUPDATE_HW);
			TAS3001C_WriteRegister (kMixer2CtrlReg, (UInt8*)shadowRegs.sMX2, kUPDATE_HW);
			TAS3001C_WriteRegister (kTrebleCtrlReg, (UInt8*)shadowRegs.sTRE, kUPDATE_HW);
			TAS3001C_WriteRegister (kBassCtrlReg,	(UInt8*)shadowRegs.sBAS, kUPDATE_HW);
		}
	}
	debug4IOLog ("AppleTexasAudio ... %d = InitEQSerialMode (%8lX, %d)\n", err, mode, restoreOnNormal);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Updates the amplifier mute state and delays for amplifier settling if
//	the amplifier mute state is not the current mute state.
IOReturn AppleTexasAudio::SetAmplifierMuteState( UInt32 ampID, Boolean muteState )
{
	IOReturn			err;
	Boolean				curMuteState;
	
	debug3IOLog( "SetAmplifierMuteState( %ld, %d )\n", ampID, muteState );
	err = kIOReturnSuccess;
	switch( ampID )
	{
		case kHEADPHONE_AMP:
			curMuteState = GpioRead( hdpnMuteGpio );
			if( muteState != curMuteState )
			{
				GpioWrite( hdpnMuteGpio, muteState );
				debug2IOLog( "updated HEADPHONE mute to %d\n", muteState );
			}
			break;
		case kSPEAKER_AMP:
			curMuteState = GpioRead( ampMuteGpio );
			if( muteState != curMuteState )
			{
				GpioWrite( ampMuteGpio, muteState );
				debug2IOLog( "updated AMP mute to %d\n", muteState );
			}
			break;
		default:
			err = -50 /*paramErr */;
			break;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::SetVolumeCoefficients( UInt32 left, UInt32 right )
{
	UInt8		volumeData[kVOLwidth];
	IOReturn	err;
	
	debug3IOLog("SetVolumeCoefficients: L=%ld R=%ld\n", left, right);
	
	volumeData[2] = left;														
	volumeData[1] = left >> 8;												
	volumeData[0] = left >> 16;												
	
	volumeData[5] = right;														
	volumeData[4] = right >> 8;												
	volumeData[3] = right >> 16;
	
	err = TAS3001C_WriteRegister( kVolumeCtrlReg, volumeData, kUPDATE_ALL );

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will perform a reset of the TAS3001C and then initialize
//	all registers within the TAS3001C to the values already held within 
//	the shadow registers.  The RESET sequence must not be performed until
//	the I2S clocks are running.	 The TAS3001C may hold the I2C bus signals
//	SDA and SCL low until the reset sequence (high->low->high) has been
//	completed.
IOReturn	AppleTexasAudio::TAS3001C_Initialize(UInt32 resetFlag) {
	IOReturn		err;
	UInt32		retryCount;
	UInt32		initMode;
	UInt8		oldMode;
	Boolean		done;
	
	switch( resetFlag )
	{	
		case kFORCE_RESET_SETUP_TIME:		debug2IOLog( "TAS3001C_Initialize( %s )\n", "kFORCE_RESET_SETUP_TIME" );		break;
		case kNO_FORCE_RESET_SETUP_TIME:	debug2IOLog( "TAS3001C_Initialize( %s )\n", "kNO_FORCE_RESET_SETUP_TIME" ); break;
		default:							debug2IOLog( "TAS3001C_Initialize( %s )\n", "UNKNOWN" );						break;
	}
	err = -227; //siDeviceBusyErr
	done = false;
	oldMode = 0;
	initMode = kUPDATE_HW;
	retryCount = 0;
	if (!semaphores)
	{
		semaphores = 1;
		do{
			debug2IOLog( "RESETTING, retryCount %ld\n", retryCount );
			TAS3001C_Reset( resetFlag );											//	cycle reset from 1 through 0 and back to 1
			if( 0 == oldMode )
				TAS3001C_ReadRegister( kMainCtrlReg, &oldMode );					//	save previous load mode

			err = InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );								//	set fast load mode for biquad initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, (UInt8*)shadowRegs.sLB0, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, (UInt8*)shadowRegs.sLB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, (UInt8*)shadowRegs.sLB2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, (UInt8*)shadowRegs.sLB3, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, (UInt8*)shadowRegs.sLB4, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, (UInt8*)shadowRegs.sLB5, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, (UInt8*)shadowRegs.sRB0, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, (UInt8*)shadowRegs.sRB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, (UInt8*)shadowRegs.sRB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, (UInt8*)shadowRegs.sRB2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, (UInt8*)shadowRegs.sRB3, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, (UInt8*)shadowRegs.sRB4, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = InitEQSerialMode( kSetNormalLoadMode, kDontRestoreOnNormal );								//	set normal load mode for most register initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, (UInt8*)shadowRegs.sDRC, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
			err = TAS3001C_WriteRegister( kVolumeCtrlReg, (UInt8*)shadowRegs.sVOL, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kTrebleCtrlReg, (UInt8*)shadowRegs.sTRE, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kBassCtrlReg, (UInt8*)shadowRegs.sBAS, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kMixer1CtrlReg, (UInt8*)shadowRegs.sMX1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kMixer2CtrlReg, (UInt8*)shadowRegs.sMX2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kMainCtrlReg, &oldMode, initMode );			//	restore previous load mode
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
AttemptToRetry:				
			if( kIOReturnSuccess == err )		//	terminate when successful
			{
				done = true;
			}
			retryCount++;
		} while ( !done && ( kRESET_MAX_RETRY_COUNT != retryCount ) );
		semaphores = 0;
		if( kRESET_MAX_RETRY_COUNT == retryCount )
			debug2IOLog( "\n\n\n\n			TAS3001 IS DEAD: Check %s\n\n\n\n", "ChooseAudio in fcr1" );
	}
	if( kIOReturnSuccess != err )
		debug3IOLog( "TAS3001C_Initialize( %ld ) err = %d\n", resetFlag, err );

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reading registers with the TAS3001C is not possible.  A shadow register
//	is maintained for each TAS3001C hardware register.	Whenever a write
//	operation is performed on a hardware register, the data is written to 
//	the shadow register.  Read operations copy data from the shadow register
//	to the client register buffer.
IOReturn	AppleTexasAudio::TAS3001C_ReadRegister(UInt8 regAddr, UInt8* registerData) {
	UInt32			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	
	err = kIOReturnSuccess;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	switch( regAddr )
	{
		case kMainCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sMCR;	registerSize = kMCRwidth;	break;
		case kDynamicRangeCtrlReg:	shadowPtr = (UInt8*)shadowRegs.sDRC;	registerSize = kDRCwidth;	break;
		case kVolumeCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sVOL;	registerSize = kVOLwidth;	break;
		case kTrebleCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sTRE;	registerSize = kTREwidth;	break;
		case kBassCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sBAS;	registerSize = kBASwidth;	break;
		case kMixer1CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX1;	registerSize = kMIXwidth;	break;
		case kMixer2CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX2;	registerSize = kMIXwidth;	break;
		case kLeftBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB0;	registerSize = kBIQwidth;	break;
		case kLeftBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB1;	registerSize = kBIQwidth;	break;
		case kLeftBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB2;	registerSize = kBIQwidth;	break;
		case kLeftBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB3;	registerSize = kBIQwidth;	break;
		case kLeftBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB4;	registerSize = kBIQwidth;	break;
		case kLeftBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB5;	registerSize = kBIQwidth;	break;
		case kRightBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB0;	registerSize = kBIQwidth;	break;
		case kRightBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB1;	registerSize = kBIQwidth;	break;
		case kRightBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB2;	registerSize = kBIQwidth;	break;
		case kRightBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB3;	registerSize = kBIQwidth;	break;
		case kRightBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB4;	registerSize = kBIQwidth;	break;
		case kRightBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB5;	registerSize = kBIQwidth;	break;
		default:					err = -201;/*notEnoughHardware;*/									break;
	}
	if( kIOReturnSuccess == err )
	{
		for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
		{
			registerData[regByteIndex] = shadowPtr[regByteIndex];
		}
	}
	if( kIOReturnSuccess != err )
		debug4IOLog( "%d notEnoughHardware = TAS3001C_ReadRegister( 0x%2.0X, 0x%8.0X )", err, regAddr, (unsigned int)registerData );

	return err;
}

IOReturn 	AppleTexasAudio::TAS3001C_Reset(UInt32 resetFlag){
    IOReturn err = kIOReturnSuccess;

	switch( resetFlag )
	{	
		case kFORCE_RESET_SETUP_TIME:		debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "kFORCE_RESET_SETUP_TIME" );		break;
		case kNO_FORCE_RESET_SETUP_TIME:	debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "kNO_FORCE_RESET_SETUP_TIME" );	break;
		default:							debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "UNKNOWN" );						break;
	}
	if( hwResetActiveState == GpioRead( hwResetGpio ) || !GpioGetDDR( hwResetGpio ) || resetFlag )	//	if reset never was performed
	{
		GpioWrite( hwResetGpio, 0 == hwResetActiveState ? 1 : 0 );	//	negate RESET
		// IODelay is very mean, don't do it!  I think we really only have to reset it for a millisecond or two
		IODelay (100 * durationMillisecond);
	}
	else
	{
		IODelay (1 * durationMillisecond);
	}
	
	GpioWrite( hwResetGpio, hwResetActiveState );					//	Assert RESET
	IODelay (1 * durationMillisecond);

	GpioWrite( hwResetGpio, 0 == hwResetActiveState ? 1 : 0 );		//	negate RESET
	IODelay (1 * durationMillisecond);

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All TAS3001C write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  This allows a quick
//	method of re-initialization of the TAS3001C register contents when the
//	clock sources have been manipulated, resulting in loss of phase lock and
//	disabling i2c communication until a RESET has been issued to the equalizer.
//	The size of the 'registerData' is implied by the register address and is
//	stipulated within the TAS3001C specification as well as within the
//	TAS3001C_registerWidths enumeration.  This function does enforce the data
//	size of the target register.  No partial register write operations are
//	supported.	IMPORTANT:	There is no enforcement regarding 'load' mode 
//	policy.	 Client routines should properly maintain the 'load' mode by 
//	saving the contents of the master control register, set the appropriate 
//	load mode for the target register and then restore the previous 'load' 
//	mode.  All biquad registers should only be loaded while in 'fast load' 
//	mode.  All other registers should be loaded while in 'normal load' mode.
IOReturn	AppleTexasAudio::TAS3001C_WriteRegister(UInt8 regAddr, UInt8* registerData, UInt8 mode){
//	CntrlParam		pb;
	UInt32			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	Boolean			updateRequired;
	Boolean			success;
	
	err = kIOReturnSuccess;
	updateRequired = false;
	success = false;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	switch( regAddr )
	{
		case kMainCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sMCR;	registerSize = kMCRwidth;	break;
		case kDynamicRangeCtrlReg:	shadowPtr = (UInt8*)shadowRegs.sDRC;	registerSize = kDRCwidth;	break;
		case kVolumeCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sVOL;	registerSize = kVOLwidth;	break;
		case kTrebleCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sTRE;	registerSize = kTREwidth;	break;
		case kBassCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sBAS;	registerSize = kBASwidth;	break;
		case kMixer1CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX1;	registerSize = kMIXwidth;	break;
		case kMixer2CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX2;	registerSize = kMIXwidth;	break;
		case kLeftBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB0;	registerSize = kBIQwidth;	break;
		case kLeftBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB1;	registerSize = kBIQwidth;	break;
		case kLeftBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB2;	registerSize = kBIQwidth;	break;
		case kLeftBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB3;	registerSize = kBIQwidth;	break;
		case kLeftBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB4;	registerSize = kBIQwidth;	break;
		case kLeftBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB5;	registerSize = kBIQwidth;	break;
		case kRightBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB0;	registerSize = kBIQwidth;	break;
		case kRightBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB1;	registerSize = kBIQwidth;	break;
		case kRightBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB2;	registerSize = kBIQwidth;	break;
		case kRightBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB3;	registerSize = kBIQwidth;	break;
		case kRightBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB4;	registerSize = kBIQwidth;	break;
		case kRightBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB5;	registerSize = kBIQwidth;	break;
		default:					err = -201;/*notEnoughHardware;*/									break;
	}
	if( kIOReturnSuccess == err )
	{
		//	Write through to the shadow register as a 'write through' cache would and
		//	then write the data to the hardware;
		if( kUPDATE_SHADOW == mode || kUPDATE_ALL == mode )
		{
			success = true;
			for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
			{
				if( shadowPtr[regByteIndex] != registerData[regByteIndex] && kUPDATE_ALL == mode )
					updateRequired = true;
				shadowPtr[regByteIndex] = registerData[regByteIndex];
			}
		}
		if( kUPDATE_HW == mode || updateRequired )
		{
#if DEBUGLOG
			IOLog( "TAS3001C_WriteRegister addr: %2.0X subaddr: %2.0X, data = ", DEQAddress, regAddr );
			for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ ) {
				IOLog( "%2.0X ", registerData[regByteIndex] );
			}
			IOLog("\n");
#endif
			if (openI2C()) {
				success = interface->writeI2CBus (DEQAddress, regAddr, registerData, registerSize);
				closeI2C();
			} else {
				debugIOLog ("couldn't open the I2C bus!\n");
			}
		}
	}

	if( kIOReturnSuccess != err || !success ) {
		debug3IOLog ("error 0x%X returned, success == %d in AppleTexasAudio::TAS3001C_WriteRegister\n", err, success);
		if (kIOReturnSuccess == err)
			err = -1;	// force a retry
	/*
		switch( err )
		{
			case notEnoughHardware: debug3IOLog( "%d notEnoughHardware = PBControlImmed( 0x%8.0X )\n", err, pb );	break;
			case noHardware:		debug3IOLog( "%d noHardware = PBControlImmed( 0x%8.0X )\n", err, pb );		break;
			case controlErr:		debug3IOLog( "%d controlErr = PBControlImmed( 0x%8.0X )\n", err, pb );		break;
			case paramErr:			debug3IOLog( "%d paramErr = PBControlImmed( 0x%8.0X )\n", err, pb );			break;
			case ioErr:				debug3IOLog( "%d ioErr = PBControlImmed( 0x%8.0X )\n", err, pb );				break;
			case siDeviceBusyErr:	debug3IOLog( "%d siDeviceBusyErr = PBControlImmed( 0x%8.0X )\n", err, pb ); break;
			default:				debug3IOLog( "%d = PBControlImmed( 0x%8.0X )\n", err, pb );					break;
		}
	*/
	}


	return err;
}

#pragma mark +UTILITY FUNCTIONS
bool AppleTexasAudio::DallasDriverPublished (AppleTexasAudio * appleTexasAudio, void * refCon, IOService * newService) {
	bool						resultCode;

	resultCode = FALSE;

	FailIf (NULL == appleTexasAudio, Exit);
	FailIf (NULL == newService, Exit);

	appleTexasAudio->dallasDriver = (AppleDallasDriver *)newService;

	appleTexasAudio->attach (appleTexasAudio->dallasDriver);
	appleTexasAudio->dallasDriver->open (appleTexasAudio);

	if (NULL != appleTexasAudio->dallasDriverNotifier)
		appleTexasAudio->dallasDriverNotifier->remove ();

	resultCode = TRUE;

Exit:
	return resultCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Set state equal to if external speakers are around. If active, restore
// preferred mute state. If inactive, mute in hardware.
// Has to be called on the IOAudioFamily workloop (because we do the config change of the controls)!!!
//
// IMPORTANT!!!  Interrupts must be disabled before calling this function if Dallas speakers has been plugged in
// IMPORTANT!!!  to make sure that when it calls dallasDriver->getSpeakerID you don't get stuck in an infinite
// IMPORTANT!!!  loop of interrupts.
//
void AppleTexasAudio::DeviceInterruptService (void) {
	EQPrefsElementPtr	eqPrefs;
	IOReturn			err;
	UInt32				speakerID;
	UInt32				deviceID;
	Boolean				result;
	UInt8				bROM[8];
	UInt8				bEEPROM[32];
	UInt8				bAppReg[8];

	debugIOLog ("+ DeviceInterruptService\n");

	err = kIOReturnSuccess;

	deviceID = GetDeviceMatch ();
	// for 2749470
	if (NULL != driverDMAEngine) {
		if (kInternalSpeakerActive == deviceID) {
			// when it's just the internal speaker, figure out if we have to mute the right channel
			switch (layoutID) {
				case layoutTessera:
					driverDMAEngine->setRightChanMixed (TRUE);
					break;
				default:
					driverDMAEngine->setRightChanMixed (FALSE);
			}
		} else {
			// If it's external speakers or headphones, don't mute the right channel
			driverDMAEngine->setRightChanMixed (FALSE);
		}
	}

	// get the layoutID from the IORegistry for the machine we're running on (which is the machine's device-id)
	// deviceMatch is set from sound objects, but we're hard coding it using a table at the moment
	speakerID = 0;
	result = TRUE;						// TRUE == failure from dallasDriver->getSpeakerID
	if (NULL != dallasDriver && kExternalSpeakersActive == deviceID) {
		IOLog ("About to get the speaker ID\n");
		result = dallasDriver->getSpeakerID (bROM, bEEPROM, bAppReg);
		speakerID = bEEPROM[1];
	} else {
		if (NULL == dallasDriver) IOLog ("dallasDriver == NULL!\n");
		if (kExternalSpeakersActive != deviceID) IOLog ("kExternalSpeakersActive != deviceID!\n");
	}

	debug3IOLog ("DallasDriver result = %d speakerID = %ld\n", result, speakerID);

	err = GetCustomEQCoefficients (layoutID, deviceID, speakerID, &eqPrefs);

	if (NULL != dallasDriver && TRUE == dallasSpeakersConnected && TRUE == result) {
		// If the Dallas speakers are misinserted, set registry up for our MacBuddy buddies no matter what the output device is
		speakerConnectFailed = TRUE;
	} else {
		speakerConnectFailed = FALSE;
	}
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	if (NULL != dallasDriver && TRUE == result && kExternalSpeakersActive == deviceID) {
		// Only put up our alert if the Dallas speakers are the output device
		DisplaySpeakersNotFullyConnected (this, NULL);
	}

	debug6IOLog ("%d = GetCustomEQCoefficients (%lX, %lX, %lX, %p)\n", err, layoutID, deviceID, speakerID, eqPrefs);

	if (kIOReturnSuccess == err && NULL != eqPrefs) {
		DRCInfo				localDRC;

		//	Set the dynamic range compressor coefficients.
		localDRC.compressionRatioNumerator	= eqPrefs->drcCompressionRatioNumerator;
		localDRC.compressionRatioDenominator	= eqPrefs->drcCompressionRatioDenominator;
		localDRC.threshold					= eqPrefs->drcThreshold;
		localDRC.maximumVolume				= eqPrefs->drcMaximumVolume;
		localDRC.enable						= (Boolean)((UInt32)(eqPrefs->drcEnable));

		err = SndHWSetDRC ((DRCInfoPtr)&localDRC);

		err = SndHWSetOutputBiquadGroup (eqPrefs->filterCount, eqPrefs->filter[0].coefficient);
	} else {
		SetUnityGainAllPass ();
	}

	// Set the level controls to their (possibly) new min and max values
	if (drc.maximumVolume < 0) {
		minVolume = kMinimumVolume;
	} else {
		minVolume = kMinimumVolume + drc.maximumVolume;
	}
	maxVolume = kMaximumVolume + drc.maximumVolume;

	debug3IOLog ("DeviceInterruptService: minVolume = %ld, maxVolume = %ld\n", minVolume, maxVolume);
	AdjustControls ();

	debugIOLog ("- DeviceInterruptService\n");
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will set the global Boolean dontReleaseHPMute to 'true' if
//	the layout references a system where the hardware implements a headphone
//	mute circuit that deviates from the standard TAS3001 implementation and
//	requires a behavior that the Headphone Mute remain asserted when the 
//	headphone is muted.	 This is a deviation from the standard behavior where
//	the headphone mute is released after 250 milliseconds.	Just add new 
//	'case' for each system to be excluded from the default behavior above
//	the 'case layoutP29' with no 'break' and let the code fall through to
//	the 'case layoutP29' statement.	 Standard hardware implementations that
//	adhere to the default behavior do not require any code change.	[2660341]
void AppleTexasAudio::ExcludeHPMuteRelease (UInt32 layout) {
	switch (layout) {
		case layoutP29:		dontReleaseHPMute = true;			break;
		default:			dontReleaseHPMute = false;			break;
	}
}

UInt32 AppleTexasAudio::GetDeviceMatch (void) {
	UInt32			theDeviceMatch;

	if (TRUE == headphonesConnected)
		theDeviceMatch = kHeadphonesActive;				// headphones are connected
	else if (TRUE == dallasSpeakersConnected)
		theDeviceMatch = kExternalSpeakersActive;		// headphones aren't connected and external Dallas speakers are connected
	else
		theDeviceMatch = kInternalSpeakerActive;		// headphones aren't connected and external Dallas speakers aren't connected

	return theDeviceMatch;
}

IORegistryEntry *AppleTexasAudio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
								theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

IORegistryEntry *AppleTexasAudio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
	iterator = start->getChildIterator (gIODTPlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		tmpData = OSDynamicCast (OSData, tmpReg->getProperty (key));
		if (NULL != tmpData && tmpData->isEqualTo (value, strlen (value))) {
			theEntry = tmpReg;
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings) {
	IOReturn				err;
	Boolean					found;
	UInt32					index;
	EQPrefsElementPtr		eqElementPtr;

	debug5IOLog ("GetCustomEQCoefficients (%lX, %lX, %lX, %p)\n", layoutID, deviceID, speakerID, filterSettings);
	debug2IOLog ("gEQPrefs %p\n", gEQPrefs);

	err = -50;
	FailIf (0 == layoutID, Exit);
	FailIf (NULL == filterSettings, Exit);
	FailIf (NULL == gEQPrefs, Exit);

//	eqElementPtr = &(gEQPrefs->eq[0]);
//	FailIf (NULL == eqElementPtr, Exit);

	found = FALSE;
	eqElementPtr = NULL;
	*filterSettings = NULL;
	for (index = 0; index < gEQPrefs->eqCount && !found; index++) {
		eqElementPtr = &(gEQPrefs->eq[index]);
		debug2IOLog ("eqElementPtr %p\n", eqElementPtr);
		debug3IOLog ("index %ld, eqCount %ld\n", index, gEQPrefs->eqCount);
		debug3IOLog ("layoutID %lX, deviceID %lX, \n", eqElementPtr->layoutID, eqElementPtr->deviceID);
		debug2IOLog ("speakerID %lX\n", eqElementPtr->speakerID);

		if ((eqElementPtr->layoutID == layoutID) && (eqElementPtr->deviceID == deviceID)) {
			if (0 == speakerID) {
				found = TRUE;
			} else if (eqElementPtr->speakerID == speakerID) {
				found = TRUE;
			}
		}

//		if (FALSE == found) {
//			eqElementPtr = (EQPrefsElementPtr)((char *)(gEQPrefs->eq) + (sizeof (EQPrefsElement) - sizeof (EQFilterCoefficients)) + (eqElementPtr->filterCount * sizeof (EQFilterCoefficients)));
//		}
	}

	if (TRUE == found) {
		*filterSettings = eqElementPtr;
		err = kIOReturnSuccess;
	}

Exit:
	if (kIOReturnSuccess != err) {
		debug2IOLog ("err %d\n", err);
	} else {
		debug2IOLog ("filterSettings %p\n", filterSettings);
	}

	return err;
}

UInt32 AppleTexasAudio::GetDeviceID (void) {
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sa;
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	theDeviceID = 0;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	i2sa = i2s->childFromPath (ki2saEntry, gIODTPlane);
	FailIf (!i2sa, Exit);
	sound = i2sa->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kDeviceID));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32*)tmpData->getBytesNoCopy ();
	if (NULL != deviceID) {
		debug2IOLog ("deviceID = %ld\n", *deviceID);
		theDeviceID = *deviceID;
	} else {
		debugIOLog ("deviceID = NULL!\n");
	}

Exit:
	return theDeviceID;
}

Boolean AppleTexasAudio::HasInput (void) {
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sa;
	IORegistryEntry			*sound;
	OSData				*tmpData;
	UInt32				*numInputs;
	Boolean				hasInput;

	hasInput = false;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	i2sa = i2s->childFromPath (ki2saEntry, gIODTPlane);
	FailIf (!i2sa, Exit);
	sound = i2sa->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kNumInputs));
	FailIf (!tmpData, Exit);
	numInputs = (UInt32*)tmpData->getBytesNoCopy ();
	debug2IOLog ("numInputs = %ld\n", *numInputs);
	if (*numInputs > 1) {
		hasInput = true;
		debugIOLog ("Has input!\n");
	} else {
		debugIOLog ("Doesn't have input\n");
	}
Exit:
	return hasInput;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::SetActiveOutput (UInt32 output, Boolean touchBiquad) {
	IOReturn			err;
	
	debug3IOLog ("AppleTexasAudio::SndHWSetActiveOutput (output = %ld, %d)\n", output, touchBiquad);

	err = kIOReturnSuccess;
	if (touchBiquad)
		SetUnityGainAllPass ();
	switch (output) {
		case kSndHWOutputNone:
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			break;
		case kSndHWOutput1:
			SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	//	unmute
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			break;
		case kSndHWOutput2:																//	fall through to kSndHWOutput4
		case kSndHWOutput3:																//	fall through to kSndHWOutput4
		case kSndHWOutput4:
			//	The TA1101B amplifier can 'crowbar' when inserting the speaker jack.
			//	Muting the amplifier will release it from the crowbar state.
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		// mute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, NEGATE_GPIO (ampActiveState));		//	unmute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			if (!dontReleaseHPMute)													//	[2660341] unmute if std hw
				SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	// unmute
			break;
	}

	debug2IOLog ("AppleTexasAudio::SndHWSetActiveOutput err %d\n", err);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTexasAudio::SetBiquadInfoToUnityAllPass (void) {
	UInt32			index;
	
	debugIOLog ("SetBiquadInfoToUnityAllPass ()\n");
	for (index = 0; index < kNumberOfBiquadCoefficients; index++) {
		biquadGroupInfo[index++] = 1.0;				//	b0
		biquadGroupInfo[index++] = 0.0;				//	b1
		biquadGroupInfo[index++] = 0.0;				//	b2
		biquadGroupInfo[index++] = 0.0;				//	a1
		biquadGroupInfo[index] = 0.0;				//	a2
	}
	debugIOLog ("EXIT SetBiquadInfoToUnityAllPass ()\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will only restore unity gain all pass coefficients to the
//	biquad registers.  All other coefficients to be passed through exported
//	functions via the sound hardware plug-in manager libaray.
void AppleTexasAudio::SetUnityGainAllPass (void) {
	UInt32			prevLoadMode;
	int				biquadRefnum;
	DRCInfo			localDRC;
	
	//	save previous load mode
	prevLoadMode = 0 == (shadowRegs.sMCR[0] & (kFastLoad << kFL)) ? kSetNormalLoadMode : kSetFastLoadMode;
	debug3IOLog ("AppleTexasAudio::SetUnityGainAllPass (), shadowRegs.sMCR[0] %2X, prevLoadMode %ld\n", shadowRegs.sMCR[0], prevLoadMode);
	//	prepare for proper effect of fast load mode (i.e. unity all pass)
	if (kSetFastLoadMode == prevLoadMode)
		InitEQSerialMode (kSetNormalLoadMode, kRestoreOnNormal);
		
	//	force unity all pass biquad coefficients
	InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);
	//	Set the biquad coefficients in the shadow registers to 'unity all pass' so that
	//	any future attempt to set the biquads is applied to the hardware registers (i.e.
	//	make sure that the shadow register accurately reflects the current state so that
	//	 a data compare in the future does not cause a write operation to be bypassed).
	for (biquadRefnum = 0; biquadRefnum < kNumberOfBiquadsPerChannel; biquadRefnum++) {
		TAS3001C_WriteRegister (kLeftBiquad0CtrlReg	 + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
		TAS3001C_WriteRegister (kRightBiquad0CtrlReg + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
	}
	SetBiquadInfoToUnityAllPass ();
	InitEQSerialMode (kSetNormalLoadMode, kRestoreOnNormal);	//	go to normal load mode and restore registers after default
	
	//	Need to restore volume & mixer control registers after going to fast load mode
	localDRC.compressionRatioNumerator	= kDrcRatioNumerator;
	localDRC.compressionRatioDenominator	= kDrcRationDenominator;
	localDRC.threshold					= kDrcUnityThresholdHW;
	localDRC.maximumVolume				= kDefaultMaximumVolume;
	localDRC.enable						= false;

	SndHWSetDRC (&localDRC);

	//	restore previous load mode
	if (kSetFastLoadMode == prevLoadMode)
		InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);

	debug3IOLog ("shadowRegs.sMCR[0] %8X, prevLoadMode %ld\n", shadowRegs.sMCR[0], prevLoadMode);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	When disabling Dynamic Range Compression, don't check the other elements
//	of the DRCInfo structure.  When enabling DRC, clip the compression threshold
//	to a valid range for the target hardware & validate that the compression
//	ratio is supported by the target hardware.	The maximumVolume argument
//	will dynamically apply the zero index reference point into the volume
//	gain translation table and will force an update of the volume registers.
//	This SPI is primarily in support of the TAS MANIA application and is only
//	accessible through passing a private selector to the output component
//	SoundComponentSetInfo() function.
IOReturn AppleTexasAudio::SndHWSetDRC( DRCInfoPtr theDRCSettings ) {
	IOReturn		err;
	UInt8			regData[kDRCwidth];
	Boolean			enableUpdated;
	
	err = kIOReturnSuccess;
	enableUpdated = false;
	FailWithAction( NULL == theDRCSettings, err = -50, Exit );
	debug2IOLog( "SndHWSetDRC( theDRCSettings %p )\n", theDRCSettings );
	debug3IOLog( "compressionRatioNumerator %ld, compressionRatioDenominator %ld\n", theDRCSettings->compressionRatioNumerator, theDRCSettings->compressionRatioDenominator );
	debug3IOLog( "threshold %ld, maximumVolume %ld\n", theDRCSettings->threshold, theDRCSettings->maximumVolume );
	debug2IOLog( "enable %d\n", theDRCSettings->enable );
//	FailWithAction( kDrcRatioNumerator != theDRCSettings->compressionRatioNumerator, err = -50, Exit );
//	FailWithAction( kDrcRationDenominator != theDRCSettings->compressionRatioDenominator, err = -50, Exit );
//	FailWithAction( kTumblerAbsMaxVolume < theDRCSettings->maximumVolume, err = -50, Exit );
//	FailWithAction( kTumblerMinVolume > theDRCSettings->maximumVolume, err = -50, Exit );
	
	if( TRUE == theDRCSettings->enable ) {
		debugIOLog( "enable DRC\n" );
		//	Turn off the dynamic range compression and update the globals DRC enable state.
		//	The compression threshold is represented by a 4.4 number with a value of 15.0
		//	representing 0.0 dB and decrementing 0.0625 dB per count as the threshold is
		//	moved down toward -36.0625 dB.	Each count represents 0.375 dB.
		regData[0] = ( kDrcEnable << kEN ) | ( kCompression3to1 << kCR );
		// divide by 1000 to remove the 1000's that the constants were multiplied by
		regData[1] = (UInt8)( kDrcUnityThresholdHW - ((SInt32)( -theDRCSettings->threshold / kDrcThresholdStepSize ) / 1000) );
		err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, regData, kUPDATE_ALL );
		FailIf( kIOReturnSuccess != err, Exit );
		
		if( drc.enable != theDRCSettings->enable ) {
			enableUpdated = true;
		}

		drc.enable = theDRCSettings->enable;
		debug2IOLog( "drc.compressionRatioNumerator %ld\n", drc.compressionRatioNumerator );
		debug2IOLog( "drc.compressionRatioDenominator %ld\n", drc.compressionRatioDenominator );
		debug2IOLog( "drc.threshold %ld\n", drc.threshold );
		debug2IOLog( "drc.enable %d\n", drc.enable );
		
		//	The current volume setting needs to be scaled against the new range of volume 
		//	control and applied to the hardware.
		if( drc.maximumVolume != theDRCSettings->maximumVolume || enableUpdated ) {
			drc.maximumVolume = theDRCSettings->maximumVolume;
		}
	} else {
		debugIOLog( "disable DRC\n" );
		//	Turn off the dynamic range compression and update the globals DRC enable state
		err = TAS3001C_ReadRegister( kDynamicRangeCtrlReg, regData );
		FailIf( kIOReturnSuccess != err, Exit );
		regData[0] = ( kDrcDisable << kEN ) | ( kCompression3to1 << kCR );	//	[2580249,2667007] Dynamic range control = disabled at 3:1 compression
		regData[1] = kDefaultCompThld;										//	[2580249] Default threshold is 0.0 dB
		err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, regData, kUPDATE_ALL );
		FailIf( kIOReturnSuccess != err, Exit );
		drc.enable = false;
	}

	drc.compressionRatioNumerator		= theDRCSettings->compressionRatioNumerator;
	drc.compressionRatioDenominator		= theDRCSettings->compressionRatioDenominator;
	drc.threshold						= theDRCSettings->threshold;
	drc.maximumVolume					= theDRCSettings->maximumVolume;

Exit:
	if( kIOReturnSuccess != err ) {
		debug2IOLog( "SndHWSetDRC: err = %d\n", err );
	}

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This function does not utilize fast mode loading as to do so would
//	revert all biquad coefficients not addressed by this execution instance to
//	unity all pass.	 Expect DSP processing delay if this function is used.	It
//	is recommended that SndHWSetOutputBiquadGroup be used instead.
IOReturn AppleTexasAudio::SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients )
{
	IOReturn		err;
	UInt32			coefficientIndex;
	UInt32			tumblerBiquadIndex;
	UInt32			biquadGroupIndex;
//	FourDotTwenty	coefficients;
	UInt8			tumblerBiquad[kTumblerCoefficientsPerBiquad * kTumblerNumBiquads];
	
#ifdef	kBIQUAD_VERBOSE
//	debug3IOLog( "SndHWSetOutputBiquad( '%0.4s', %0.2d )\n", &streamID, biquadRefNum );
#endif
	err = kIOReturnSuccess;
	FailWithAction( kTumblerMaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );
	
	tumblerBiquadIndex = 0;
	biquadGroupIndex = biquadRefNum * kTumblerCoefficientsPerBiquad;
	if( kStreamFrontRight == streamID )
		biquadGroupIndex += kNumberOfBiquadCoefficientsPerChannel;
#ifdef	kEQ_VERBOSE
	debug3IOLog( "%0.4s %d : ", &streamID, biquadRefNum );
#endif
	for( coefficientIndex = 0; coefficientIndex < kTumblerCoefficientsPerBiquad; coefficientIndex++ )
	{
		// commented out because in this code biquadCoefficients is a double, not a FourDotTwenty
		// this saved the biquad info so that it could be read back later for verification (because you can't read the values from the hardware -- you have to remember what you wrote)
//		biquadGroupInfo[biquadGroupIndex] = biquadCoefficients[coefficientIndex];
//		if( kStreamStereo == streamID )
//			biquadGroupInfo[biquadGroupIndex + kNumberOfBiquadCoefficientsPerChannel] = biquadCoefficients[coefficientIndex];
//		biquadGroupIndex++;
		
#if kEQ_VERBOSE
//		if( 0.0 <= biquadCoefficients[coefficientIndex] )
//			IOLog( " +%3.10f ", biquadCoefficients[coefficientIndex] );
//		else
//			IOLog( " %3.10f ", biquadCoefficients[coefficientIndex] );
#endif
//		DoubleToFourDotTwenty( biquadCoefficients[coefficientIndex], &coefficients );
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].integerAndFraction1;
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].fraction2;
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].fraction3;
	}
	debugIOLog( "\n" );
	
	err = SetOutputBiquadCoefficients( streamID, biquadRefNum, tumblerBiquad );
Exit:
	if( kIOReturnSuccess != err )
		debug4IOLog( "err %d = SndHWSetOutputBiquad( '%4.4s', %ld )\n", err, (char*)&streamID, biquadRefNum );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients )
{
	UInt32			index;
	IOReturn		err;
	
	FailWithAction( 0 == biquadFilterCount || NULL == biquadCoefficients, err = -50, Exit );
//	debug3IOLog( "SndHWSetOutputBiquadGroup( %d, %2.6f )\n", biquadFilterCount, biquadCoefficients );
	err = kIOReturnSuccess;
	InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );
	index = 0;
	do {
		if( index >= ( biquadFilterCount / 2 ) ) {
			err = SndHWSetOutputBiquad( kStreamFrontRight, index - ( biquadFilterCount / 2 ), biquadCoefficients );
		} else {
			err = SndHWSetOutputBiquad( kStreamFrontLeft, index, biquadCoefficients );
		}
		index++;
		biquadCoefficients += kNumberOfCoefficientsPerBiquad;
	} while ( ( index < biquadFilterCount ) && ( kIOReturnSuccess == err ) );
	InitEQSerialMode( kSetNormalLoadMode, kRestoreOnNormal );
Exit:
	debug2IOLog( "err = %d\n", err );
	return err;
}

IOReturn AppleTexasAudio::SetOutputBiquadCoefficients( UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients )
{
	IOReturn			err;
	
	debug4IOLog ( "SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", (char*)&streamID, biquadRefNum, biquadCoefficients );
	err = kIOReturnSuccess;
	FailWithAction ( kTumblerMaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction ( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );

	switch ( biquadRefNum )
	{
		case kBiquadRefNum_0:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_1:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_2:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_3:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_4:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_5:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
	}

Exit:
	if( kIOReturnSuccess != err )
		debug5IOLog( "err %d = SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", err, (char*)&streamID, biquadRefNum, biquadCoefficients );
	return err;
}

#pragma mark +I2C FUNCTIONS
// Taken from PPCDACA.cpp

// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//		  returns the i2c port to use for the audio chip.
UInt32 AppleTexasAudio::getI2CPort()
{
	if(ourProvider) {
		OSData *t;
		
		t = OSDynamicCast(OSData, ourProvider->getProperty("AAPL,i2c-port-select"));	// we don't need a port select on Tangent, but look anyway
		if (t != NULL) {
			UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
			return myPort;
		}
//		else
//			debugIOLog( "AppleTexasAudio::getI2CPort missing property port, but that's not necessarily a problem\n");
	}

	return 0;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//		  opens and sets up the i2c bus
bool AppleTexasAudio::openI2C()
{
	FailIf (NULL == interface, Exit);

	// Open the interface and sets it in the wanted mode:
	FailIf (!interface->openI2CBus (getI2CPort()), Exit);
	interface->setStandardSubMode ();

	// have to turn on polling or it doesn't work...need to figure out why, but not today.
	interface->setPollingMode (true);

	return true;

Exit:
	return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//
// Purpose:
//		  closes the i2c bus
void AppleTexasAudio::closeI2C ()
{
	// Closes the bus so other can access to it:
	interface->closeI2CBus ();
}

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//	 Attaches to the i2c interface:
bool AppleTexasAudio::findAndAttachI2C(IOService *provider)
{
	const OSSymbol *i2cDriverName;
	IOService *i2cCandidate;

	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
	i2cCandidate = waitForService(resourceMatching(i2cDriverName));
	//interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
	interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

	if (interface == NULL) {
		debugIOLog("AppleTexasAudio::findAndAttachI2C can't find the i2c in the registry\n");
		return false;
	}

	// Make sure that we hold the interface:
	interface->retain();

	return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//	 detaches from the I2C
bool AppleTexasAudio::detachFromI2C(IOService* /*provider*/)
{
	if (interface) {
		//delete interface;
		interface->release();
		interface = 0;
	}
		
	return (true);
}

// Generic INLINEd methods to access to registers:
// ===============================================
inline UInt32 AppleTexasAudio::ReadWordLittleEndian(void *address, UInt32 offset)
{
#if 0
	UInt32 *realAddress = (UInt32*)(address) + offset;
	UInt32 value = *realAddress;
	UInt32 newValue =
		((value & 0x000000FF) << 16) |
		((value & 0x0000FF00) << 8) |
		((value & 0x00FF0000) >> 8) |
		((value & 0xFF000000) >> 16);

	return (newValue);
#else
	return OSReadLittleInt32(address, offset);
#endif
}

inline void AppleTexasAudio::WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value)
{
#if 0
	UInt32 *realAddress = (UInt32*)(address) + offset;
	UInt32 newValue =
		((value & 0x000000FF) << 16) |
		((value & 0x0000FF00) << 8) |
		((value & 0x00FF0000) >> 8) |
		((value & 0xFF000000) >> 16);

	*realAddress = newValue;
#else
	OSWriteLittleInt32(address, offset, value);
#endif	  
}

inline void AppleTexasAudio::I2SSetSerialFormatReg(UInt32 value)
{
	WriteWordLittleEndian(soundConfigSpace, kI2SSerialFormatOffset, value);
}

inline void AppleTexasAudio::I2SSetDataWordSizeReg(UInt32 value)
{
	WriteWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset, value);
}

inline UInt32 AppleTexasAudio::I2SGetDataWordSizeReg(void)
{
	return ReadWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset);
}

// Access to Keylargo registers:
inline void AppleTexasAudio::KLSetRegister(void *klRegister, UInt32 value)
{
	UInt32 *reg = (UInt32*)klRegister;
	*reg = value;
}

inline UInt32 AppleTexasAudio::KLGetRegister(void *klRegister)
{
	UInt32 *reg = (UInt32*)klRegister;
	return (*reg);
}

inline UInt32 AppleTexasAudio::I2SGetIntCtlReg()
{
	return ReadWordLittleEndian(soundConfigSpace, kI2SIntCtlOffset);
}

// --------------------------------------------------------------------------
// Method: setSampleRate
//
// Purpose:
//		  Sets the sample rate on the I2S bus
bool AppleTexasAudio::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio)
{
	UInt32	mclkRatio;
	UInt32	reqMClkRate;

	mclkRatio = mclkToFsRatio;			// remember the MClk ratio required

	if ( mclkRatio == 0 )																				   // or make one up if MClk not required
		mclkRatio = 64;				// use 64 x ratio since that will give us the best characteristics
	
	reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

	// look for a source clock that divides down evenly into the MClk
	if ((kClock18MHz % reqMClkRate) == 0) {
		// preferential source is 18 MHz
		clockSource = kClock18MHz;
	}
	else if ((kClock45MHz % reqMClkRate) == 0) {
		// next check 45 MHz clock
		clockSource = kClock45MHz;
	}
	else if ((kClock49MHz % reqMClkRate) == 0) {
		// last, try 49 Mhz clock
		clockSource = kClock49MHz;
	}
	else {
		debugIOLog("AppleTexasAudio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
		return false;
	}

	// get the MClk divisor
	debug3IOLog("AppleTexasAudio:setSampleParameters %ld / %ld =", (UInt32)clockSource, (UInt32)reqMClkRate); 
	mclkDivisor = clockSource / reqMClkRate;
	debug2IOLog("%ld\n", mclkDivisor);
	switch (serialFormat)					// SClk depends on format
	{
		case kSndIOFormatI2SSony:
		case kSndIOFormatI2S64x:
			sclkDivisor = mclkRatio / k64TicksPerFrame; // SClk divisor is MClk ratio/64
			break;
		case kSndIOFormatI2S32x:
			sclkDivisor = mclkRatio / k32TicksPerFrame; // SClk divisor is MClk ratio/32
			break;
		default:
			debugIOLog("AppleTexasAudio::setSampleParameters Invalid serial format\n");
			return false;
			break;
	}

	return true;
 }


// --------------------------------------------------------------------------
// Method: setSerialFormatRegister
//
// Purpose:
//		  Set global values to the serial format register
void AppleTexasAudio::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
	UInt32	regValue = 0;

	debug5IOLog("AppleTexasAudio::SetSerialFormatRegister(%d,%d,%d,%d)\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);

	switch ((int)clockSource)
	{
		case kClock18MHz:
			regValue = kClockSource18MHz;
			break;
		case kClock45MHz:
			regValue = kClockSource45MHz;
			break;
		case kClock49MHz:
			regValue = kClockSource49MHz;
			break;
		default:
			debug5IOLog("AppleTexasAudio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid clock source\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	switch (mclkDivisor)
	{
		case 1:
			regValue |= kMClkDivisor1;
			break;
		case 3:
			regValue |= kMClkDivisor3;
			break;
		case 5:
			regValue |= kMClkDivisor5;
			break;
		default:
			regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;
			break;
	}

	switch ((int)sclkDivisor)
	{
		case 1:
			regValue |= kSClkDivisor1;
			break;
		case 3:
			regValue |= kSClkDivisor3;
			break;
		default:
			regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;
			break;
	}
	regValue |= kSClkMaster;										// force master mode

	switch (serialFormat)
	{
		case kSndIOFormatI2SSony:
			regValue |= kSerialFormatSony;
			break;
		case kSndIOFormatI2S64x:
			regValue |= kSerialFormat64x;
			break;
		case kSndIOFormatI2S32x:
			regValue |= kSerialFormat32x;
			break;
		default:
			debug5IOLog("AppleTexasAudio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid serial format\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	// Set up the data word size register for stereo (input and output)
	I2SSetDataWordSizeReg (0x02000200);

	// Set up the serial format register
	I2SSetSerialFormatReg(i2sSerialFormat);
}

// --------------------------------------------------------------------------
// Method: setHWSampleRate
//
// Purpose:
//		  Gets the sample rate and makes it in a format that is compatible
//		  with the adac register. The funtion returns false if it fails.
bool AppleTexasAudio::setHWSampleRate(UInt rate)
{
	UInt32 dacRate = 0;

	debug2IOLog("AppleTexasAudio::setHWSampleRate(%d)\n", rate);

	switch (rate) {
		case 44100:					// 32 kHz - 48 kHz
			dacRate = kSRC_48SR_REG;
			break;

		default:
			debugIOLog("AppleTexasAudio::setHWSampleRate, supports only 44100 Hz (for now)\n");
			break;
	}

	// manipulate serial format register to change the mclock value for the new sample rate.
	return true;	// 44.1kHz is already set
}

// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//		  returns the frame rate as in the registry, if it is
//		  not found in the registry, it returns the default value.
#define kCommonFrameRate 44100

UInt32 AppleTexasAudio::frameRate(UInt32 index)
{
	if(ourProvider) {
		OSData *t;

		t = OSDynamicCast(OSData, ourProvider->getProperty("sample-rates"));
		if (t != NULL) {
			UInt32 *fArray = (UInt32*)(t->getBytesNoCopy());

			if ((fArray != NULL) && (index < fArray[0])){
				// I could do >> 16, but in this way the code is portable and
				// however any decent compiler will recognize this as a shift
				// and do the right thing.
				UInt32 fR = fArray[index + 1] / (UInt32)65536;

				debug2IOLog( "AppleTexasAudio::frameRate (%ld)\n",	fR);
				return fR;
			}
		}
	}

	return (UInt32)kCommonFrameRate;
}
