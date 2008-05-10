/*
  ==============================================================================

  This is an automatically generated file created by the Jucer!

  Creation date:  28 Mar 2008 9:13:40 pm

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Jucer version: 1.11

  ------------------------------------------------------------------------------

  The Jucer is part of the JUCE library - "Jules' Utility Class Extensions"
  Copyright 2004-6 by Raw Material Software ltd.

  ==============================================================================
*/

#ifndef __JUCER_HEADER_KITTYEDITOR_KITTYEDITOR_1AF9F8A5__
#define __JUCER_HEADER_KITTYEDITOR_KITTYEDITOR_1AF9F8A5__

//[Headers]     -- You can add your own extra header files here --
#include <juce.h>
#include "kitty.h"
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Jucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class kittyEditor  : public AudioProcessorEditor,
                     public ChangeListener,
                     public SliderListener
{
public:
    //==============================================================================
    kittyEditor (kitty *owner);
    ~kittyEditor();

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void changeListenerCallback (void* source);
    kitty* getFilter() const throw()       { return (kitty*) getAudioProcessor(); }
    //[/UserMethods]

    void paint (Graphics& g);
    void resized();
    void sliderValueChanged (Slider* sliderThatWasMoved);

    // Binary resources:
    static const char* kitty_png;
    static const int kitty_pngSize;

    //==============================================================================
    juce_UseDebuggingNewOperator

private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    Slider* bitDepthSlider;
    Label* bitDepthLabel;
    Slider* sampleRateSlider;
    Label* sampleRateLabel;
    Path internalPath1;
    Image* internalCachedImage3;

    //==============================================================================
    // (prevent copy constructor and operator= being generated..)
    kittyEditor (const kittyEditor&);
    const kittyEditor& operator= (const kittyEditor&);
};


#endif   // __JUCER_HEADER_KITTYEDITOR_KITTYEDITOR_1AF9F8A5__
