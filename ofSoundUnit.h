/*
 *  ofSoundUnit.h
 *  ofSound
 *
 *  Created by damian on 10/01/11.
 *  Copyright 2011 frey damian@frey.co.nz. All rights reserved.
 *
 */

#pragma once

#include "ofSoundStream.h"
#include "ofTypes.h"
#include "ofUtils.h"
#include <vector>
using namespace std;

class ofSoundSource;


/** ofSoundUnit
 
 Base class for sound units that can be strung together into a DSP-style chain.
 
 @author damian
 */

class ofSoundUnit
{
public:	
	virtual ~ofSoundUnit() {};
	
	/// Return the name of this synth unit.
	virtual string getName() = 0;

	/// Return our inputs in a vector (but at ofSoundUnit level we have no inputs).
	virtual vector<ofSoundSource*> getInputs()  { return vector<ofSoundSource*>(); }
	
protected:
	
	
};




/** ofSoundSource
 
 Base class for ofSoundUnits that generate some kind of sound output.
 
 @author damian
 */


class ofSoundSource: public virtual ofSoundUnit
{
public:
	virtual ~ofSoundSource() {};

	/// Set the sample rate. If you need to know about sample rate changes, override this function.
	virtual void setSampleRate( int rate ) {};
	
	/// Fill buffer with (numFrames*numChannels) floats of data, interleaved
	virtual void audioRequested( float* buffer, int numFrames, int numChannels ) = 0;
	
protected:
	
	
};



/** ofAudioBuffer
 
 Wrapper for an interleaved floating point buffer holding a fixed number of frames
 of a fixed number of channels of audio.
 
 @author damian
 */

class ofAudioBuffer // Changed to ofAudioBuffer
{
public:
	
	float* buffer;
	int numFrames;
	int numChannels;
	
	ofAudioBuffer();
	ofAudioBuffer( const ofAudioBuffer& other );
	ofAudioBuffer( int nFrames, int nChannels );
	ofAudioBuffer& operator=( const ofAudioBuffer& other );
	
	~ofAudioBuffer();
	void copyFrom( const ofAudioBuffer& other );	
	/// Set audio data to 0.
	void clear();
	
	/// Allocate the buffer to the given size if necessary.
	void allocate( int nFrames, int nChannels );
	
	/// Copy just a single channel to output. output should have space for numFrames floats.
	void copyChannel( int channel, float* output ) const ;
	
	/// Copy safely to out. Copy as many frames as possible, repeat channels as necessary.
	void copyTo( float* outBuffer, int outNumFrames, int outNumChannels );


};


/** ofSoundSink
 
 Base class for ofSoundUnits that receive audio data, either by pulling from upstream 
 inputs, or from the audioReceived function which is called from outside in special 
 cases (eg when used as microphone/line input).
 
 @author damian
 */

class ofSoundSink: public virtual ofSoundUnit
{
public:
	ofSoundSink() { input = NULL; sampleRate = 44100; }
	virtual ~ofSoundSink() {};
	
	/// Set the sample rate of this synth unit. If you overload this remember to call the base class.
	virtual void setSampleRate( int rate );

	/// Add an input to this unit from the given source unit. If overloading remember to call base.
	virtual bool addInputFrom( ofSoundSource* source );
	
	/// Return our inputs in a vector.
	virtual vector<ofSoundSource*> getInputs();
	
	/// Receive incoming audio from elsewhere (eg coming from a microphone or line input source)
	virtual void audioReceived( float* buffer, int numFrames, int numChannels );
	
protected:
	// fill our input buffer from the upstream source (input)
	void fillInputBufferFromUpstream( int numFrames, int numChannels );
	
	// walk the DSP graph and see if adding test_input as an input to ourselves; return true if it would.
	bool addingInputWouldCreateCycle( ofSoundSource* test_input );

	
	void lock() { mutex.lock(); }
	void unlock() { mutex.unlock(); }	
	ofMutex mutex;
		
	
	
	ofSoundSource* input;
	ofAudioBuffer inputBuffer;

	int sampleRate;
};




/** ofSoundMixer
 
 Mixes together inputs from multiple ofSoundSources.
 
 @author damian
 */

class ofSoundMixer : public ofSoundSink, public ofSoundSource
{
public:	
	ofSoundMixer() { working = NULL; masterVolume = 1.0f; }
	ofSoundMixer( const ofSoundMixer& other ) { copyFrom( other ); }
	ofSoundMixer& operator=( const ofSoundMixer& other ) { copyFrom( other ); return *this; }
	~ofSoundMixer();
	
	string getName() { return "ofSoundMixer"; }
	
