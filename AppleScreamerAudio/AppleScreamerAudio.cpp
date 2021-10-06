/*
 *  AudioScreamerAudio.cpp (definition)
 *  Project : AppleOnboardAudio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 * 
 *  The contents of this file constitute Original Code as defined in and
 *  are subject to the Apple Public Source License Version 1.1 (the
 *  "License").  You may not use this file except in compliance with the
 *  License.  Please obtain a copy of the License at
 *  http://www.apple.com/publicsource and read it before using this file.
 * 
 *  This Original Code and all software distributed under the License are
 *  distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AN APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Hardware independent (relatively) code for the Awacs Controller
 *  NEW-WORLD MACHINE ONLY!!!
 *
 *  As of the release of OS X version 1, the AppleScreamerAudio driver has gone
 *  under major changes. These changes adress only NewWorld machines (the translucent
 *  ones).
 *  
 *  This work was undertaken in order to use the information present in the Open Firmware
 *  "sound" node, and that represent the hardware wiring. We can then avoid the multiple
 *  cases for different machines, and have a driver that autoconfigures itself. Another
 *  goal is to isolate the Codec related function, from the rest of the driver. A superclass
 *  for all Apple Hardware driver can then be extracted and reused when new hardware
 *  is coming. 
 *    
 *  For commodity, all functions have been defined inside the driver class. This can 
 *  obviously done in a better way, by grouping the functionnality and creating 
 *  appropriate objects.
 *  
 *  The list of hardware access functions is not restrictive. It is only sufficient
 *  enough to answer to the behavior asked by the UI guidelines of OS X version 1.
 *  As long as the hardware support it, it is our intention to different 
 *  UI policies, and have a wider flexibility. 
 *
 *  PCMCIA card support is not supported for now 
 */
 
#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareDetect.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareInput.h"
#include "AudioHardwareMux.h"
#include "AudioHardwarePower.h"

#include "AppleScreamerAudio.h"
#include "awacs_hw.h"

#include "AppleDBDMAAudioDMAEngine.h"


static void 	Screamer_writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static void 	Screamer_writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value );
static UInt32   Screamer_ReadStatusRegisters( volatile awacs_regmap_t *ioBaseAwacs );
static int Screamer_readCodecControlReg(volatile awacs_regmap_t *ioBaseAwacs, int regNum);

// Not used at the moment, turned off just to kill warning

static int Screamer_readCodecControlReg(volatile awacs_regmap_t *ioBaseAwacs, int regNum)
{
    UInt32 reg, result;
    
    reg = (((regNum << kReadBackRegisterShift) & kReadBackRegisterMask) | kReadBackEnable) & kCodecCtlDataMask;
    reg |= kCodecCtlAddress7;
    reg |= kCodecCtlEMSelect0;
    
    OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, reg);
    eieio();
    
    do {
        result = OSReadLittleInt32(&ioBaseAwacs->CodecControlRegister, 0);
        eieio();
    } while (result & kCodecCtlBusy);
    
    // We're going to do this twice to make sure the results are back before reading the status register
    // What a pain - there must be a better way
    OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, reg);
    eieio();
    
    do {
        result = OSReadLittleInt32(&ioBaseAwacs->CodecControlRegister, 0);
        eieio();
    } while (result & kCodecCtlBusy);
    
    result = (OSReadLittleInt32(&ioBaseAwacs->CodecStatusRegister, 0) >> 4) & kCodecCtlDataMask;
    
    return result;
}


static void Screamer_writeCodecControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{
  int          CodecControlReg;

  //DEBUG_IOLOG( "PPCSound(awacs): CodecControlReg @ %08x = %08x\n", (int)&ioBaseAwacs->CodecControlRegister, value);

  OSWriteLittleInt32(&ioBaseAwacs->CodecControlRegister, 0, value );
  eieio();

  do
    {
      CodecControlReg =  OSReadLittleInt32( &ioBaseAwacs->CodecControlRegister, 0 );
      eieio();
    }
  while ( CodecControlReg & kCodecCtlBusy );
}


static void Screamer_writeSoundControlReg( volatile awacs_regmap_t *ioBaseAwacs, int value )
{
  //DEBUG_IOLOG( "PPCSound(awacs): SoundControlReg = %08x\n", value);

  OSWriteLittleInt32( &ioBaseAwacs->SoundControlRegister, 0, value );
  eieio();
}

