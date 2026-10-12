#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/Music.h"
class ProPreviewSound:public juce::SynthesiserSound{public:bool appliesToNote(int)override{return true;}bool appliesToChannel(int)override{return true;}};
class ProPreviewVoice:public juce::SynthesiserVoice{
public:
 bool canPlaySound(juce::SynthesiserSound*s)override{return dynamic_cast<ProPreviewSound*>(s)!=nullptr;}
 void setTimbre(ccp::Timbre t){nextTimbre=t;}
 void startNote(int,float,juce::SynthesiserSound*,int)override;
 void stopNote(float,bool)override;
 void renderNextBlock(juce::AudioBuffer<float>&,int,int)override;
 void pitchWheelMoved(int)override{}
 void controllerMoved(int,int)override{}
private:
 // Sine oscillator advanced by complex rotation, with a per-sample exponential decay.
 struct Partial{double re{},im{},cr{1},ci{};float amp{},decay{1};
  void set(double w,float a,float d){re=1;im=0;cr=std::cos(w);ci=std::sin(w);amp=a;decay=d;}
  float tick(){const double r=re*cr-im*ci;im=re*ci+im*cr;re=r;const float out=(float)im*amp;amp*=decay;return out;}
  void renormalise(){const double m=1.0/std::sqrt(re*re+im*im);re*=m;im*=m;}};
 float renderRhodes();float renderOrgan(float&pan);float renderSaw(float&left,float&right);
 float noise(){return rng.nextFloat()*2.0f-1.0f;}
 void updateFilter();
 ccp::Timbre timbre{ccp::Timbre::electricPiano},nextTimbre{ccp::Timbre::electricPiano};
 std::array<Partial,16> partials{};int partialCount{};
 double hz{},sr{44100},phase{},age{};double sawPhase[3]{},sawInc[3]{};
 float level{},velocity{},pan{},clickAmp{},clickDecay{},clickState{};
 float svfIc1[2]{},svfIc2[2]{},svfA1{},svfA2{},svfA3{},svfK{};
 int counter{};
 juce::Random rng;juce::ADSR env;
};
class PreviewSynth{
public:
 PreviewSynth();void prepare(double,int maximumBlockSize=512);void render(juce::AudioBuffer<float>&,juce::MidiBuffer&);void allNotesOff(bool allowTailOff=true);bool isActive() const;void setTimbre(ccp::Timbre);
private:
 static void addSample(juce::Synthesiser&,const char*,const void*,int,int);
 static void addPiano(juce::Synthesiser&);
 juce::Synthesiser procedural,piano,pad,violin,flute;
 juce::AudioBuffer<float> scratch;
 juce::MidiBuffer chunkEvents;
 ccp::Timbre current{ccp::Timbre::piano};
};
