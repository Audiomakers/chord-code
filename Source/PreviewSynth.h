#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/Music.h"
class ProPreviewSound:public juce::SynthesiserSound{public:bool appliesToNote(int)override{return true;}bool appliesToChannel(int)override{return true;}};
class ProPreviewVoice:public juce::SynthesiserVoice{public:bool canPlaySound(juce::SynthesiserSound*s)override{return dynamic_cast<ProPreviewSound*>(s)!=nullptr;}void setTimbre(ccp::Timbre t){nextTimbre=t;}void startNote(int,float,juce::SynthesiserSound*,int)override;void stopNote(float,bool)override;void renderNextBlock(juce::AudioBuffer<float>&,int,int)override;void pitchWheelMoved(int)override{}void controllerMoved(int,int)override{}private:ccp::Timbre timbre{ccp::Timbre::electricPiano},nextTimbre{ccp::Timbre::electricPiano};double phase{},delta{},phase2{},age{};float level{},filterL{},filterR{};juce::ADSR env;};
class PreviewSynth{
public:
 PreviewSynth();void prepare(double,int maximumBlockSize=512);void render(juce::AudioBuffer<float>&,juce::MidiBuffer&);void allNotesOff(bool allowTailOff=true);bool isActive() const;void setTimbre(ccp::Timbre);
private:
 static void addSample(juce::Synthesiser&,const char*,const void*,int,int);
 juce::Synthesiser procedural,pad,violin,flute;
 juce::AudioBuffer<float> scratch;
 juce::MidiBuffer chunkEvents;
 ccp::Timbre current{ccp::Timbre::electricPiano};
};
