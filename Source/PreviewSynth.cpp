#include "PreviewSynth.h"
#include <BinaryData.h>
#include <cmath>

namespace {
float normalisationGain(ccp::Timbre timbre){
 switch(timbre){
  case ccp::Timbre::electricPiano:return 1.00f;
  case ccp::Timbre::synthSaw:return 1.65f;
  case ccp::Timbre::organ:return 1.15f;
  case ccp::Timbre::atmosphericPad:return .60f;
  case ccp::Timbre::violin:return .70f;
  case ccp::Timbre::flute:return .88f;
  case ccp::Timbre::count:return 1.0f;
 }
 return 1.0f;
}
}

void ProPreviewVoice::startNote(int note,float velocity,juce::SynthesiserSound*,int){
 timbre=nextTimbre;
 phase=phase2=age=0;filterL=filterR=0;
 delta=juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(note)/getSampleRate();
 level=velocity*.085f*normalisationGain(timbre);juce::ADSR::Parameters p;
 switch(timbre){
  case ccp::Timbre::synthSaw:p={.008f,.16f,.72f,.38f};break;
  case ccp::Timbre::organ:p={.015f,.22f,.78f,.28f};break;
  case ccp::Timbre::electricPiano:p={.006f,.62f,.24f,.7f};break;
  case ccp::Timbre::atmosphericPad:case ccp::Timbre::violin:case ccp::Timbre::flute:case ccp::Timbre::count:
  default:p={.01f,.3f,.5f,.4f};break;
 }
 env.setSampleRate(getSampleRate());env.setParameters(p);env.noteOn();
}
void ProPreviewVoice::stopNote(float,bool tail){env.noteOff();if(!tail){env.reset();clearCurrentNote();}}
void ProPreviewVoice::renderNextBlock(juce::AudioBuffer<float>&b,int start,int count){
 while(count--){float raw=0;
  switch(timbre){
   case ccp::Timbre::synthSaw:{
    phase2+=delta*1.0045;
    for(int h=1;h<=16;++h)raw+=(float)(std::sin(phase*h)/h);
    for(int h=1;h<=12;++h)raw+=(float)(.32*std::sin(phase2*h)/h);
    raw*=.43f;break;
   }
   case ccp::Timbre::organ:{ const double tremolo=.92+.08*std::sin(age*5.6); raw=(float)(tremolo*(.58*std::sin(phase)+.44*std::sin(phase*2)+.31*std::sin(phase*3)+.22*std::sin(phase*4)+.14*std::sin(phase*8))); break; }
   case ccp::Timbre::electricPiano:{double d=std::exp(-age*1.8),attack=std::exp(-age*18.0);raw=(float)(.70*std::sin(phase)+.32*std::sin(phase*2.01)*d+.18*std::sin(phase*3.99)*d+.16*std::sin(phase*7.03)*attack);break;}
   case ccp::Timbre::atmosphericPad:case ccp::Timbre::violin:case ccp::Timbre::flute:case ccp::Timbre::count:
   default:break;
  }
  auto x=raw*level*env.getNextSample();phase+=delta;age+=1.0/getSampleRate();
  if(phase>juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
  if(phase2>juce::MathConstants<double>::twoPi)phase2-=juce::MathConstants<double>::twoPi;
  filterL+=.18f*(x-filterL);filterR+=.16f*(x-filterR);
  if(b.getNumChannels()>0)b.addSample(0,start,filterL);if(b.getNumChannels()>1)b.addSample(1,start,filterR);++start;
  if(!env.isActive()){clearCurrentNote();break;}
 }
}

void PreviewSynth::addSample(juce::Synthesiser& synth,const char* name,const void* data,int size,int rootNote){
 for(int i=0;i<32;++i)synth.addVoice(new juce::SamplerVoice);
 juce::WavAudioFormat wav;
 auto stream=std::make_unique<juce::MemoryInputStream>(data,(size_t)size,false);
 if(auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(stream.release(),true))){
  juce::BigInteger notes;notes.setRange(0,128,true);
  synth.addSound(new juce::SamplerSound(name,*reader,notes,rootNote,.008,.55,30.0));
 }
}

PreviewSynth::PreviewSynth(){
 for(int i=0;i<48;++i)procedural.addVoice(new ProPreviewVoice);
 procedural.addSound(new ProPreviewSound);
 addSample(pad,"Pad C4",BinaryData::pad_C4_wav,BinaryData::pad_C4_wavSize,60);
 addSample(violin,"Violino F4",BinaryData::violin_F4_wav,BinaryData::violin_F4_wavSize,65);
 addSample(flute,"Flauta F#4",BinaryData::flute_Fsharp4_wav,BinaryData::flute_Fsharp4_wavSize,66);
 setTimbre(current);
}
void PreviewSynth::prepare(double sr,int maximumBlockSize){
 for(auto*s:{&procedural,&pad,&violin,&flute}){
  s->allNotesOff(0,false);
  s->setCurrentPlaybackSampleRate(sr);
  s->setMinimumRenderingSubdivisionSize(1,true);
 }
 scratch.setSize(2,juce::jmax(1,maximumBlockSize));
 chunkEvents.ensureSize(16384);
}
void PreviewSynth::render(juce::AudioBuffer<float>&b,juce::MidiBuffer&m){
 auto* selected=&procedural;
 if(current==ccp::Timbre::atmosphericPad)selected=&pad;else if(current==ccp::Timbre::violin)selected=&violin;else if(current==ccp::Timbre::flute)selected=&flute;
 // Keep rendering released voices even when the selected instrument changes.
 for(int offset=0;offset<b.getNumSamples();offset+=scratch.getNumSamples()){
  const int count=juce::jmin(scratch.getNumSamples(),b.getNumSamples()-offset);
  for(auto*s:{&procedural,&pad,&violin,&flute}){
   scratch.clear();chunkEvents.clear();
   if(s==selected)chunkEvents.addEvents(m,offset,count,-offset);
   s->renderNextBlock(scratch,chunkEvents,0,count);
   const float gain=s==&pad?normalisationGain(ccp::Timbre::atmosphericPad):s==&violin?normalisationGain(ccp::Timbre::violin):s==&flute?normalisationGain(ccp::Timbre::flute):1.0f;
   for(int c=0;c<juce::jmin(2,b.getNumChannels());++c)b.addFrom(c,offset,scratch,c,0,count,gain);
  }
 }
 for(int c=0;c<b.getNumChannels();++c)for(int i=0;i<b.getNumSamples();++i)b.setSample(c,i,std::tanh(b.getSample(c,i)*.82f));
}
bool PreviewSynth::isActive() const{
 for(auto*s:{&procedural,&pad,&violin,&flute})
  for(int i=0;i<s->getNumVoices();++i)if(s->getVoice(i)->isVoiceActive())return true;
 return false;
}
void PreviewSynth::allNotesOff(bool allowTailOff){for(auto*s:{&procedural,&pad,&violin,&flute})s->allNotesOff(0,allowTailOff);}
void PreviewSynth::setTimbre(ccp::Timbre t){
 allNotesOff();current=t;
 for(int i=0;i<procedural.getNumVoices();++i)if(auto*v=dynamic_cast<ProPreviewVoice*>(procedural.getVoice(i)))v->setTimbre(t);
}