static UInt32 Screamer_ReadStatusRegisters( volatile awacs_regmap_t *ioBaseAwacs )
{	

    //we need to have something that check if the Screamer is busy or in readback mode

   return OSReadLittleInt32( &ioBaseAwacs->CodecStatusRegister, 0 );
}

#define super AppleOnboardAudio

OSDefineMetaClassAndStructors(AppleScreamerAudio, AppleOnboardAudio)

        //Unix like prototypes 
        
bool AppleScreamerAudio::init(OSDictionary *properties)
{
    DEBUG_IOLOG("+ AppleScreamerAudio::init\n");
    if (!super::init(properties))
        return false;        
    chipInformation.awacsVersion = kAWACsAwacsRevision;
    
    mVolLeft = 0;
    mVolRight = 0;
    mIsMute = false;
    mVolMuteActive = false;

    DEBUG_IOLOG("- AppleScreamerAudio::init\n");
    return true;
}


void AppleScreamerAudio::free()
{
    DEBUG_IOLOG("+ AppleScreamerAudio::free\n");
    super::free();
    DEBUG_IOLOG("- AppleScreamerAudio::free\n");
}


IOService* AppleScreamerAudio::probe(IOService* provider, SInt32* score)
{
        // Finds the possible candidate for sound, to be used in
        // reading the caracteristics of this hardware:
    IORegistryEntry *sound = 0;
    DEBUG_IOLOG("+ AppleScreamerAudio::probe\n");
    
    super::probe(provider, score);
    *score = kIODefaultProbeScore;
    sound = provider->childFromPath("sound", gIODTPlane);
         //we are on a new world : the registry is assumed to be fixed
    if(sound) {
        OSData *tmpData;
        
        tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
        if(tmpData) {
            if(tmpData->isEqualTo(kAWACsModelName, sizeof(kAWACsModelName) -1) ||
               tmpData->isEqualTo(kScreamerModelName, sizeof(kScreamerModelName) -1) ) {
                *score = *score+1;
                DEBUG_IOLOG("- AppleScreamerAudio::probe\n");
                return(this);
            } 
        } 
    } 
    DEBUG_IOLOG("- AppleScreamerAudio::probe\n");
    return (0);
}


bool AppleScreamerAudio::initHardware(IOService *provider)
{
    AbsoluteTime		timerInterval;
    bool myreturn = true;

    DEBUG_IOLOG("+ AppleScreamerAudio::initHardware\n");
    

    
    super::initHardware(provider);
            //Common information
       
   
    codecStatus &= ~kAllSense;
    gCanPollSatus = true;    
    checkStatus(true);
   
            //create and flush ports, controls
    
    // Use the current hardware settings as our defaults
   /* if (chipInformation.outputAActive) {
        int regValue;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsOutputAAttenReg);
        
        mVolLeft = (15 - ((regValue & kAWACsOutputLeftAtten) << kAWACsOutputLeftShift)) * 4096;
        mVolRight = (15 - (regValue & kAWACsOutputRightAtten)) * 4096;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsMuteReg);
        
        deviceMuted = (regValue & kAWACsMuteOutputA) == kAWACsMuteOutputA;
    } else if (chipInformation.outputCActive) {
        int regValue;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsOutputCAttenReg);
        
        mVolLeft = (15 - ((regValue & kAWACsOutputLeftAtten) << kAWACsOutputLeftShift)) * 4096;
        mVolRight = (15 - (regValue & kAWACsOutputRightAtten)) * 4096;
        
        regValue = Screamer_readCodecControlReg(ioBase, kAWACsMuteReg);
        
        deviceMuted = (regValue & kAWACsMuteOutputC) == kAWACsMuteOutputC;
    } else {
        deviceMuted = true;
    }
    
    if (outVolLeft) {
        outVolLeft->setValue(mVolLeft);
    }
    
    if (outVolRight) {
        outVolRight->setValue(mVolRight);
    }
    
    if (outMute) {
        outMute->setValue(deviceMuted ? 1 : 0);
    } */
    
    
//    flushAudioControls();
    
        //Prepare the timer loop --> should go on the workloop
    nanoseconds_to_absolutetime(NSEC_PER_SEC, &timerInterval);
    addTimerEvent(this, &AppleScreamerAudio::timerCallback, timerInterval);
    
    duringInitialization = false;
    DEBUG_IOLOG("- AppleScreamerAudio::initHardware\n");
    return myreturn;
}

