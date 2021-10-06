/*
 *  AudioI2SControl.h
 *  AppleOnboardAudio
 *
 *  Created by nthompso on Fri Jul 13 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
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
 * This file contains a class for dealing with i2s for audio drivers.
 */


#ifndef _AUDIOI2SCONTROL_H
#define _AUDIOI2SCONTROL_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"
#include "AppleOnboardAudio.h"


// In debug mode we may wish to step trough the INLINEd methods, so:
#ifdef DEBUGMODE
#define INLINE
#else
#define INLINE	inline
#endif



// Sound Formats:
typedef enum SoundFormat 
{
    kSndIOFormatI2SSony,
    kSndIOFormatI2S64x,
    kSndIOFormatI2S32x,

    // This says "we never decided for a sound format before"
    kSndIOFormatUnknown
} SoundFormat;

// Characteristic constants:
typedef enum TicksPerFrame 
{
    k64TicksPerFrame		= 64,			// 64 ticks per frame
    k32TicksPerFrame		= 32 			// 32 ticks per frame
} TicksPerFrame;

typedef enum ClockSource 
{
        kClock49MHz				= 49152000,		// 49 MHz clock source
        kClock45MHz				= 45158400,		// 45 MHz clock source
        kClock18MHz				= 18432000		 // 18 MHz clock source
} ClockSource;

// this struct type is used as a param block for passing info about the i2s object
// being created into the create and init methods.
typedef struct _s_AudioI2SInfo 
{
    SoundFormat i2sSerialFormat;
    IOMemoryMap *map ;
} AudioI2SInfo ;


// AudioI2SControl is essentially a class for setting the state of the I2S registers
class AudioI2SControl : public OSObject 
{
    OSDeclareDefaultStructors(AudioI2SControl);
private:
    // holds the current frame rate settings:
    ClockSource dacaClockSource;
    UInt32      dacaMclkDivisor;
    UInt32      dacaSclkDivisor;
    SoundFormat dacaSerialFormat;

    bool setSampleParameters( UInt32 sampleRate, UInt32 mclkToFsRatio) ;
    void setSerialFormatRegister( ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat) ;

    bool dependentSetup(void) ;

    UInt32 ReadWordLittleEndian(void *address,UInt32 offset);
    void WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);

    void *soundConfigSpace;        // address of sound config space
    void *ioBaseAddress;           // base address of our I/O controller
    void *ioClockBaseAddress;	   // base address for the clock
    void *ioStatusRegister_GPIO12; // the register with the input detection

    // Recalls which i2s interface we are attached to:
    UInt8 i2SInterfaceNumber;
    
public:

    // Access to all the I2S registers:
    // -------------------------------
    UInt32 I2SGetIntCtlReg();
    void   I2SSetIntCtlReg(UInt32 value);
    UInt32 I2SGetSerialFormatReg();
    void   I2SSetSerialFormatReg(UInt32 value);
    UInt32 I2SGetCodecMsgOutReg();
    void   I2SSetCodecMsgOutReg(UInt32 value);
    UInt32 I2SGetCodecMsgInReg();
    void   I2SSetCodecMsgInReg(UInt32 value);
    UInt32 I2SGetFrameCountReg();
    void   I2SSetFrameCountReg(UInt32 value);
    UInt32 I2SGetFrameMatchReg();
    void   I2SSetFrameMatchReg(UInt32 value);
    UInt32 I2SGetDataWordSizesReg();
    void   I2SSetDataWordSizesReg(UInt32 value);
    UInt32 I2SGetPeakLevelSelReg();
    void   I2SSetPeakLevelSelReg(UInt32 value);
    UInt32 I2SGetPeakLevelIn0Reg();
    void   I2SSetPeakLevelIn0Reg(UInt32 value);
    UInt32 I2SGetPeakLevelIn1Reg();
    void   I2SSetPeakLevelIn1Reg(UInt32 value);
    UInt32 I2SCounterReg();

    // starts and stops the clock count:
    void   KLSetRegister(void *klRegister, UInt32 value);
    UInt32   KLGetRegister(void *klRegister);
    
    static AudioI2SControl *create(AudioI2SInfo *theInfo) ;
    inline void *getIOStatusRegister_GPIO12(void) { return (ioStatusRegister_GPIO12); } ;

protected:
    bool init(AudioI2SInfo *theInfo) ;
    void free(void) ;
    bool clockRun(bool start) ;    
    UInt32 frameRate(UInt32 index) ;
} ;

#endif



