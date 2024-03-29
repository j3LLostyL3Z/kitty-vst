/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-7 by Raw Material Software ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the
   GNU General Public License, as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later version.

   JUCE is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with JUCE; if not, visit www.gnu.org/licenses or write to the
   Free Software Foundation, Inc., 59 Temple Place, Suite 330,
   Boston, MA 02111-1307 USA

  ------------------------------------------------------------------------------

   If you'd like to release a closed-source product which uses JUCE, commercial
   licenses are also available: visit www.rawmaterialsoftware.com/juce for
   more information.

  ==============================================================================
*/

/*
                        ***  DON't EDIT THIS FILE!!  ***

    The idea is that everyone's plugins should share this same wrapper
    code, so if you start hacking around in here you're missing the point!

    If there's a bug or a function you need that can't be done without changing
    some of the code in here, give me a shout so we can add it to the library,
    rather than branching off and going it alone!
*/


//==============================================================================
#ifdef _MSC_VER
  #pragma warning (disable : 4996)
#endif

#ifdef _WIN32
  #include <windows.h>
#elif defined (LINUX)
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/Xatom.h>
  #undef KeyPress
#else
  #include <Carbon/Carbon.h>
#endif

#ifdef PRAGMA_ALIGN_SUPPORTED
  #undef PRAGMA_ALIGN_SUPPORTED
  #define PRAGMA_ALIGN_SUPPORTED 1
#endif

#include "../../juce_IncludeCharacteristics.h"

//==============================================================================
/*  These files come with the Steinberg VST SDK - to get them, you'll need to
    visit the Steinberg website and jump through some hoops to sign up as a
    VST developer.

    Then, you'll need to make sure your include path contains your "vstsdk2.3" or
    "vstsdk2.4" directory.

    Note that the JUCE_USE_VSTSDK_2_4 macro should be defined in JucePluginCharacteristics.h
*/
#if JUCE_USE_VSTSDK_2_4
 // VSTSDK V2.4 includes..
 #include "public.sdk/source/vst2.x/audioeffectx.h"
 #include "public.sdk/source/vst2.x/aeffeditor.h"
 #include "public.sdk/source/vst2.x/audioeffectx.cpp"
 #include "public.sdk/source/vst2.x/audioeffect.cpp"
#else
 // VSTSDK V2.3 includes..
 #include "source/common/audioeffectx.h"
 #include "source/common/AEffEditor.hpp"
 #include "source/common/audioeffectx.cpp"
 #include "source/common/AudioEffect.cpp"
 typedef long VstInt32;
 typedef long VstIntPtr;
 enum Vst2StringConstants
 {
   kVstMaxNameLen       = 64,
   kVstMaxLabelLen      = 64,
   kVstMaxShortLabelLen = 8,
   kVstMaxCategLabelLen = 24,
   kVstMaxFileNameLen   = 100
 };

 enum VstSmpteFrameRate
 {
    kVstSmpte24fps    = 0,  ///< 24 fps
    kVstSmpte25fps    = 1,  ///< 25 fps
    kVstSmpte2997fps  = 2,  ///< 29.97 fps
    kVstSmpte30fps    = 3,  ///< 30 fps
    kVstSmpte2997dfps = 4,  ///< 29.97 drop
    kVstSmpte30dfps   = 5,  ///< 30 drop
    kVstSmpteFilm16mm = 6,  ///< Film 16mm
    kVstSmpteFilm35mm = 7,  ///< Film 35mm
    kVstSmpte239fps   = 10, ///< HDTV: 23.976 fps
    kVstSmpte249fps   = 11, ///< HDTV: 24.976 fps
    kVstSmpte599fps   = 12, ///< HDTV: 59.94 fps
    kVstSmpte60fps    = 13  ///< HDTV: 60 fps
 };
#endif

//==============================================================================
#ifdef _MSC_VER
  #pragma pack (push, 8)
#endif

#include <juce.h>

#ifdef _MSC_VER
  #pragma pack (pop)
#endif

#undef MemoryBlock

class JuceVSTWrapper;
static bool recursionCheck = false;
static uint32 lastMasterIdleCall = 0;

BEGIN_JUCE_NAMESPACE
 extern void juce_callAnyTimersSynchronously();

 #if JUCE_MAC
  extern void juce_setCurrentExecutableFileNameFromBundleId (const String& bundleId) throw();
 #endif

 #if JUCE_LINUX
  extern Display* display;
  extern bool juce_postMessageToSystemQueue (void* message);
 #endif
END_JUCE_NAMESPACE


//==============================================================================
#if JUCE_WIN32

static HWND findMDIParentOf (HWND w)
{
    const int frameThickness = GetSystemMetrics (SM_CYFIXEDFRAME);

    while (w != 0)
    {
        HWND parent = GetParent (w);

        if (parent == 0)
            break;

        TCHAR windowType [32];
        zeromem (windowType, sizeof (windowType));
        GetClassName (parent, windowType, 31);

        if (String (windowType).equalsIgnoreCase (T("MDIClient")))
        {
            w = parent;
            break;
        }

        RECT windowPos;
        GetWindowRect (w, &windowPos);

        RECT parentPos;
        GetWindowRect (parent, &parentPos);

        const int dw = (parentPos.right - parentPos.left) - (windowPos.right - windowPos.left);
        const int dh = (parentPos.bottom - parentPos.top) - (windowPos.bottom - windowPos.top);

        if (dw > 100 || dh > 100)
            break;

        w = parent;

        if (dw == 2 * frameThickness)
            break;
    }

    return w;
}

