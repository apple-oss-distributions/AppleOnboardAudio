/*
 *  AudioHardwareConstants.h
 *  AppleOnboardAudio
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
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Contains a lot of constants used across the AppleOnboardAudio project.
 *  There are three kind of constants : 
 *      - the string equivalent used mainly to parse the Open Firmware 
 *        device tree. Eventually they will move to the header file
 *        of a OF device tree parser objects.
 *      - A serie of enum representing the abstracted input, output numbers
 *        for any chip. This is enough to cover hardware Codec with 6 or 7
 *        input and outputs, which we will never have...
 *      - A enumeration for device kinds. We may extend it for Apple Speakers
 *        All devices are bit exclusives
 *      - An enumeration for all kind codec.
 */

#ifndef __AUDIOHARDWARECONSTANT__
#define __AUDIOHARDWARECONSTANT__

#define kSoundEntryName	"sound"

// Sound Entry
#define kSoundObjectsPropName		"sound-objects"

#define kInputObjEntryName			"input"
#define kOutputObjEntryName			"output"
#define kDetectObjEntryName			"detect"
#define kFeatureObjEntryName		"feature"
#define kInitObjEntryName			"init"
#define kMuxObjEntryName			"mux"
        
#define kNumDetectsPropName			"#-detects"
#define kNumFeaturesPropName		"#-features"
#define kNumOutputsPropName			"#-outputs"
#define kNumInputsPropName			"#-inputs"
#define kModelPropName				"model"

#define kAnyInDetectObjName			"AnyInSenseBitsDetect"
#define kInSenseBitsDetectObjName	"InSenseBitsDetect"	
#define kGPIODetectObjName			"GPIODetect"
#define kGPIOGenericDetectObjName	"GPIOGenericDetect"

#define kBitMaskPropName			"bit-mask"
#define kBitMatchPropName			"bit-match"
#define kDevicePropName				"device"
#define kDeviceIDPropName			"device-id"
#define kDeviceMaskPropName			"device-mask"
#define kDeviceMatchPropName		"device-match"
#define kDeviceTypePropName			"device_type"
#define kIconIDPropName         	"icon-id"
#define kPortChannelsPropName   	"port-channels"
#define kPortConnectionPropName 	"port-connection"
#define kPortTypePropName       	"port-type"
#define kNameIDPropName         	"name-id"

#define kOutputPortObjName			"OutputPort"
#define kOutputEQPortObjName		"OutputEQPort"
#define kOutputDallasEQPortObjName	"OutputDallasEQPort"
#define	kOutputMonoEQPortObjName	"OutputMonoEQPort"
#define kGazIntSpeakerObjName		"Proj1Speaker"
#define kGazSubwooferObjName		"Proj2Speaker"
#define kWSIntSpeakerObjName		"Proj3Speaker"
#define kGossSGSToneOutObjName		"Proj4Speaker"
#define kKiheiSpeakerObjName		"Proj5Speaker"

#define kAWACsModelName				"343S0140"
#define kScreamerModelName			"343S0184"
#define kBurgundyModelName			"343S0177"
#define kDacaModelName				"353S0228"
#define kTexasModelName				"355S0056"
#define	kTexas2ModelName			"353S0303"

#define kMuxPSurgeObjName 			"Proj1Mux"        
#define kMuxAlchemyObjName   		"Proj2Mux"    
#define kMuxHooperObjName    		"Proj3Mux"
#define kMuxPExpressObjName			"Proj4Mux"    
#define kMuxWSObjName         		"Proj5Mux"
#define kMuxGossWingsAObjName  		"Proj6Mux" 
#define kMuxGossWingsBObjName   	"Proj7Mux"
#define kMuxGossCanardObjName   	"Proj8Mux"
#define kMux101ObjName          	"Proj9Mux"
#define kMuxProgOutName         	"MuxProgOut" 

#define kSourceMapPropName      	"source-map"
#define kSourceMapCountPropName 	"source-map-count"

// Machine layout constants. All NewWorld machines have a device-id property
// matching one of these layout constants for specifying the sound hardware layout.
enum {
	layoutC1					=	1,
	layout101					=	2,
	layoutG3					=	3,
	layoutYosemite				=	4,
	layoutSawtooth				=	5,
	layoutP1					=	6,
	layoutUSB					=	7,
	layoutKihei					= 	8,
	layoutDigitalCD				= 	9,
	layoutPismo					=	10,
	layoutPerigee				= 	11,
	layoutVirtual				=	12,
	layoutMercury				=	13,
	layoutTangent				=	14,
	layoutTessera				=	15,
	layoutP29					=	16,
	layoutWallStreet			=   17,
	layoutP25					=	18,
	layoutP53					=	19,
	layoutP54					=	20,
	layoutP57					=	21,
	layoutP58					=	22,
	layoutP62					=	23,
	layoutP72					=	24,
	layoutP92					=	25
};

// Hardware type 
enum{
    kUnknownHardwareType,
    kGCAwacsHardwareType,
    kBurgundyHardwareType,
    kDACAHardwareType,
    kTexas3001HardwareType,
};