	/// Set the master out volume of this mixer
	void setMasterVolume( float vol ) { if ( !isnan(vol) && isfinite(vol) ) { masterVolume = vol; } }
	/// Set the volume of the mixer input for the given unit to vol.
	void setVolume( ofSoundSource* input, float vol );
	/// Set the pan of the mixer input for the given unit to pan
	void setPan( ofSoundSource* input, float pan );
	
	/// Add an input to this unit from the given source unit.
	bool addInputFrom( ofSoundSource* source );
	
	
	/// Remove an input from the mixer
	bool removeInputFrom(ofSoundSource* source);
	
	/// render
	void audioRequested( float* buffer, int numFrames, int numChannels );
	
	/// return all our inputs
	vector<ofSoundSource*> getInputs();
	
	/// pass sample rate changes on to inputs
	void setSampleRate( int rate );
	
private:
	// safely copy from the other mixer (probably a bad idea though)
	void copyFrom( const ofSoundMixer& other );
	
	// Inputs into the mixer, with volume and pan
	struct MixerInput
	{
		ofSoundSource* input;
		ofAudioBuffer inputBuffer;
		float volume;
		float pan;
		MixerInput( ofSoundSource* i, float v, float p )
		{
			input = i;
			volume = v; 
			pan = p;
		}
	};
	vector<MixerInput*> inputs;

	float masterVolume;
	
	float* working;
};




/** ofSoundDeclickedFloat
 
 Declick a changing float value by using a 64 sample ramp (around 1.5ms at 44.1kHz).
 
 You must call rebuildRampIfNecessary before processing every audio block, in order to apply incoming
 value changes.
 
 Also, you must call frameTick() for every audio frame (sample) to advance the ramp.
 
 @author damian
 */

class ofSoundDeclickedFloat
{
public:
	ofSoundDeclickedFloat( float startValue=0.0f ) { rampPos = 0; current = startValue; setValue( startValue ); }
	
	/// Return the current value, declicked
	float getDeclickedValue() { return current; }
	/// Return the raw value (the target for declicking)
	float getRawValue() { return target; }
	/// Set a new value + rebuild ramp
	void setValue( float newValue ) { if ( !isnan(newValue)&&finite(newValue) ) { target = newValue; rampNeedsRebuild = true; } }
	
	/// Rebuild the ramp, if necessary. Call before processing a block of samples.
	void rebuildRampIfNecessary() { if ( rampNeedsRebuild ) rebuildRamp(); rampNeedsRebuild = false; }
	
	/// Update, to be called for every frame
	void frameTick() { current = ramp[rampPos]; ramp[rampPos] = target; rampPos = (rampPos+1)%64; }
	
	/// operator overloading
	ofSoundDeclickedFloat& operator=( float new_value ) { setValue( new_value ); return *this; }
	ofSoundDeclickedFloat& operator+=( float adjustment ) { setValue( target+adjustment ); return *this; }
	ofSoundDeclickedFloat& operator-=( float adjustment ) { setValue( target-adjustment ); return *this; }
	
private:
	
	void rebuildRamp() { float v = current; float step = (target-current)/63; for ( int i=0; i<64; i++ ) { ramp[(i+rampPos)%64] = v; v += step; } }
	
	float current;
	float target;
	
	bool rampNeedsRebuild;
	int rampPos;
	float ramp[64];
};


/** ofSoundSourceTestTone
 
 An ofSoundSource that generates a test tone with a given frequency.
 
 @author damian
 */

class ofSoundSourceTestTone: public ofSoundSource
{
public:	
	static const int TESTTONE_SINE = 0;
	static const int TESTTONE_SAWTOOTH = 1;
	
	ofSoundSourceTestTone() { sampleRate = 44100; phase = 0; sawPhase = 0; setFrequency( 440.0f ); waveform = TESTTONE_SINE; }
	
	/// Return our name
	string getName() { return "ofSoundUnitTestTone"; }
	
	/// Set our frequency
	void setFrequency( float freq );
	/// Set our frequency using a midi note
	void setFrequencyMidiNote( float midiNote ) { setFrequency(440.0f*powf(2.0f,(midiNote-60.0f)/12.0f)); };
	/// Update generation for the new sample rate
	void setSampleRate( int rate );
	/// Set waveform
	void setSineWaveform() { waveform = TESTTONE_SINE; }
	void setSawtoothWaveform() { waveform = TESTTONE_SAWTOOTH; }
	
	void audioRequested( float* output, int numFrames, int numChannels );
	
private:
	
	float phase;
	float frequency;
	int sampleRate;
	float phaseAdvancePerFrame;
	
	int waveform;
	
	float sawPhase;
	float sawAdvancePerFrame;
	
};