void AppleScreamerAudio::setDeviceDetectionActive(){
    gCanPollSatus = true;
}
    
void AppleScreamerAudio::setDeviceDetectionInActive(){
    gCanPollSatus = false;
}

        //IOAudio subclasses
void AppleScreamerAudio::sndHWInitialize(IOService *provider)
{
    IOMemoryMap *map;
    
    DEBUG_IOLOG("+ AppleScreamerAudio::sndHWInitialize\n");
    map = provider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
    
    ioBase = (awacs_regmap_t *)map->getVirtualAddress();
    
    codecStatus = Screamer_ReadStatusRegisters( ioBase );
            
            //fill the chip info
    chipInformation.partType = sndHWGetType();
    chipInformation.awacsVersion = (codecStatus & kAWACsRevisionNumberMask) >> kAWACsRevisionShift;
    chipInformation.preRecalDelay = kPreRecalDelayCrystal;      // assume Crystal for recalibrate (safest)
    chipInformation.rampDelay = kRampDelayNational;		// assume National for ramping (safest)
    
    soundControlRegister = ( kInSubFrame0  |kOutSubFrame0 | kHWRate44100);
    Screamer_writeSoundControlReg( ioBase, soundControlRegister);

    codecControlRegister[0] = kCodecCtlAddress0 | kCodecCtlEMSelect0;
    codecControlRegister[1] = kCodecCtlAddress1 | kCodecCtlEMSelect0;
    codecControlRegister[2] = kCodecCtlAddress2 | kCodecCtlEMSelect0;
    codecControlRegister[4] = kCodecCtlAddress4 | kCodecCtlEMSelect0;
    
    Screamer_writeCodecControlReg(  ioBase, codecControlRegister[0] );
    Screamer_writeCodecControlReg(  ioBase, codecControlRegister[1] );
    Screamer_writeCodecControlReg(  ioBase, codecControlRegister[2] );
    Screamer_writeCodecControlReg(  ioBase, codecControlRegister[4] );
    
    if ( chipInformation.awacsVersion > kAWACsAwacsRevision ) {
        codecControlRegister[5] = kCodecCtlAddress5 | kCodecCtlEMSelect0;
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[5] );
        codecControlRegister[6] = kCodecCtlAddress6 | kCodecCtlEMSelect0;
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[6] );
    }

    switch (sndHWGetManufacturer()){
        case kSndHWManfCrystal :
            chipInformation.preRecalDelay = kPreRecalDelayCrystal;
            chipInformation.rampDelay = kRampDelayCrystal;
            break;
        case kSndHWManfNational :
            chipInformation.preRecalDelay = kPreRecalDelayNational;
            chipInformation.rampDelay = kRampDelayNational;
            break;
        default :
            break;
    }
    
            //do the IO init
   

        //These line should go into the IO part
    
        //this means we assume an mic input. We should assume through the 
        //input objects
    codecControlRegister[0] |= (kUnusedInput | kDefaultMicGain);

    
   
        //we should add the Screamer info
    DEBUG_IOLOG("- AppleScreamerAudio::sndHWInitialize\n");
}

    

/****************************** Workloop functions ***************************/
/*   There is work to do there !!!!  					     */
/*   In fact we should put that on the work loop, and have a family method   */
/*   to add whatever we want on it (polling, interrupt based stuff...)       */
/*   There is also to improve into the communication with any input 	     */
/*   and output. Instead of calling the arrays, we should send notification  */
/*   trough the IOAudioJackControl inheritance of the AudioHardwareDetect,   */
/*   and have all ports, listen to these notification (maybe it would have   */
/*   easier in Objective-C... or the IOKit does it for us)		     */
/*****************************************************************************/