// Kind of devices
enum {
    kSndHWInternalSpeaker	=	0x00000001,		// internal speaker present on CPU
    kSndHWCPUHeadphone		=	0x00000002,		// headphones inserted into CPU headphone port
    kSndHWCPUExternalSpeaker	=	0x00000004,		// external speakers (or headphones) inserted in CPU output port
    kSndHWCPUSubwoofer		=	0x00000008,		// subwoofer (software controllable) present
    kSndHWCPUMicrophone		=	0x00000010,		// short jack microphone (mic level) inserted in CPU input port
    kSndHWCPUPlainTalk		=	0x00000020,		// PlainTalk microphone inserted into CPU input port
    kSndHWMonitorHeadphone	=	0x00000040,		// headphones inserted in monitor headphone port
    kSndHWMonitorPlainTalk	=	0x00000080,		// PlainTalk source input inserted in sound input port (even though it may physically be a short plug)
    kSndHWModemRingDetect	=	0x00000100,		// modem ring detect
    kSndHWModemLineCurrent	=	0x00000200,		// modem line current
    kSndHWModemESquared		=	0x00000400,		// modem E squared

    kSndHWInputDevices		=       0x000000B0,		// mask to get input devices (excluding modems)
    kSndHWAllDevices		=	0xFFFFFFFF		// all available devices
    
};

// Codec kind
enum {
    kSndHWTypeUnknown			=	0x00000000,		// unknown part
    kSndHWTypeAWACs			=	0x00000001,		// AWACs part
    kSndHWTypeScreamer			=	0x00000002,		// Screamer part
    kSndHWTypeBurgundy			=	0x00000003,		// Burgundy
    kSndHWTypeUSB				=	0x00000004,		// USB codec on a wire...
    kSndHWTypeDaca				= 	0x00000005,		// DAC3550A
    kSndHWTypeDigitalSnd		=	0x00000006,		// DigitalSnd virtual HW
    kSndHWTypeTumbler			=	0x00000007,		// Tumbler I2S with Equalizer & Dallas ID thing...
	kSndHWTypeTexas2			=	0x00000008,		// Texas2 I2s with Equalizer & Dallas ID thing...

    kSndHWManfUnknown			=	0x00000000,		// unknown manufacturer (error during read)
    kSndHWManfCrystal			=	0x00000001,		// manufactured by crystal
    kSndHWManfNational			=	0x00000002,		// manufactured by national
    kSndHWManfTI				=	0x00000003,		// manufactured by texas instruments
    kSndHWManfMicronas			=	0x00000004		// manufactured by Micronas Intermetall
};


// Output port constants
enum {
	kSndHWOutput1				=	1,				// output 1
	kSndHWOutput2				=	2,				// output 2
	kSndHWOutput3				=	3,				// output 3
	kSndHWOutput4				=	4,				// output 4
	kSndHWOutput5				=	5,				// output 5
	
	kSndHWOutputNone			=	0				// no output
};


enum {
	kSndHWProgOutput0			=	0x00000001,		// programmable output zero
	kSndHWProgOutput1			=	0x00000002,		// programmable output one
	kSndHWProgOutput2			=	0x00000004,		// programmable output two
	kSndHWProgOutput3			=	0x00000008,		// programmable output three
	kSndHWProgOutput4			=	0x00000010		// programmable output four
};

// Input Port constants
enum {
	kSndHWInSenseNone			=	0x00000000,		// no input sense bits
	kSndHWInSense0				=	0x00000001,		// input sense bit zero
	kSndHWInSense1				=	0x00000002,		// input sense bit one
	kSndHWInSense2				=	0x00000004,		// input sense bit two
	kSndHWInSense3				=	0x00000008,		// input sense bit three
	kSndHWInSense4				=	0x00000010,		// input sense bit four
	kSndHWInSense5				=	0x00000020,		// input sense bit five
	kSndHWInSense6				=	0x00000040		// input sense bit six
};


enum {
  kSndHWInput1                  = 1,    /* input 1*/
  kSndHWInput2                  = 2,    /* input 2*/
  kSndHWInput3                  = 3,    /* input 3*/
  kSndHWInput4                  = 4,    /* input 4*/
  kSndHWInput5                  = 5,    /* input 5*/
  kSndHWInput6                  = 6,    /* input 6*/
  kSndHWInput7                  = 7,    /* input 7*/
  kSndHWInput8                  = 8,    /* input 8*/
  kSndHWInput9                  = 9,    /* input 9*/
  kSndHWInputNone               = 0     /* no input*/
};


// Shift value to identify the control IDs
#define DETECTSHIFT 1000
#define OUTPUTSHIFT 1100
#define INPUTSHIFT  1200


// PRAM read write values
enum{
    kMaximumPRAMVolume 	= 7,
    kMinimumPRAMVolume	= 0,
    KNumPramVolumeSteps	= (kMaximumPRAMVolume- kMinimumPRAMVolume+1),
    kPRamVolumeAddr	= 8,
    
    kDefaultVolume	= 0x006E006E,
    kInvalidVolumeMask	= 0xFE00FE00
    
};

typedef UInt32 sndHWDeviceSpec;

#endif
