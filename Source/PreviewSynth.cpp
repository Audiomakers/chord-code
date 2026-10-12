#include "PreviewSynth.h"
#include <BinaryData.h>
#include <cmath>

namespace {
// Per-timbre gains measured so every preview instrument plays a chord at the
// same loudness (RMS of a C major triad over its first second).
float normalisationGain(ccp::Timbre timbre){
 switch(timbre){
  case ccp::Timbre::electricPiano:return .938f;
  case ccp::Timbre::synthSaw:return 3.410f;
  case ccp::Timbre::organ:return 1.820f;
  case ccp::Timbre::piano:return 1.900f;
  case ccp::Timbre::atmosphericPad:return 0.610f;
  case ccp::Timbre::violin:return 0.553f;
  case ccp::Timbre::flute:return 0.834f;
  case ccp::Timbre::count:return 1.0f;
 }
 return 1.0f;
}

float perSampleDecay(double seconds,double sampleRate){return (float)std::exp(-1.0/(juce::jmax(0.001,seconds)*sampleRate));}

float polyBlepSaw(double t,double dt){
 double v=2.0*t-1.0;
 if(t<dt){const double x=t/dt;v-=x+x-x*x-1.0;}
 else if(t>1.0-dt){const double x=(t-1.0)/dt;v-=x*x+x+x+1.0;}
 return (float)v;
}
}

void ProPreviewVoice::startNote(int note,float vel,juce::SynthesiserSound*,int){
 timbre=nextTimbre;sr=getSampleRate();
 hz=juce::MidiMessage::getMidiNoteInHertz(note);
 phase=age=0;counter=0;clickState=0;
 velocity=juce::jlimit(0.05f,1.0f,vel);
 level=(.30f+.70f*velocity)*.11f*normalisationGain(timbre);
 pan=juce::jlimit(-.6f,.6f,(float)(note-60)/40.0f);
 for(auto&partial:partials)partial=Partial{};
 partialCount=0;
 for(int c=0;c<2;++c)svfIc1[c]=svfIc2[c]=0;
 const double nyquistGuard=.45*sr;
 const auto addPartial=[&](double freq,float amp,double decaySeconds){
  if(partialCount>=(int)partials.size()||freq>=nyquistGuard)return;
  partials[(size_t)partialCount++].set(juce::MathConstants<double>::twoPi*freq/sr,amp,perSampleDecay(decaySeconds,sr));
 };
 juce::ADSR::Parameters p;
 switch(timbre){
  case ccp::Timbre::piano:{
   // Slightly inharmonic string partials; upper partials die away faster,
   // and a second detuned string on the lower partials adds natural beating.
   const double inharmonicity=.00028*std::pow(2.0,(note-60)/24.0);
   const double sustain=juce::jlimit(.9,7.0,4.8*std::pow(2.0,-(note-48)/18.0));
   const float brightness=.42f+.40f*velocity;
   for(int n=1;n<=12;++n){
    const double f=hz*n*std::sqrt(1.0+inharmonicity*n*n);
    const float amp=(float)(std::pow(brightness,(float)(n-1))/std::pow((double)n,.9));
    addPartial(f,amp*(n<=4?.62f:1.0f),sustain/(1.0+.55*(n-1)));
    if(n<=4)addPartial(f*1.0011,amp*.38f,sustain*.85/(1.0+.55*(n-1)));
   }
   clickAmp=.10f*velocity;clickDecay=perSampleDecay(.004,sr);
   p={.001f,.0f,1.0f,.32f};break;
  }
  case ccp::Timbre::organ:{
   // Drawbars 16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1'.
   static constexpr double ratios[]{.5,1.0,1.5,2.0,3.0,4.0,5.0,8.0};
   static constexpr float bars[]{.55f,1.0f,.55f,.42f,.24f,.30f,.10f,.16f};
   for(size_t i=0;i<8;++i)addPartial(hz*ratios[i],bars[i]*.62f,1.0e6);
   clickAmp=.07f;clickDecay=perSampleDecay(.003,sr);
   p={.004f,.0f,1.0f,.07f};break;
  }
  case ccp::Timbre::synthSaw:{
   static constexpr double cents[]{0.0,8.0,-8.0};
   for(int i=0;i<3;++i){sawInc[i]=hz*std::pow(2.0,cents[i]/1200.0)/sr;sawPhase[i]=rng.nextDouble();}
   updateFilter();
   p={.012f,.35f,.82f,.30f};break;
  }
  case ccp::Timbre::electricPiano:
  case ccp::Timbre::atmosphericPad:case ccp::Timbre::violin:case ccp::Timbre::flute:case ccp::Timbre::count:
  default:
   p={.002f,.0f,1.0f,.45f};break;
 }
 env.setSampleRate(sr);env.setParameters(p);env.noteOn();
}

void ProPreviewVoice::stopNote(float,bool tail){env.noteOff();if(!tail){env.reset();clearCurrentNote();}}