void AppleScreamerAudio::checkStatus(bool force) {

    UInt32 newCodecStatus, inSense, extdevices;
    AudioHardwareDetect *theDetect;
    OSArray *AudioDetects;
        
//    DEBUG_IOLOG("+ AppleScreamerAudio::checkStatus\n");        
    if(false == gCanPollSatus)
        return;
    
    newCodecStatus = Screamer_ReadStatusRegisters(ioBase);

    if (((codecStatus & kAWACsStatusInSenseMask) != (newCodecStatus &kAWACsStatusInSenseMask)) || force)   {
        UInt32 i;

        inSense = 0;
        codecStatus = newCodecStatus;
        extdevices = 0;
        inSense = sndHWGetInSenseBits();
            
        AudioDetects = super::getDetectArray();
        if(AudioDetects) {
            for(i = 0; i < AudioDetects->getCount(); i++) {
                theDetect = OSDynamicCast(AudioHardwareDetect, AudioDetects->getObject(i));
                if (theDetect) extdevices |= theDetect->refreshDevices(inSense);
            }
            super::setCurrentDevices(extdevices);
        } else {
            DEBUG_IOLOG("I didn;t get the array\n");
        }
        
           }
  //  DEBUG_IOLOG("- AppleScreamerAudio::checkStatus\n");
}


void AppleScreamerAudio::timerCallback(OSObject *target, IOAudioDevice *device) {
    AppleScreamerAudio *screamer;

//    DEBUG_IOLOG("+ AppleScreamerAudio::timerCallback\n");
    screamer = OSDynamicCast(AppleScreamerAudio, target);
    if (screamer) 
        screamer->checkStatus(false);
//    DEBUG_IOLOG("- AppleScreamerAudio::timerCallback\n");
}




/*************************** sndHWXXXX functions******************************/
/*   These functions should be common to all Apple hardware drivers and be   */
/*   declared as virtual in the superclass. This is the only place where we  */
/*   should manipulate the hardware. Order is given by the UI policy system  */
/*   The set of function should be enought to implement the policy           */
/*****************************************************************************/


/************************** Hardware Register Manipulation ********************/

UInt32 AppleScreamerAudio::sndHWGetInSenseBits(){
    UInt32 newCodecStatus, status, inSense;
    
    inSense = 0;
    newCodecStatus = Screamer_ReadStatusRegisters(ioBase);
    newCodecStatus &= kAWACsStatusMask;
    status = newCodecStatus & kAWACsInSenseMask;
    
            //something is screwed up in the order of bytes
    if(status & kAWACsInSense0) 
        inSense |= kAWACsInSense3;
    if(status & kAWACsInSense1)
        inSense |= kAWACsInSense2;
    if(status & kAWACsInSense2)
        inSense |= kAWACsInSense1;
    if(status & kAWACsInSense3) 
        inSense |= kAWACsInSense0;

    return(inSense);
}


UInt32 AppleScreamerAudio::sndHWGetRegister(UInt32 regNum){
    return(codecControlRegister[regNum]);
}


IOReturn AppleScreamerAudio::sndHWSetRegister(UInt32 regNum, UInt32 value){
    IOReturn myReturn = kIOReturnSuccess;
    
    codecControlRegister[regNum] = value;
    Screamer_writeCodecControlReg(ioBase, codecControlRegister[regNum]);
    return(myReturn);
}

UInt32	AppleScreamerAudio::sndHWGetConnectedDevices(void) {
    return(currentDevices);
}


/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32 AppleScreamerAudio::sndHWGetActiveOutputExclusive(void){
    UInt32 result;
    
    if(!(chipInformation.outputAActive) && !(chipInformation.outputCActive))
        result = kSndHWOutputNone;
    else if(chipInformation.outputAActive)
        result = kSndHWOutput1;
    else
        result = kSndHWOutput2;

    return(result);
}
    
IOReturn   AppleScreamerAudio::sndHWSetActiveOutputExclusive(UInt32 outputPort ){
    IOReturn myReturn = kIOReturnSuccess;
    
    switch (outputPort){
        case kSndHWOutputNone:
            chipInformation.outputAActive = false;
            chipInformation.outputCActive = false;
            break;
        case kSndHWOutput1:
            DEBUG_IOLOG("+ output A is active \n");
            chipInformation.outputAActive = true;
            chipInformation.outputCActive = false;
            break;
        case kSndHWOutput2:
            DEBUG_IOLOG("+ output C is active \n");
            chipInformation.outputAActive = false;
            chipInformation.outputCActive = true;
            break;
        default:
            myReturn = false;
            break;
    }		
    sndHWSetSystemMute(mIsMute);
    return (myReturn);
}

