#ifndef __JUCE_PLUGIN_CHARACTERISTICS_H__
#define __JUCE_PLUGIN_CHARACTERISTICS_H__

#define JucePlugin_Name                     "kitty decimator"
#define JucePlugin_Desc                     "a simple decimator written in juce"
#define JucePlugin_Manufacturer             "sndlab.com"
#define JucePlugin_ManufacturerCode         'SndL'
#define JucePlugin_PluginCode               'kitt'
#define JucePlugin_MaxNumInputChannels              2
#define JucePlugin_MaxNumOutputChannels             2
#define JucePlugin_PreferredChannelConfigurations   { 1, 1 }, { 2, 2 }
#define JucePlugin_IsSynth                          0
#define JucePlugin_WantsMidiInput                   1
#define JucePlugin_ProducesMidiOutput               1
#define JucePlugin_SilenceInProducesSilenceOut      0
#define JucePlugin_EditorRequiresKeyboardFocus      1
#define JucePlugin_VersionCode              0x00010100
#define JucePlugin_VersionString            "1.1"
#define JUCE_USE_VSTSDK_2_4                 1
#define JucePlugin_VSTUniqueID              JucePlugin_PluginCode
#if JucePlugin_IsSynth
  #define JucePlugin_VSTCategory            kPlugCategSynth
#else
  #define JucePlugin_VSTCategory            kPlugCategEffect
#endif

#endif