float ProPreviewVoice::renderPiano(){
 float x=0;
 for(int i=0;i<partialCount;++i)x+=partials[(size_t)i].tick();
 // Hammer: a short low-passed noise thump.
 clickState+=.25f*(noise()*clickAmp-clickState);clickAmp*=clickDecay;
 return x*.55f+clickState;
}

float ProPreviewVoice::renderRhodes(){
 // Tine and tonebar: FM with a decaying index for the bell-like attack.
 const double t=age;
 const double index=(.35+1.9*velocity)*std::exp(-t*5.5)+.18*velocity;
 const double body=std::exp(-t*juce::jlimit(.35,2.2,.55*hz/220.0));
 const double tineFreq=hz*7.0;
 double tine=0;
 if(tineFreq<.45*sr)tine=.16*velocity*std::exp(-t*30.0)*std::sin(phase*7.0);
 const double y=std::sin(phase+index*std::sin(phase))*body+tine;
 // Gentle bark that grows with velocity.
 const double drive=1.0+1.6*velocity;
 return (float)(std::tanh(y*drive)/std::tanh(drive))*.95f;
}

float ProPreviewVoice::renderOrgan(float& panOut){
 float x=0;
 for(int i=0;i<partialCount;++i)x+=partials[(size_t)i].tick();
 clickState+=.6f*(noise()*clickAmp-clickState);clickAmp*=clickDecay;
 // Rotary speaker: a quick tremolo that sweeps across the stereo field.
 const double rotor=std::sin(age*juce::MathConstants<double>::twoPi*5.7);
 panOut=(float)(.35*rotor);
 return (x*(float)(.90+.10*rotor))*.55f+clickState;
}

void ProPreviewVoice::updateFilter(){
 const double sweep=std::exp(-age*3.2);
 const double cutoff=juce::jlimit(180.0,.42*sr,hz*(2.2+(4.0+9.0*velocity)*sweep));
 const double g=std::tan(juce::MathConstants<double>::pi*cutoff/sr);
 svfK=1.0f/.85f;
 svfA1=(float)(1.0/(1.0+g*(g+svfK)));svfA2=(float)g*svfA1;svfA3=(float)g*svfA2;
}

float ProPreviewVoice::renderSaw(float& left,float& right){
 float osc[3];
 for(int i=0;i<3;++i){
  osc[i]=polyBlepSaw(sawPhase[i],sawInc[i]);
  sawPhase[i]+=sawInc[i];if(sawPhase[i]>=1.0)sawPhase[i]-=1.0;
 }
 const float sub=(float)std::sin(phase*.5)*.35f;
 const float in[2]{osc[0]*.5f+osc[1]*.62f+osc[2]*.28f+sub,osc[0]*.5f+osc[1]*.28f+osc[2]*.62f+sub};
 float out[2];
 for(int c=0;c<2;++c){
  const float v3=in[c]-svfIc2[c];
  const float v1=svfA1*svfIc1[c]+svfA2*v3;
  const float v2=svfIc2[c]+svfA2*svfIc1[c]+svfA3*v3;
  svfIc1[c]=2.0f*v1-svfIc1[c];svfIc2[c]=2.0f*v2-svfIc2[c];
  out[c]=v2;
 }
 left=out[0]*.42f;right=out[1]*.42f;
 return 0.0f;
}

void ProPreviewVoice::renderNextBlock(juce::AudioBuffer<float>&b,int start,int count){
 const double delta=juce::MathConstants<double>::twoPi*hz/sr;
 while(count--){
  float left=0,right=0,voicePan=pan;
  switch(timbre){
   case ccp::Timbre::piano:left=right=renderPiano();break;
   case ccp::Timbre::organ:{float rotor=0;left=right=renderOrgan(rotor);voicePan=juce::jlimit(-.8f,.8f,pan*.5f+rotor);break;}
   case ccp::Timbre::synthSaw:renderSaw(left,right);voicePan=0;break;
   case ccp::Timbre::electricPiano:left=right=renderRhodes();break;
   case ccp::Timbre::atmosphericPad:case ccp::Timbre::violin:case ccp::Timbre::flute:case ccp::Timbre::count:
   default:break;
  }
  const float gain=level*env.getNextSample();
  const float panL=std::sqrt(.5f*(1.0f-voicePan)),panR=std::sqrt(.5f*(1.0f+voicePan));
  if(b.getNumChannels()>0)b.addSample(0,start,left*gain*panL*1.41421356f);
  if(b.getNumChannels()>1)b.addSample(1,start,right*gain*panR*1.41421356f);
  ++start;
  phase+=delta;if(phase>juce::MathConstants<double>::twoPi*2.0)phase-=juce::MathConstants<double>::twoPi*2.0;
  age+=1.0/sr;
  if(++counter>=32){
   counter=0;
   for(int i=0;i<partialCount;++i)partials[(size_t)i].renormalise();
   if(timbre==ccp::Timbre::synthSaw)updateFilter();
  }
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