UInt32 	 AppleScreamerAudio::sndHWGetActiveInputExclusive(void){
    UInt32		input;
    UInt32		inputReg;
    UInt32		pcmciaReg;
		
    input = kSndHWInputNone;
	
    inputReg = sndHWGetRegister(kAWACsInputReg) & kAWACsInputField;
    pcmciaReg = sndHWGetRegister(kAWACsPCMCIAAttenReg) & kAWACsPCMCIAAttenField;

    switch (inputReg){
        case kAWACsInputA:
            input = kSndHWInput1;
            break;
        case kAWACsInputB:
            input = kSndHWInput2;
            break;
        case kAWACsInputC:
            input = kSndHWInput3;
            break;
        default:
            if (pcmciaReg == kAWACsPCMCIAOn)
                input = kSndHWInput4;
            else if (inputReg != 0)
                DEBUG_IOLOG("Invalid input setting\n");
            break;
    }
	
    return (input);
}

IOReturn AppleScreamerAudio::sndHWSetActiveInputExclusive(UInt32 input ){
    
    IOReturn result = kIOReturnSuccess; 
    
    UInt32		inputReg;
    UInt32		pcmciaReg;
    Boolean		needsRecalibrate;
	
    needsRecalibrate = (input != sndHWGetActiveInputExclusive());
        // start with all inputs off
    inputReg = sndHWGetRegister(kAWACsInputReg) & ~kAWACsInputField;
    pcmciaReg = sndHWGetRegister(kAWACsPCMCIAAttenReg) & ~kAWACsPCMCIAAttenField;
    	
    switch (input){
        case kSndHWInputNone:
            break;
        case kSndHWInput1:
            inputReg |= kAWACsInputA;	
            break;
        case kSndHWInput2:
            inputReg |= kAWACsInputB;
            break;
        case kSndHWInput3:
            inputReg |= kAWACsInputC;
            break;
        case kSndHWInput4:
            pcmciaReg |= kAWACsPCMCIAOn;
            needsRecalibrate = false;
            break;
        default:
            result = kIOReturnError;
            goto EXIT;
            break;
    }
	
        //this should disappear. We put the gain input to the max value
    
//    gainReg = sndHWGetRegister(kAWACsGainReg) & ~kAWACsGainField;		// get and clear current gain setting

//   gainReg |= ((kAWACsMaxHardwareGain << kAWACsGainLeftShift) & kAWACsGainLeft);
//    gainReg |= (kAWACsMaxHardwareGain & kAWACsGainRight);
//    sndHWSetRegister(kAWACsGainReg, gainReg);

    sndHWSetRegister(kAWACsInputReg, inputReg);
	
 if (needsRecalibrate) 
        GC_Recalibrate();
      
	
EXIT:
    return(result);
}


UInt32  AppleScreamerAudio::sndHWGetProgOutput(void ){
    return (sndHWGetRegister(kAWACsProgOutputReg) & kAWACsProgOutputField) >> kAWACsProgOutputShift;
}


IOReturn   AppleScreamerAudio::sndHWSetProgOutput(UInt32 outputBits){
    UInt32	progOutputReg;
    IOReturn result; 
	
    result = kIOReturnError;
    FAIL_IF((outputBits & (kSndHWProgOutput0 | kSndHWProgOutput1)) != outputBits, EXIT);
    
    result = kIOReturnSuccess;
    progOutputReg = sndHWGetRegister(kAWACsProgOutputReg) & ~kAWACsProgOutputField;
    progOutputReg |= ((outputBits << kAWACsProgOutputShift) & kAWACsProgOutputField);
    sndHWSetRegister(kAWACsProgOutputReg, progOutputReg);
	
EXIT:
    return result;
}


/************************** Global (i.e all outputs) manipulation of mute and volume ***************/

bool   AppleScreamerAudio::sndHWGetSystemMute(void){
    return (mIsMute);
}
 
IOReturn   AppleScreamerAudio::sndHWSetSystemMute(bool mutestate){
    UInt32	muteReg;
    
    muteReg = sndHWGetRegister(kAWACsMuteReg) & ~kAWACsMuteField;
    if (mutestate || ((mVolRight == mVolLeft) && (0 == mVolRight))) {	
        muteReg |= (kAWACsMuteOutputA | kAWACsMuteOutputC);	
        mIsMute = true;				
    } else {
        if (chipInformation.outputAActive) {
            muteReg &= ~kAWACsMuteOutputA;
        } else {
            muteReg |= kAWACsMuteOutputA;
        }
        if (chipInformation.outputCActive) {
            muteReg &= ~kAWACsMuteOutputC;
        } else {
            muteReg |= kAWACsMuteOutputC;
        }
        mIsMute = false;
    }
    sndHWSetRegister(kAWACsMuteReg, muteReg);
    return(kIOReturnSuccess);
}

