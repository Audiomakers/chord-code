#include "MidiFile.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace ccp {namespace {
void be16(std::vector<uint8_t>&o,uint16_t x){o.push_back(x>>8);o.push_back(x);}void be32(std::vector<uint8_t>&o,uint32_t x){o.push_back(x>>24);o.push_back(x>>16);o.push_back(x>>8);o.push_back(x);}void vlq(std::vector<uint8_t>&o,uint32_t v){uint8_t b[5];int n=0;b[n++]=v&127;while((v>>=7))b[n++]=128|(v&127);while(n)o.push_back(b[--n]);}
struct Event{int tick;bool on;int note;int velocity;};
std::vector<uint8_t>encode(std::vector<Event>events,const MidiOptions&){std::sort(events.begin(),events.end(),[](const Event&a,const Event&b){if(a.tick!=b.tick)return a.tick<b.tick;return a.on<b.on;});std::vector<uint8_t>tr;int last=0;for(const auto&e:events){vlq(tr,(uint32_t)std::max(0,e.tick-last));last=e.tick;tr.push_back(e.on?0x90:0x80);tr.push_back((uint8_t)e.note);tr.push_back((uint8_t)(e.on?e.velocity:0));}vlq(tr,0);tr.insert(tr.end(),{0xff,0x2f,0});std::vector<uint8_t>out{'M','T','h','d'};be32(out,6);be16(out,0);be16(out,1);be16(out,MidiFile::ppq);out.insert(out.end(),{'M','T','r','k'});be32(out,(uint32_t)tr.size());out.insert(out.end(),tr.begin(),tr.end());return out;}
std::vector<int>pulseOffsets(Style style,int length){if(style==Style::house||style==Style::edm){std::vector<int>x;for(int t=0;t<length;t+=MidiFile::ppq)x.push_back(t);return x;}if(style==Style::latin||style==Style::funkBrasileiro)return{0,std::min(length-1,MidiFile::ppq+MidiFile::ppq/2),std::min(length-1,MidiFile::ppq*3)};if(style==Style::trap||style==Style::hipHop||style==Style::phonk)return{0,std::min(length-1,MidiFile::ppq*3)};return{0,std::min(length-1,MidiFile::ppq*2)};}
bool writeBytes(const std::vector<uint8_t>&b,const std::filesystem::path&f){std::ofstream s(f,std::ios::binary);s.write((const char*)b.data(),(std::streamsize)b.size());return s.good();}
}
double chordLengthBeats(const MidiOptions&o,int index){
 const bool first=index%2==0;
 switch(o.groove){case Groove::longShort:return o.beatsPerChord*(first?1.5:0.5);case Groove::shortLong:return o.beatsPerChord*(first?0.5:1.5);default:return o.beatsPerChord;}
}
double chordStartBeats(const MidiOptions&o,int index){return o.beatsPerChord*(index-index%2)+(index%2?chordLengthBeats(o,index-1):0.0);}
std::vector<uint8_t>MidiFile::bytes(const Progression&p,const MidiOptions&o){
 std::vector<Event>events;
 for(int ci=0;ci<p.chordCount;++ci){const auto&c=p.chords[(size_t)ci];const double base=chordStartBeats(o,ci);
  for(const auto&n:arrangeChord(c,o.rhythm,chordLengthBeats(o,ci),ci,o.velocity)){
   const int on=(int)std::lround((base+n.start)*ppq),off=std::max(on+24,(int)std::lround((base+n.start+n.length)*ppq));
   events.push_back({on,true,n.note,n.velocity});events.push_back({off,false,n.note,0});}
 }
 return encode(std::move(events),o);
}
std::vector<uint8_t>MidiFile::basslineBytes(const Progression&p,const MidiOptions&o){std::vector<Event>events;for(int ci=0;ci<p.chordCount;++ci){const int length=std::max(120,(int)std::lround(chordLengthBeats(o,ci)*ppq));int note=48+p.chords[(size_t)ci].root;while(note>52)note-=12;while(note<36)note+=12;int base=(int)std::lround(chordStartBeats(o,ci)*ppq);auto pulses=pulseOffsets(o.style,length);if(o.style==Style::standard||o.style==Style::cinematic||o.style==Style::rnb)pulses={0};for(size_t i=0;i<pulses.size();++i){int start=base+pulses[i],end=base+(i+1<pulses.size()?pulses[i+1]:length)-36;events.push_back({start,true,note,std::clamp(o.velocity-12,1,127)});events.push_back({std::max(start+60,end),false,note,0});}}return encode(std::move(events),o);}
bool MidiFile::write(const Progression&p,const std::filesystem::path&f,const MidiOptions&o){return writeBytes(bytes(p,o),f);}bool MidiFile::writeBassline(const Progression&p,const std::filesystem::path&f,const MidiOptions&o){return writeBytes(basslineBytes(p,o),f);}
}

