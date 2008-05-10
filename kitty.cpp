#include <juce.h>
#include "kitty.h"
#include "kittyEditor.h"

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kitty();
}

kitty::kitty()
{
    bitDepth = 32;
    sampleRate = 1.0;
}

kitty::~kitty()
{
}

const String kitty::getName() const
{
    return "kitty decimator";
}

int kitty::getNumParameters()
{
    return (2);
}

float kitty::getParameter (int index)
{
	if (index == kBitDepth)
	{
		return ((float)bitDepth/32);
	}

	if (index == kSampleRate)
	{
		return (sampleRate);
	}

	return (0.0);
}

void kitty::setParameter (int index, float newValue)
{
	if (index == kBitDepth)
	{
		if (bitDepth != roundFloatToInt(newValue))
		{
			bitDepth = (int)(newValue * 32);
			sendChangeMessage (this);
		}
	} 

	if (index == kSampleRate)
	{
		if (sampleRate != newValue)
		{
			sampleRate = newValue;
			sendChangeMessage (this);
		}
	}
}

const String kitty::getParameterName (int index)
{
	if (index == kBitDepth)
	{
		return T("Bit Depth");
	}

	if (index == kSampleRate)
	{
		return T("Sample Rate");
	}

	return String::empty;
}

const String kitty::getParameterText (int index)
{
	if (index == kBitDepth)
	{
		return String::formatted (T("bits:%d"), bitDepth);
	}
	if (index == kSampleRate)
	{
		return String::formatted (T("sr:%.2f"), sampleRate);
	}
	
	return String::empty;
}

const String kitty::getInputChannelName (const int channelIndex) const
{
    return String (channelIndex + 1);
}

const String kitty::getOutputChannelName (const int channelIndex) const
{
    return String (channelIndex + 1);
}

bool kitty::isInputChannelStereoPair (int index) const
{
    return false;
}

bool kitty::isOutputChannelStereoPair (int index) const
{
    return false;
}

bool kitty::acceptsMidi() const
{
    return false;
}

bool kitty::producesMidi() const
{
    return false;
}

void kitty::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void kitty::releaseResources()
{
}

void kitty::processBlock (AudioSampleBuffer& buffer,
                                   MidiBuffer& midiMessages)
{
	y=cnt=0;
	m=1<<(bitDepth-1);

	for (int channel = 0; channel < getNumInputChannels(); ++channel)
	{
		float *p = buffer.getSampleData (channel);
		int size = buffer.getNumSamples();

		for (int x=0; x<size; x++)
		{
			*(p+x) = decimate (*(p+x));
		}
	}

	for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i)
	{
		buffer.clear (i, 0, buffer.getNumSamples());
	}
}

float kitty::decimate(float i)
{
	cnt += sampleRate;
	if (cnt >= 1)
	{
		cnt -= 1;
		y = (long int)(i*m)/(float)m;
	}

	return y;
}

AudioProcessorEditor* kitty::createEditor()
{
    return new kittyEditor (this);
}

void kitty::getStateInformation (MemoryBlock& destData)
{
    XmlElement xmlState (T("kittySettings"));
    xmlState.setAttribute (T("bitDepth"), bitDepth);
    xmlState.setAttribute (T("sampleRate"), sampleRate);
    copyXmlToBinary (xmlState, destData);
}

void kitty::setStateInformation (const void* data, int sizeInBytes)
{
	XmlElement* const xmlState = getXmlFromBinary (data, sizeInBytes);

    if (xmlState != 0)
    {
        if (xmlState->hasTagName (T("kittySettings")))
        {
            bitDepth = xmlState->getIntAttribute (T("bitDepth"), bitDepth);
            sampleRate = (float)xmlState->getDoubleAttribute (T("sampleRate"), sampleRate);

            sendChangeMessage (this);
        }

        delete xmlState;
    }
}

void kitty::setBitDepth(int d)
{
	bitDepth = d;
}

void kitty::setSampleRate (float r)
{
	sampleRate = r;
}