//==============================================================================
#elif JUCE_LINUX

class SharedMessageThread : public Thread
{
public:
    SharedMessageThread()
      : Thread (T("VstMessageThread"))
    {
        startThread (7);
    }

    ~SharedMessageThread()
    {
        signalThreadShouldExit();

        const int quitMessageId = 0xfffff321;
        Message* const m = new Message (quitMessageId, 1, 0, 0);

        if (! juce_postMessageToSystemQueue (m))
            delete m;

        clearSingletonInstance();
    }

    void run()
    {
        MessageManager* const messageManager = MessageManager::getInstance();

        const int originalThreadId = messageManager->getCurrentMessageThread();
        messageManager->setCurrentMessageThread (getThreadId());

        while (! threadShouldExit()
                && messageManager->dispatchNextMessage())
        {
        }

        messageManager->setCurrentMessageThread (originalThreadId);
    }

    juce_DeclareSingleton (SharedMessageThread, false)
};

juce_ImplementSingleton (SharedMessageThread);

#endif

//==============================================================================
// A component to hold the AudioProcessorEditor, and cope with some housekeeping
// chores when it changes or repaints.
class EditorCompWrapper  : public Component,
                           public AsyncUpdater
{
    JuceVSTWrapper* wrapper;

public:
    EditorCompWrapper (JuceVSTWrapper* const wrapper_,
                       AudioProcessorEditor* const editor)
        : wrapper (wrapper_)
    {
        setOpaque (true);
        editor->setOpaque (true);

        setBounds (editor->getBounds());
        editor->setTopLeftPosition (0, 0);
        addAndMakeVisible (editor);

#if JUCE_WIN32
        addMouseListener (this, true);
#endif
    }

    ~EditorCompWrapper()
    {
        deleteAllChildren();
    }

    void paint (Graphics& g)
    {
    }

    void paintOverChildren (Graphics& g)
    {
        // this causes an async call to masterIdle() to help
        // creaky old DAWs like Nuendo repaint themselves while we're
        // repainting. Otherwise they just seem to give up and sit there
        // waiting.
        triggerAsyncUpdate();
    }

    AudioProcessorEditor* getEditorComp() const
    {
        return dynamic_cast <AudioProcessorEditor*> (getChildComponent (0));
    }

    void resized()
    {
        Component* const c = getChildComponent (0);

        if (c != 0)
        {
#if JUCE_LINUX
            const MessageManagerLock mml;
#endif
            c->setBounds (0, 0, getWidth(), getHeight());
        }
    }

    void childBoundsChanged (Component* child);
    void handleAsyncUpdate();

#if JUCE_WIN32
    void mouseDown (const MouseEvent&)
    {
        broughtToFront();
    }

    void broughtToFront()
    {
        // for hosts like nuendo, need to also pop the MDI container to the
        // front when our comp is clicked on.
        HWND parent = findMDIParentOf ((HWND) getWindowHandle());

        if (parent != 0)
        {
            SetWindowPos (parent,
                          HWND_TOP,
                          0, 0, 0, 0,
                          SWP_NOMOVE | SWP_NOSIZE);
        }
    }
#endif

    //==============================================================================
    juce_UseDebuggingNewOperator
};

static VoidArray activePlugins;