IOReturn   AppleScreamerAudio::sndHWSetSystemVolume(UInt32 value){
    sndHWSetSystemVolume( value,  value);
    return kIOReturnSuccess;
}

bool AppleScreamerAudio::sndHWSetSystemVolume(UInt32 leftvalue, UInt32 rightvalue) {

        //This function is not very flexible. It sets the volume for 
        //each output port to the same level. This is obvioulsy not
        //very flexible, but we keep that for the UI Policy implementation

    bool hasChanged = false;
    UInt32 leftAttn, rightAttn;
    
    if((leftvalue != mVolLeft) || duringInitialization) {

        mVolLeft = leftvalue;		
        if(0 == leftvalue) leftvalue = 1;
        leftvalue -=1;
        leftAttn = 15-leftvalue;
         
            //we change the left value for each register
        codecControlRegister[kAWACsOutputAAttenReg] 
                = (codecControlRegister[kAWACsOutputAAttenReg] & ~kAWACsOutputLeftAtten) |
                            (leftAttn << kAWACsOutputLeftShift);
        codecControlRegister[kAWACsOutputCAttenReg] 
                = (codecControlRegister[kAWACsOutputCAttenReg] & ~kAWACsOutputLeftAtten) |
                            (leftAttn << kAWACsOutputLeftShift);
        hasChanged = true;
    }
    
    if((rightvalue != mVolRight) || duringInitialization){
        mVolRight = rightvalue;
        if(0 == rightvalue) rightvalue = 1;
        rightvalue -=1;
        rightAttn = 15-rightvalue;
        
            //we change the right value for each register
        codecControlRegister[kAWACsOutputAAttenReg] 
                = (codecControlRegister[kAWACsOutputAAttenReg] & ~kAWACsOutputRightAtten) |
                            (rightAttn);
        codecControlRegister[kAWACsOutputCAttenReg] 
                = (codecControlRegister[kAWACsOutputCAttenReg] & ~kAWACsOutputRightAtten) |
                            (rightAttn);
        hasChanged = true;
    }
    
    if(hasChanged) {
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[kAWACsOutputAAttenReg] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[kAWACsOutputCAttenReg] );
    }
    
    if((rightvalue == leftvalue) && (0 == rightvalue)) {
        mVolMuteActive = true;
        sndHWSetSystemMute(true);
        //We set the mute volume
    } else {
        if(mVolMuteActive) {
            mVolMuteActive = false;
            sndHWSetSystemMute(false);
        }
    }
    
    return(true);
}


IOReturn AppleScreamerAudio::sndHWSetPlayThrough( bool playthroughState )
{
	UInt32	playthruReg;
	IOReturn result = kIOReturnSuccess; 
                	
	playthruReg = sndHWGetRegister(kAWACsLoopThruReg) & ~kAWACsLoopThruEnable;
	if (playthroughState) {
            playthruReg |= kAWACsLoopThruEnable;
        }
	sndHWSetRegister(kAWACsLoopThruReg, playthruReg);

	return result;
}


/************************** Identification of the codec ************************/

UInt32 AppleScreamerAudio::sndHWGetType( void ) {
	UInt32		revision, info;
	UInt32		codecStatus;
	
        codecStatus = Screamer_ReadStatusRegisters( ioBase );
        revision = (codecStatus & kAWACsRevisionNumberMask) >> kAWACsRevisionShift;
        
	switch (revision) {
            case kAWACsAwacsRevision :
                info = kSndHWTypeAWACs;
                break;
            case kAWACsScreamerRevision :
                info = kSndHWTypeScreamer;
                break;
            default :
                info = kSndHWTypeUnknown;
                break;
	}		
        return info;
}

IOReturn   AppleScreamerAudio::sndHWSetPowerState(IOAudioDevicePowerState theState){

    if (chipInformation.partType == kSndHWTypeAWACs)
        setAWACsPowerState(theState);
    else
        setScreamerPowerState(theState);	
        
    return(kIOReturnSuccess);
}

