#ifndef KITTY_H
#define KITTY_H

class kitty  : public AudioProcessor, public ChangeBroadcaster
{
public:
    kitty();
    ~kitty();
    enum
    {
	kBitDepth,
	kSampleRate
    };
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
    AudioProcessorEditor* createEditor();

    const String getName() const;

    int getNumParameters();

    float getParameter (int index);
    void setParameter (int index, float newValue);

    const String getParameterName (int index);
    const String getParameterText (int index);

    const String getInputChannelName (const int channelIndex) const;
    const String getOutputChannelName (const int channelIndex) const;
    bool isInputChannelStereoPair (int index) const;
    bool isOutputChannelStereoPair (int index) const;

    bool acceptsMidi() const;
    bool producesMidi() const;

    int getNumPrograms()                                        { return 0; }
    int getCurrentProgram()                                     { return 0; }
    void setCurrentProgram (int index)                          { }
    const String getProgramName (int index)                     { return String::empty; }
    void changeProgramName (int index, const String& newName)   { }

    void getStateInformation (MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);
    float decimate(float i);

    void setBitDepth (int d);
    void setSampleRate (float r);

    juce_UseDebuggingNewOperator

private:
    float gain;
    float y;
    float cnt;
    float sampleRate;
    int bitDepth;
    long m;
};

#endif