namespace ccp {
const char* rhythmName(MidiRhythm r){static const char*n[]{"RETO","HUMANIZE","STRUM","ARPEJO","ARPEJO DESCENDO","ARPEJO SOBE E DESCE","ARPEJO RÁPIDO",
 "PULSO","SÍNCOPE POP","CONTRATEMPO HOUSE","BALADA","BAIXO E ACORDE","TRAP","TAMBORZÃO","REGGAETON","BOSSA","DEDILHADO"};
 return n[std::clamp((int)r,0,(int)MidiRhythm::count-1)];}

std::vector<ArrangedNote> arrangeChord(const Chord&c,MidiRhythm r,double L,int ci,int velocity){
 std::vector<ArrangedNote>out;if(c.noteCount<=0||L<=0)return out;
 std::vector<int>all(c.notes.begin(),c.notes.begin()+c.noteCount);std::sort(all.begin(),all.end());
 const int bass=all.front();std::vector<int>upper(all.begin()+(all.size()>3?1:0),all.end());
 auto vel=[&](double scale,int salt){int v=(int)std::lround(velocity*scale)+((ci*13+salt*7)%7)-3;return std::clamp(v,1,127);};
 auto add=[&](double at,double len,int note,double scale,int salt){if(at>=L-0.01)return;len=std::min(len,L-at-0.02);if(len<0.04)len=0.04;out.push_back({at,len,note,vel(scale,salt)});};
 auto block=[&](double at,double len,const std::vector<int>&notes,double scale){for(size_t i=0;i<notes.size();++i)add(at,len,notes[i],scale,(int)i);};
 // tiles a pattern of (start, length, part, accent) over the chord, cycle beats long. part: 0 all, 1 bass, 2 upper
 struct H{double t,len;int part;double acc;};
 auto tile=[&](double cycle,std::initializer_list<H>hits){for(double b=0;b<L-0.01;b+=cycle)for(const auto&h:hits){const double at=b+h.t;if(at>=L-0.01)continue;
   const auto&notes=h.part==1?std::vector<int>{bass}:h.part==2?upper:all;block(at,h.len,notes,h.acc);}};
 auto arp=[&](const std::vector<int>&seq,double step,double gate){int k=0;for(double t=0;t<L-0.01;t+=step,++k){const double accent=k%4==0?1.0:.82;add(t,step*gate,seq[(size_t)k%seq.size()],accent,k);}};
 const double hold=L-0.04;
 switch(r){
  case MidiRhythm::straight: block(0,hold,all,1.0);break;
  case MidiRhythm::humanize: for(size_t i=0;i<all.size();++i)add(0.02+0.01*(double)((ci*17+(int)i*23)%5),hold,all[i],1.0,(int)i*3);break;
  case MidiRhythm::strum: for(size_t i=0;i<all.size();++i)add(0.055*(double)i,hold-0.055*(double)i,all[i],1.0-0.03*(double)i,(int)i);break;
  case MidiRhythm::arpeggio: arp(all,std::max(0.25,std::min(0.5,L/(double)all.size())),0.92);break;
  case MidiRhythm::arpDown:{auto d=all;std::reverse(d.begin(),d.end());arp(d,std::max(0.25,std::min(0.5,L/(double)all.size())),0.92);break;}
  case MidiRhythm::arpUpDown:{auto d=all;for(int i=(int)all.size()-2;i>0;--i)d.push_back(all[(size_t)i]);arp(d,0.25,0.9);break;}
  case MidiRhythm::arpFast:{auto d=upper;for(int i=(int)upper.size()-2;i>0;--i)d.push_back(upper[(size_t)i]);add(0,hold,bass,1.0,99);arp(d,0.125,0.85);break;}
  case MidiRhythm::pulse: tile(0.5,{{0,.42,0,1.0},{.5,.42,0,.78}});break;
  case MidiRhythm::popSync: add(0,hold,bass,1.0,1);tile(2.0,{{0,.45,2,1.0},{.75,.4,2,.85},{1.5,.45,2,.9}});break;
  case MidiRhythm::offbeat: add(0,hold,bass,.9,1);tile(1.0,{{.5,.28,2,1.0}});break;
  case MidiRhythm::ballad: add(0,hold,bass,1.0,1);tile(1.0,{{0,.95,2,.85}});break;
  case MidiRhythm::bassChord: tile(2.0,{{0,.9,1,1.0},{1,.6,2,.85}});break;
  case MidiRhythm::trap: tile(4.0,{{0,1.3,0,1.0},{1.5,.6,2,.85},{2.5,.9,0,.95},{3.5,.4,2,.75}});break;
  case MidiRhythm::tamborzao: tile(2.0,{{0,.32,0,1.0},{.75,.32,2,.9},{1.5,.4,0,.95}});break;
  case MidiRhythm::reggaeton: tile(1.0,{{0,.3,1,1.0},{.75,.2,2,.9},{.5,.2,2,.7}});break;
  case MidiRhythm::bossa: tile(2.0,{{0,.9,1,1.0},{1,.9,1,.9},{0,.45,2,.8},{.75,.4,2,.85},{1.5,.4,2,.8}});break;
  case MidiRhythm::fingerpick:{std::vector<int>seq{bass};for(size_t i=0;i<upper.size();++i)seq.push_back(upper[(i*2)%upper.size()]);
   int k=0;for(double t=0;t<L-0.01;t+=0.5,++k){const int note=k%4==0?bass:seq[(size_t)(1+(k-1)%(int)(seq.size()-1))];add(t,k%4==0?std::min(2.0,L-t):1.0,note,k%4==0?1.0:.8,k);}break;}
  default: block(0,hold,all,1.0);break;
 }
 return out;}
}