//==============================================================================
/**
    This wraps an AudioProcessor as an AudioEffectX...
*/
class JuceVSTWrapper  : public AudioEffectX,
                        private Timer,
                        public AudioProcessorListener,
                        public AudioPlayHead
{
public:
    //==============================================================================
    JuceVSTWrapper (audioMasterCallback audioMaster,
                    AudioProcessor* const filter_)
       : AudioEffectX (audioMaster,
                       filter_->getNumPrograms(),
                       filter_->getNumParameters()),
         filter (filter_)
    {
        editorComp = 0;
        outgoingEvents = 0;
        outgoingEventSize = 0;
        chunkMemoryTime = 0;
        isProcessing = false;
        firstResize = true;
        hasShutdown = false;
        firstProcessCallback = true;
        channels = 0;
        numInChans = JucePlugin_MaxNumInputChannels;
        numOutChans = JucePlugin_MaxNumOutputChannels;

#if JUCE_MAC || JUCE_LINUX
        hostWindow = 0;
#endif

        filter->setPlayConfigDetails (numInChans, numOutChans, 0, 0);

        filter_->setPlayHead (this);
        filter_->addListener (this);

        cEffect.flags |= effFlagsHasEditor;
        cEffect.version = (long) (JucePlugin_VersionCode);

        setUniqueID ((int) (JucePlugin_VSTUniqueID));

#if JucePlugin_WantsMidiInput && ! JUCE_USE_VSTSDK_2_4
        wantEvents();
#endif

        setNumInputs (numInChans);
        setNumOutputs (numOutChans);

        canProcessReplacing (true);

#if ! JUCE_USE_VSTSDK_2_4
        hasVu (false);
        hasClip (false);
#endif

        isSynth ((JucePlugin_IsSynth) != 0);
        noTail ((JucePlugin_SilenceInProducesSilenceOut) != 0);
        setInitialDelay (filter->getLatencySamples());
        programsAreChunks (true);

        activePlugins.add (this);
    }

    ~JuceVSTWrapper()
    {
        stopTimer();
        deleteEditor();

        hasShutdown = true;

        delete filter;
        filter = 0;

        if (outgoingEvents != 0)
        {
            for (int i = outgoingEventSize; --i >= 0;)
                juce_free (outgoingEvents->events[i]);

            juce_free (outgoingEvents);
            outgoingEvents = 0;
        }

        jassert (editorComp == 0);

        juce_free (channels);
        channels = 0;
        deleteTempChannels();

        jassert (activePlugins.contains (this));
        activePlugins.removeValue (this);

        if (activePlugins.size() == 0)
        {
#if JUCE_LINUX
            SharedMessageThread::deleteInstance();
#endif
            shutdownJuce_GUI();
        }
    }

    void open()
    {
        startTimer (1000 / 4);
    }

    void close()
    {
        jassert (! recursionCheck);

        stopTimer();
        deleteEditor();
    }

    //==============================================================================
    bool getEffectName (char* name)
    {
        String (JucePlugin_Name).copyToBuffer (name, 64);
        return true;
    }

    bool getVendorString (char* text)
    {
        String (JucePlugin_Manufacturer).copyToBuffer (text, 64);
        return true;
    }

    bool getProductString (char* text)
    {
        return getEffectName (text);
    }

    VstInt32 getVendorVersion()
    {
        return JucePlugin_VersionCode;
    }

    VstPlugCategory getPlugCategory()
    {
        return JucePlugin_VSTCategory;
    }

    VstInt32 canDo (char* text)
    {
        VstInt32 result = 0;

        if (strcmp (text, "receiveVstEvents") == 0
            || strcmp (text, "receiveVstMidiEvent") == 0
            || strcmp (text, "receiveVstMidiEvents") == 0)
        {
#if JucePlugin_WantsMidiInput
            result = 1;
#else
            result = -1;
#endif
        }
        else if (strcmp (text, "sendVstEvents") == 0
                 || strcmp (text, "sendVstMidiEvent") == 0
                 || strcmp (text, "sendVstMidiEvents") == 0)
        {
#if JucePlugin_ProducesMidiOutput
            result = 1;
#else
            result = -1;
#endif
        }
        else if (strcmp (text, "receiveVstTimeInfo") == 0
                 || strcmp (text, "conformsToWindowRules") == 0)
        {
            result = 1;
        }

        return result;
    }

    bool keysRequired()
    {
        return (JucePlugin_EditorRequiresKeyboardFocus) != 0;
    }

    bool getInputProperties (VstInt32 index, VstPinProperties* properties)
    {
        if (filter == 0 || index >= filter->getNumInputChannels())
            return false;

        const String name (filter->getInputChannelName ((int) index));

        name.copyToBuffer (properties->label, kVstMaxLabelLen - 1);
        name.copyToBuffer (properties->shortLabel, kVstMaxShortLabelLen - 1);

        properties->flags = kVstPinIsActive;

        if (filter->isInputChannelStereoPair ((int) index))
            properties->flags |= kVstPinIsStereo;

        properties->arrangementType = 0;

        return true;
    }

    bool getOutputProperties (VstInt32 index, VstPinProperties* properties)
    {
        if (filter == 0 || index >= filter->getNumOutputChannels())
            return false;

        const String name (filter->getOutputChannelName ((int) index));

        name.copyToBuffer (properties->label, kVstMaxLabelLen - 1);
        name.copyToBuffer (properties->shortLabel, kVstMaxShortLabelLen - 1);

        properties->flags = kVstPinIsActive;

        if (filter->isOutputChannelStereoPair ((int) index))
            properties->flags |= kVstPinIsStereo;

        properties->arrangementType = 0;

        return true;
    }

    //==============================================================================
    VstInt32 processEvents (VstEvents* events)
    {
#if JucePlugin_WantsMidiInput
        for (int i = 0; i < events->numEvents; ++i)
        {
            const VstEvent* const e = events->events[i];

            if (e != 0 && e->type == kVstMidiType)
            {
                const VstMidiEvent* const vme = (const VstMidiEvent*) e;

                midiEvents.addEvent ((const uint8*) vme->midiData,
                                     4,
                                     vme->deltaFrames);
            }
        }

        return 1;
#else
        return 0;
#endif
    }

    void process (float** inputs, float** outputs, VstInt32 numSamples)
    {
        const int numIn = numInChans;
        const int numOut = numOutChans;

        AudioSampleBuffer temp (numIn, numSamples);
        int i;
        for (i = numIn; --i >= 0;)
            memcpy (temp.getSampleData (i), outputs[i], sizeof (float) * numSamples);

        processReplacing (inputs, outputs, numSamples);

        AudioSampleBuffer dest (outputs, numOut, numSamples);

        for (i = jmin (numIn, numOut); --i >= 0;)
            dest.addFrom (i, 0, temp, i, 0, numSamples);
    }

    void processReplacing (float** inputs, float** outputs, VstInt32 numSamples)
    {
        if (firstProcessCallback)
        {
            firstProcessCallback = false;

            // if this fails, the host hasn't called resume() before processing
            jassert (isProcessing);

            // (tragically, some hosts actually need this, although it's stupid to have
            //  to do it here..)
            if (! isProcessing)
                resume();

            filter->setNonRealtime (getCurrentProcessLevel() == 4 /* kVstProcessLevelOffline */);

#if JUCE_WIN32
            if (GetThreadPriority (GetCurrentThread()) <= THREAD_PRIORITY_NORMAL)
                filter->setNonRealtime (true);
#endif
        }

#if JUCE_DEBUG && ! JucePlugin_ProducesMidiOutput
        const int numMidiEventsComingIn = midiEvents.getNumEvents();
#endif

        jassert (activePlugins.contains (this));

        {
            const ScopedLock sl (filter->getCallbackLock());

            const int numIn = numInChans;
            const int numOut = numOutChans;

            if (filter->isSuspended())
            {
                for (int i = 0; i < numOut; ++i)
                    zeromem (outputs[i], sizeof (float) * numSamples);
            }
            else
            {
                if (! hasCreatedTempChannels)
                {
                    // do this just once when we start processing..
                    hasCreatedTempChannels = true;

                    // if some output channels are disabled, some hosts supply the same buffer
                    // for multiple channels - this buggers up our method of copying the
                    // inputs over the outputs, so we need to create unique temp buffers in this case..
                    for (int i = 0; i < numOut; ++i)
                    {
                        for (int j = i; --j >= 0;)
                        {
                            if (outputs[j] == outputs[i] && outputs[i] != 0)
                            {
                                tempChannels.set (i, juce_malloc (sizeof (float) * blockSize * 2));
                                break;
                            }
                        }
                    }
                }

                {
                    int i;
                    for (i = 0; i < numOut; ++i)
                    {
                        // if some output channels are disabled, the host may pass the same dummy buffer
                        // pointer for all of these outputs - and that means that we'd be copying all our
                        // input channels into the same place... so in this case, we use an internal dummy
                        // buffer which has enough channels for each input.
                        float* chan = (float*) tempChannels.getUnchecked(i);
                        if (chan == 0)
                            chan = outputs[i];

                        if (i < numIn && chan != inputs[i])
                            memcpy (chan, inputs[i], sizeof (float) * numSamples);

                        channels[i] = chan;
                    }

                    for (; i < numIn; ++i)
                        channels[i] = inputs[i];
                }

                AudioSampleBuffer chans (channels, jmax (numIn, numOut), numSamples);

                filter->processBlock (chans, midiEvents);
            }
        }

        if (! midiEvents.isEmpty())
        {
#if JucePlugin_ProducesMidiOutput
            const int numEvents = midiEvents.getNumEvents();

            ensureOutgoingEventSize (numEvents);
            outgoingEvents->numEvents = 0;

            const uint8* midiEventData;
            int midiEventSize, midiEventPosition;
            MidiBuffer::Iterator i (midiEvents);

            while (i.getNextEvent (midiEventData, midiEventSize, midiEventPosition))
            {
                if (midiEventSize <= 4)
                {
                    VstMidiEvent* const vme = (VstMidiEvent*) outgoingEvents->events [outgoingEvents->numEvents++];

                    memcpy (vme->midiData, midiEventData, midiEventSize);
                    vme->deltaFrames = midiEventPosition;

                    jassert (vme->deltaFrames >= 0 && vme->deltaFrames < numSamples);
                }
            }

            sendVstEventsToHost (outgoingEvents);
#else
            /*  This assertion is caused when you've added some events to the
                midiMessages array in your processBlock() method, which usually means
                that you're trying to send them somewhere. But in this case they're
                getting thrown away.

                If your plugin does want to send midi messages, you'll need to set
                the JucePlugin_ProducesMidiOutput macro to 1 in your
                JucePluginCharacteristics.h file.

                If you don't want to produce any midi output, then you should clear the
                midiMessages array at the end of your processBlock() method, to
                indicate that you don't want any of the events to be passed through
                to the output.
            */
            jassert (midiEvents.getNumEvents() <= numMidiEventsComingIn);
#endif

            midiEvents.clear();
        }
    }

    //==============================================================================
    VstInt32 startProcess () { return 0; }
    VstInt32 stopProcess () { return 0;}

    void resume()
    {
        if (filter == 0)
            return;

        isProcessing = true;
        juce_free (channels);
        channels = (float**) juce_calloc (sizeof (float*) * (numInChans + numOutChans));

        double rate = getSampleRate();
        jassert (rate > 0);
        if (rate <= 0.0)
            rate = 44100.0;

        const int blockSize = getBlockSize();
        jassert (blockSize > 0);

        firstProcessCallback = true;

        filter->setNonRealtime (getCurrentProcessLevel() == 4 /* kVstProcessLevelOffline */);

        filter->setPlayConfigDetails (numInChans, numOutChans,
                                      rate, blockSize);

        deleteTempChannels();

        filter->prepareToPlay (rate, blockSize);
        midiEvents.clear();

        setInitialDelay (filter->getLatencySamples());

        AudioEffectX::resume();

#if JucePlugin_ProducesMidiOutput
        ensureOutgoingEventSize (64);
#endif

#if JucePlugin_WantsMidiInput && ! JUCE_USE_VSTSDK_2_4
        wantEvents();
#endif
    }

    void suspend()
    {
        if (filter == 0)
            return;

        AudioEffectX::suspend();

        filter->releaseResources();
        midiEvents.clear();

        isProcessing = false;
        juce_free (channels);
        channels = 0;

        deleteTempChannels();
    }

    bool getCurrentPosition (AudioPlayHead::CurrentPositionInfo& info)
    {
        const VstTimeInfo* const ti = getTimeInfo (kVstPpqPosValid
                                                   | kVstTempoValid
                                                   | kVstBarsValid
                                                   //| kVstCyclePosValid
                                                   | kVstTimeSigValid
                                                   | kVstSmpteValid
                                                   | kVstClockValid);

        if (ti == 0 || ti->sampleRate <= 0)
            return false;

        if ((ti->flags & kVstTempoValid) != 0)
            info.bpm = ti->tempo;
        else
            info.bpm = 0.0;

        if ((ti->flags & kVstTimeSigValid) != 0)
        {
            info.timeSigNumerator = ti->timeSigNumerator;
            info.timeSigDenominator = ti->timeSigDenominator;
        }
        else
        {
            info.timeSigNumerator = 4;
            info.timeSigDenominator = 4;
        }

        info.timeInSeconds = ti->samplePos / ti->sampleRate;

        if ((ti->flags & kVstPpqPosValid) != 0)
            info.ppqPosition = ti->ppqPos;
        else
            info.ppqPosition = 0.0;

        if ((ti->flags & kVstBarsValid) != 0)
            info.ppqPositionOfLastBarStart = ti->barStartPos;
        else
            info.ppqPositionOfLastBarStart = 0.0;

        if ((ti->flags & kVstSmpteValid) != 0)
        {
            AudioPlayHead::FrameRateType rate = AudioPlayHead::fpsUnknown;
            double fps = 1.0;

            switch (ti->smpteFrameRate)
            {
            case kVstSmpte24fps:
                rate = AudioPlayHead::fps24;
                fps = 24.0;
                break;

            case kVstSmpte25fps:
                rate = AudioPlayHead::fps25;
                fps = 25.0;
                break;

            case kVstSmpte2997fps:
                rate = AudioPlayHead::fps2997;
                fps = 29.97;
                break;

            case kVstSmpte30fps:
                rate = AudioPlayHead::fps30;
                fps = 30.0;
                break;

            case kVstSmpte2997dfps:
                rate = AudioPlayHead::fps2997drop;
                fps = 29.97;
                break;

            case kVstSmpte30dfps:
                rate = AudioPlayHead::fps30drop;
                fps = 30.0;
                break;

            case kVstSmpteFilm16mm:
            case kVstSmpteFilm35mm:
                fps = 24.0;
                break;

            case kVstSmpte239fps:       fps = 23.976; break;
            case kVstSmpte249fps:       fps = 24.976; break;
            case kVstSmpte599fps:       fps = 59.94; break;
            case kVstSmpte60fps:        fps = 60; break;

            default:
                jassertfalse // unknown frame-rate..
            }

            info.frameRate = rate;
            info.editOriginTime = ti->smpteOffset / (80.0 * fps);
        }
        else
        {
            info.frameRate = AudioPlayHead::fpsUnknown;
            info.editOriginTime = 0;
        }

        info.isRecording = (ti->flags & kVstTransportRecording) != 0;
        info.isPlaying   = (ti->flags & kVstTransportPlaying) != 0 || info.isRecording;

        return true;
    }

    //==============================================================================
    VstInt32 getProgram()
    {
        return filter != 0 ? filter->getCurrentProgram() : 0;
    }

    void setProgram (VstInt32 program)
    {
        if (filter != 0)
            filter->setCurrentProgram (program);
    }

    void setProgramName (char* name)
    {
        if (filter != 0)
            filter->changeProgramName (filter->getCurrentProgram(), name);
    }

    void getProgramName (char* name)
    {
        if (filter != 0)
            filter->getProgramName (filter->getCurrentProgram()).copyToBuffer (name, 24);
    }

    bool getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
    {
        if (filter != 0 && ((unsigned int) index) < (unsigned int) filter->getNumPrograms())
        {
            filter->getProgramName (index).copyToBuffer (text, 24);
            return true;
        }

        return false;
    }

    //==============================================================================
    float getParameter (VstInt32 index)
    {
        if (filter == 0)
            return 0.0f;

        jassert (((unsigned int) index) < (unsigned int) filter->getNumParameters());
        return filter->getParameter (index);
    }

    void setParameter (VstInt32 index, float value)
    {
        if (filter != 0)
        {
            jassert (((unsigned int) index) < (unsigned int) filter->getNumParameters());
            filter->setParameter (index, value);
        }
    }

    void getParameterDisplay (VstInt32 index, char* text)
    {
        if (filter != 0)
        {
            jassert (((unsigned int) index) < (unsigned int) filter->getNumParameters());
            filter->getParameterText (index).copyToBuffer (text, 24); // length should technically be kVstMaxParamStrLen, which is 8, but hosts will normally allow a bit more.
        }
    }

    void getParameterName (VstInt32 index, char* text)
    {
        if (filter != 0)
        {
            jassert (((unsigned int) index) < (unsigned int) filter->getNumParameters());
            filter->getParameterName (index).copyToBuffer (text, 16); // length should technically be kVstMaxParamStrLen, which is 8, but hosts will normally allow a bit more.
        }
    }

    void audioProcessorParameterChanged (AudioProcessor*, int index, float newValue)
    {
        setParameterAutomated (index, newValue);
    }

    void audioProcessorParameterChangeGestureBegin (AudioProcessor*, int index)
    {
        beginEdit (index);
    }

    void audioProcessorParameterChangeGestureEnd (AudioProcessor*, int index)
    {
        endEdit (index);
    }

    void audioProcessorChanged (AudioProcessor*)
    {
        updateDisplay();
    }

    bool canParameterBeAutomated (VstInt32 index)
    {
        return filter != 0 && filter->isParameterAutomatable ((int) index);
    }

    bool setSpeakerArrangement (VstSpeakerArrangement* pluginInput,
                                VstSpeakerArrangement* pluginOutput)
    {
        // if this method isn't implemented, nuendo4 + cubase4 crash when you've got multiple channels..

        numInChans = pluginInput->numChannels;
        numOutChans = pluginOutput->numChannels;

        filter->setPlayConfigDetails (numInChans, numOutChans,
                                      filter->getSampleRate(),
                                      filter->getBlockSize());

        return true;
    }

    //==============================================================================
    VstInt32 getChunk (void** data, bool onlyStoreCurrentProgramData)
    {
        if (filter == 0)
            return 0;

        chunkMemory.setSize (0);
        if (onlyStoreCurrentProgramData)
            filter->getCurrentProgramStateInformation (chunkMemory);
        else
            filter->getStateInformation (chunkMemory);

        *data = (void*) chunkMemory;

        // because the chunk is only needed temporarily by the host (or at least you'd
        // hope so) we'll give it a while and then free it in the timer callback.
        chunkMemoryTime = JUCE_NAMESPACE::Time::getApproximateMillisecondCounter();

        return chunkMemory.getSize();
    }

    VstInt32 setChunk (void* data, VstInt32 byteSize, bool onlyRestoreCurrentProgramData)
    {
        if (filter == 0)
            return 0;

        chunkMemory.setSize (0);
        chunkMemoryTime = 0;

        if (byteSize > 0 && data != 0)
        {
            if (onlyRestoreCurrentProgramData)
                filter->setCurrentProgramStateInformation (data, byteSize);
            else
                filter->setStateInformation (data, byteSize);
        }

        return 0;
    }

    void timerCallback()
    {
        if (chunkMemoryTime > 0
             && chunkMemoryTime < JUCE_NAMESPACE::Time::getApproximateMillisecondCounter() - 2000
             && ! recursionCheck)
        {
            chunkMemoryTime = 0;
            chunkMemory.setSize (0);
        }

        tryMasterIdle();
    }

    void tryMasterIdle()
    {
        if (Component::isMouseButtonDownAnywhere()
             && ! recursionCheck)
        {
            const uint32 now = JUCE_NAMESPACE::Time::getMillisecondCounter();

            if (now > lastMasterIdleCall + 20 && editorComp != 0)
            {
                lastMasterIdleCall = now;

                recursionCheck = true;
                masterIdle();
                recursionCheck = false;
            }
        }
    }

    void doIdleCallback()
    {
        // (wavelab calls this on a separate thread and causes a deadlock)..
        if (MessageManager::getInstance()->isThisTheMessageThread()
             && ! recursionCheck)
        {
            const MessageManagerLock mml;

            recursionCheck = true;

            juce_callAnyTimersSynchronously();

            for (int i = ComponentPeer::getNumPeers(); --i >= 0;)
                ComponentPeer::getPeer (i)->performAnyPendingRepaintsNow();

            recursionCheck = false;
        }
    }

    void createEditorComp()
    {
        if (hasShutdown || filter == 0)
            return;

        if (editorComp == 0)
        {
#if JUCE_LINUX
            const MessageManagerLock mml;
#endif

            AudioProcessorEditor* const ed = filter->createEditorIfNeeded();

            if (ed != 0)
            {
                ed->setOpaque (true);
                ed->setVisible (true);

                editorComp = new EditorCompWrapper (this, ed);
            }
        }
    }

    void deleteEditor()
    {
        PopupMenu::dismissAllActiveMenus();

        jassert (! recursionCheck);
        recursionCheck = true;

#if JUCE_LINUX
        const MessageManagerLock mml;
#endif

        if (editorComp != 0)
        {
            Component* const modalComponent = Component::getCurrentlyModalComponent();
            if (modalComponent != 0)
                modalComponent->exitModalState (0);

            filter->editorBeingDeleted (editorComp->getEditorComp());

            deleteAndZero (editorComp);

            // there's some kind of component currently modal, but the host
            // is trying to delete our plugin. You should try to avoid this happening..
            jassert (Component::getCurrentlyModalComponent() == 0);
        }

#if JUCE_MAC || JUCE_LINUX
        hostWindow = 0;
#endif

        recursionCheck = false;
    }

    VstIntPtr dispatcher (VstInt32 opCode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
    {
        if (hasShutdown)
            return 0;

        if (opCode == effEditIdle)
        {
            doIdleCallback();
            return 0;
        }
        else if (opCode == effEditOpen)
        {
            jassert (! recursionCheck);

            deleteEditor();
            createEditorComp();

            if (editorComp != 0)
            {
#if JUCE_LINUX
                const MessageManagerLock mml;
#endif

                editorComp->setOpaque (true);
                editorComp->setVisible (false);

#if JUCE_WIN32
                editorComp->addToDesktop (0);

                hostWindow = (HWND) ptr;
                HWND editorWnd = (HWND) editorComp->getWindowHandle();

                SetParent (editorWnd, hostWindow);

                DWORD val = GetWindowLong (editorWnd, GWL_STYLE);
                val = (val & ~WS_POPUP) | WS_CHILD;
                SetWindowLong (editorWnd, GWL_STYLE, val);

                editorComp->setVisible (true);
#elif JUCE_LINUX
                editorComp->addToDesktop (0);

                hostWindow = (Window) ptr;

                Window editorWnd = (Window) editorComp->getWindowHandle();

                XReparentWindow (display, editorWnd, hostWindow, 0, 0);

                editorComp->setVisible (true);
#else
                hostWindow = (WindowRef) ptr;
                firstResize = true;

                SetAutomaticControlDragTrackingEnabledForWindow (hostWindow, true);

                WindowAttributes attributes;
                GetWindowAttributes (hostWindow, &attributes);

                HIViewRef parentView = 0;

                if ((attributes & kWindowCompositingAttribute) != 0)
                {
                    HIViewRef root = HIViewGetRoot (hostWindow);
                    HIViewFindByID (root, kHIViewWindowContentID, &parentView);

                    if (parentView == 0)
                        parentView = root;
                }
                else
                {
                    GetRootControl (hostWindow, (ControlRef*) &parentView);

                    if (parentView == 0)
                        CreateRootControl (hostWindow, (ControlRef*) &parentView);
                }

                jassert (parentView != 0); // agh - the host has to provide a compositing window..

                editorComp->setVisible (true);
                editorComp->addToDesktop (0, (void*) parentView);
#endif

                return 1;
            }
        }
        else if (opCode == effEditClose)
        {
            deleteEditor();
            return 0;
        }
        else if (opCode == effEditGetRect)
        {
            createEditorComp();

            if (editorComp != 0)
            {
                editorSize.left = 0;
                editorSize.top = 0;
                editorSize.right = editorComp->getWidth();
                editorSize.bottom = editorComp->getHeight();

                *((ERect**) ptr) = &editorSize;

                return (VstIntPtr) &editorSize;
            }
            else
            {
                return 0;
            }
        }

        return AudioEffectX::dispatcher (opCode, index, value, ptr, opt);
    }

    void resizeHostWindow (int newWidth, int newHeight)
    {
        if (editorComp != 0)
        {
#if ! JUCE_LINUX // linux hosts shouldn't be trusted!
            if (! (canHostDo ("sizeWindow") && sizeWindow (newWidth, newHeight)))
#endif
            {
                // some hosts don't support the sizeWindow call, so do it manually..
#if JUCE_MAC
                Rect r;
                GetWindowBounds (hostWindow, kWindowContentRgn, &r);

                if (firstResize)
                {
                    diffW = (r.right - r.left) - editorComp->getWidth();
                    diffH = (r.bottom - r.top) - editorComp->getHeight();
                    firstResize = false;
                }

                r.right = r.left + newWidth + diffW;
                r.bottom = r.top + newHeight + diffH;

                SetWindowBounds (hostWindow, kWindowContentRgn, &r);

                r.bottom -= r.top;
                r.right -= r.left;
                r.left = r.top = 0;
                InvalWindowRect (hostWindow, &r);
#elif JUCE_LINUX
                Window root;
                int x, y;
                unsigned int width, height, border, depth;

                XGetGeometry (display, hostWindow, &root,
                              &x, &y, &width, &height, &border, &depth);

                newWidth += (width + border) - editorComp->getWidth();
                newHeight += (height + border) - editorComp->getHeight();

                XResizeWindow (display, hostWindow, newWidth, newHeight);
#else
                int dw = 0;
                int dh = 0;
                const int frameThickness = GetSystemMetrics (SM_CYFIXEDFRAME);

                HWND w = (HWND) editorComp->getWindowHandle();

                while (w != 0)
                {
                    HWND parent = GetParent (w);

                    if (parent == 0)
                        break;

                    TCHAR windowType [32];
                    zeromem (windowType, sizeof (windowType));
                    GetClassName (parent, windowType, 31);

                    if (String (windowType).equalsIgnoreCase (T("MDIClient")))
                        break;

                    RECT windowPos;
                    GetWindowRect (w, &windowPos);

                    RECT parentPos;
                    GetWindowRect (parent, &parentPos);

                    SetWindowPos (w, 0, 0, 0,
                                  newWidth + dw,
                                  newHeight + dh,
                                  SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);

                    dw = (parentPos.right - parentPos.left) - (windowPos.right - windowPos.left);
                    dh = (parentPos.bottom - parentPos.top) - (windowPos.bottom - windowPos.top);

                    w = parent;

                    if (dw == 2 * frameThickness)
                        break;

                    if (dw > 100 || dh > 100)
                        w = 0;
                }

                if (w != 0)
                    SetWindowPos (w, 0, 0, 0,
                                  newWidth + dw,
                                  newHeight + dh,
                                  SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
#endif
            }

            if (editorComp->getPeer() != 0)
                editorComp->getPeer()->handleMovedOrResized();
        }
    }


    //==============================================================================
    juce_UseDebuggingNewOperator

private:
    AudioProcessor* filter;
    juce::MemoryBlock chunkMemory;
    uint32 chunkMemoryTime;
    EditorCompWrapper* editorComp;
    ERect editorSize;
    MidiBuffer midiEvents;
    VstEvents* outgoingEvents;
    int outgoingEventSize;
    bool isProcessing;
    bool firstResize;
    bool hasShutdown;
    bool firstProcessCallback;
    int diffW, diffH;
    int numInChans, numOutChans;
    float** channels;
    VoidArray tempChannels; // see note in processReplacing()
    bool hasCreatedTempChannels;

    void deleteTempChannels()
    {
        int i;
        for (i = tempChannels.size(); --i >= 0;)
            juce_free (tempChannels.getUnchecked(i));

        tempChannels.clear();

        if (filter != 0)
            tempChannels.insertMultiple (0, 0, filter->getNumInputChannels() + filter->getNumOutputChannels());

        hasCreatedTempChannels = false;
    }

    void ensureOutgoingEventSize (int numEvents)
    {
        if (outgoingEventSize < numEvents)
        {
            numEvents += 32;
            const int size = 16 + sizeof (VstEvent*) * numEvents;

            if (outgoingEvents == 0)
                outgoingEvents = (VstEvents*) juce_calloc (size);
            else
                outgoingEvents = (VstEvents*) juce_realloc (outgoingEvents, size);

            for (int i = outgoingEventSize; i < numEvents; ++i)
            {
                VstMidiEvent* const e = (VstMidiEvent*) juce_calloc (sizeof (VstMidiEvent));
                e->type = kVstMidiType;
                e->byteSize = 24;

                outgoingEvents->events[i] = (VstEvent*) e;
            }

            outgoingEventSize = numEvents;
        }
    }

    const String getHostName()
    {
        char host[256];
        zeromem (host, sizeof (host));
        getHostProductString (host);
        return host;
    }

#if JUCE_MAC
    WindowRef hostWindow;
#elif JUCE_LINUX
    Window hostWindow;
#else
    HWND hostWindow;
#endif
};

//==============================================================================
void EditorCompWrapper::childBoundsChanged (Component* child)
{
    child->setTopLeftPosition (0, 0);

    const int cw = child->getWidth();
    const int ch = child->getHeight();

    wrapper->resizeHostWindow (cw, ch);
    setSize (cw, ch);

#if JUCE_MAC
    wrapper->resizeHostWindow (cw, ch);  // (doing this a second time seems to be necessary in tracktion)
#endif
}

void EditorCompWrapper::handleAsyncUpdate()
{
    wrapper->tryMasterIdle();
}

//==============================================================================
/** Somewhere in the codebase of your plugin, you need to implement this function
    and make it create an instance of the filter subclass that you're building.
*/
extern AudioProcessor* JUCE_CALLTYPE createPluginFilter();


//==============================================================================
static AEffect* pluginEntryPoint (audioMasterCallback audioMaster)
{
    initialiseJuce_GUI();

#if JUCE_MAC && defined (JucePlugin_CFBundleIdentifier)
    juce_setCurrentExecutableFileNameFromBundleId (JucePlugin_CFBundleIdentifier);
#endif

    MessageManager::getInstance()->setTimeBeforeShowingWaitCursor (0);

    try
    {
        if (audioMaster (0, audioMasterVersion, 0, 0, 0, 0) != 0)
        {
            AudioProcessor* const filter = createPluginFilter();

            if (filter != 0)
            {
                JuceVSTWrapper* const wrapper = new JuceVSTWrapper (audioMaster, filter);
                return wrapper->getAeffect();
            }
        }
    }
    catch (...)
    {}

    return 0;
}


//==============================================================================
// Mac startup code..
#if JUCE_MAC

extern "C" __attribute__ ((visibility("default"))) AEffect* VSTPluginMain (audioMasterCallback audioMaster)
{
    return pluginEntryPoint (audioMaster);
}

extern "C" __attribute__ ((visibility("default"))) AEffect* main_macho (audioMasterCallback audioMaster)
{
    return pluginEntryPoint (audioMaster);
}

//==============================================================================
// Linux startup code..
#elif JUCE_LINUX

extern "C" AEffect* VSTPluginMain (audioMasterCallback audioMaster)
{
    initialiseJuce_GUI();
    SharedMessageThread::getInstance();

    return pluginEntryPoint (audioMaster);
}

extern "C" __attribute__ ((visibility("default"))) AEffect* main_plugin (audioMasterCallback audioMaster) asm ("main");

extern "C" __attribute__ ((visibility("default"))) AEffect* main_plugin (audioMasterCallback audioMaster)
{
    return VSTPluginMain (audioMaster);
}

__attribute__((constructor)) void myPluginInit()
{
    // don't put initialiseJuce_GUI here... it will crash !
}

__attribute__((destructor)) void myPluginFini()
{
    // don't put shutdownJuce_GUI here... it will crash !
}

//==============================================================================
// Win32 startup code..
#else

extern "C" __declspec (dllexport) AEffect* VSTPluginMain (audioMasterCallback audioMaster)
{
    return pluginEntryPoint (audioMaster);
}

extern "C" __declspec (dllexport) void* main (audioMasterCallback audioMaster)
{
    return (void*) pluginEntryPoint (audioMaster);
}

BOOL WINAPI DllMain (HINSTANCE instance, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH)
        PlatformUtilities::setCurrentModuleInstanceHandle (instance);

    return TRUE;
}

#endif