UInt32 AppleScreamerAudio::sndHWGetManufacturer(void) {
    UInt32		codecStatus, info = 0;
    
    codecStatus = Screamer_ReadStatusRegisters( ioBase );
	
    switch (codecStatus & kAWACsManufacturerIDMask){
        case kAWACsManfCrystal:
            DEBUG_IOLOG("Crystal Manufacturer\n");
            info = kSndHWManfCrystal;
            break;
        case kAWACsManfNational:
            DEBUG_IOLOG("National Manufacturer\n");
            info = kSndHWManfNational;
            break;
        case kAWACsManfTI:
            DEBUG_IOLOG("TI Manufacturer\n");
            info = kSndHWManfTI;
            break;
        default:
            DEBUG_IOLOG("Unknown Manufacturer\n");
            info = kSndHWManfUnknown;
            break;
    }
    return info;
}


IOReturn AppleScreamerAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain){
    IOReturn myReturn = kIOReturnSuccess; 
    UInt32 gainReg;
    UInt8 galeft, garight;

    DEBUG3_IOLOG("+ AppleScreamerAudio::sndHWSetSystemInputGain (%ld, %ld)\n", leftGain, rightGain);
    galeft = (UInt8) leftGain;
    garight = (UInt8) rightGain;
	
    gainReg = sndHWGetRegister(kAWACsGainReg) & ~kAWACsGainField;		// get and clear current gain setting

    gainReg |= ((galeft << kAWACsGainLeftShift) & kAWACsGainLeft);
    gainReg |= (garight & kAWACsGainRight);
    sndHWSetRegister(kAWACsGainReg, gainReg);
    
    DEBUG_IOLOG("- AppleScreamerAudio::sndHWSetSystemInputGain\n");
    return(myReturn);
}

#pragma mark ++++++++ UTILITIES

/************************** Utilities for AWACS/Screamer only ************************/

void AppleScreamerAudio::GC_Recalibrate()
{
    UInt32 awacsReg, saveReg;
    
    DEBUG_IOLOG("+ AppleScreamerAudio::GC_Recalibrate\n");

    IOSleep( kPreRecalDelayCrystal); //This recalibrate delay is a hack for some
                                     //broken Crystal parts
        
    awacsReg = sndHWGetRegister(kAWACsRecalibrateReg);
    saveReg = awacsReg;
    
    awacsReg |= (kMuteInternalSpeaker | kMuteHeadphone); //mute the outputs
    awacsReg |= kAWACsRecalibrate;			 //set the recalibrate bits
    
    sndHWSetRegister(kAWACsRecalibrateReg, awacsReg);
    IOSleep(1000);//kRecalibrateDelay*2);//There seems to be some confusion on the time we have to wait
                                         //No doc indicates it clearly. This value was indicated in 
                                         //the OS 9 code
    sndHWSetRegister(kAWACsRecalibrateReg, saveReg);
    DEBUG_IOLOG("- AppleScreamerAudio::GC_Recalibrate\n");
}

void AppleScreamerAudio::restoreSndHWRegisters( void )
{
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[0] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[1] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[2] );
        Screamer_writeCodecControlReg(  ioBase, codecControlRegister[4] );
        if ( chipInformation.awacsVersion > kAWACsAwacsRevision ) {
            Screamer_writeCodecControlReg(  ioBase, codecControlRegister[5] );
            Screamer_writeCodecControlReg(  ioBase, codecControlRegister[6] );
        }
}


void AppleScreamerAudio::setAWACsPowerState( IOAudioDevicePowerState state )
{
    
    switch (state){
        case kIOAudioDeviceActive : 
            restoreSndHWRegisters();
            GC_Recalibrate();
            break;
        case kIOAudioDeviceSleep :
            break;
        default:
            break;
    }
}

void AppleScreamerAudio::setScreamerPowerState(IOAudioDevicePowerState state) {
    setAWACsPowerState(state);  //we should do better !!!
}


void AppleScreamerAudio::InitializeShadowRegisters(void){
    UInt32 regNumber;
    
    switch(chipInformation.partType) {
        case kSndHWTypeScreamer:
            codecControlRegister[kMaxSndHWRegisters] = 0;
            for (regNumber = 0; regNumber < kMaxSndHWRegisters-1; regNumber++)
                codecControlRegister[regNumber] = Screamer_readCodecControlReg(ioBase,regNumber);;
            break;
        case kSndHWTypeAWACs:
            for (regNumber = 0; regNumber < kMaxSndHWRegisters; regNumber++)
                    codecControlRegister[regNumber] = 0;
            break;
    }

}
