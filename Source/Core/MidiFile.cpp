#include "MidiFile.h"
#include <algorithm>
#include <fstream>

namespace ccp {namespace {
void be16(std::vector<uint8_t>&o,uint16_t x){o.push_back(x>>8);o.push_back(x);}void be32(std::vector<uint8_t>&o,uint32_t x){o.push_back(x>>24);o.push_back(x>>16);o.push_back(x>>8);o.push_back(x);}void vlq(std::vector<uint8_t>&o,uint32_t v){uint8_t b[5];int n=0;b[n++]=v&127;while((v>>=7))b[n++]=128|(v&127);while(n)o.push_back(b[--n]);}
struct Event{int tick;bool on;int note;int velocity;};
std::vector<uint8_t>encode(std::vector<Event>events,const MidiOptions&){std::sort(events.begin(),events.end(),[](const Event&a,const Event&b){if(a.tick!=b.tick)return a.tick<b.tick;return a.on<b.on;});std::vector<uint8_t>tr;int last=0;for(const auto&e:events){vlq(tr,(uint32_t)std::max(0,e.tick-last));last=e.tick;tr.push_back(e.on?0x90:0x80);tr.push_back((uint8_t)e.note);tr.push_back((uint8_t)(e.on?e.velocity:0));}vlq(tr,0);tr.insert(tr.end(),{0xff,0x2f,0});std::vector<uint8_t>out{'M','T','h','d'};be32(out,6);be16(out,0);be16(out,1);be16(out,MidiFile::ppq);out.insert(out.end(),{'M','T','r','k'});be32(out,(uint32_t)tr.size());out.insert(out.end(),tr.begin(),tr.end());return out;}
std::vector<int>pulseOffsets(Style style,int length){if(style==Style::house||style==Style::edm){std::vector<int>x;for(int t=0;t<length;t+=MidiFile::ppq)x.push_back(t);return x;}if(style==Style::latin||style==Style::funkBrasileiro)return{0,std::min(length-1,MidiFile::ppq+MidiFile::ppq/2),std::min(length-1,MidiFile::ppq*3)};if(style==Style::trap||style==Style::hipHop||style==Style::phonk)return{0,std::min(length-1,MidiFile::ppq*3)};return{0,std::min(length-1,MidiFile::ppq*2)};}
bool writeBytes(const std::vector<uint8_t>&b,const std::filesystem::path&f){std::ofstream s(f,std::ios::binary);s.write((const char*)b.data(),(std::streamsize)b.size());return s.good();}
}
std::vector<uint8_t>MidiFile::bytes(const Progression&p,const MidiOptions&o){
 std::vector<Event>events;const int length=std::max(120,(int)std::lround(o.beatsPerChord*ppq));
 for(int ci=0;ci<p.chordCount;++ci){const auto&c=p.chords[(size_t)ci];const int base=ci*length;
  for(int n=0;n<c.noteCount;++n){int offset=0,velocity=o.velocity,end=length-24;
   if(o.rhythm==MidiRhythm::humanize){offset=12+((ci*47+n*71)%72);velocity+=((ci*11+n*7)%13)-6;}
   else if(o.rhythm==MidiRhythm::strum)offset=n*52;
   else if(o.rhythm==MidiRhythm::arpeggio){const int step=std::max(90,length/std::max(1,c.noteCount));offset=n*step;end=std::min(length-12,offset+std::max(72,step*4/5));}
   offset=std::min(offset,std::max(0,length-48));end=std::max(offset+24,end);
   events.push_back({base+offset,true,c.notes[(size_t)n],std::clamp(velocity,1,127)});
   events.push_back({base+end,false,c.notes[(size_t)n],0});
  }
 }
 return encode(std::move(events),o);
}
std::vector<uint8_t>MidiFile::basslineBytes(const Progression&p,const MidiOptions&o){std::vector<Event>events;const int length=std::max(120,(int)std::lround(o.beatsPerChord*ppq));for(int ci=0;ci<p.chordCount;++ci){int note=48+p.chords[(size_t)ci].root;while(note>52)note-=12;while(note<36)note+=12;int base=ci*length;auto pulses=pulseOffsets(o.style,length);if(o.style==Style::standard||o.style==Style::cinematic||o.style==Style::rnb)pulses={0};for(size_t i=0;i<pulses.size();++i){int start=base+pulses[i],end=base+(i+1<pulses.size()?pulses[i+1]:length)-36;events.push_back({start,true,note,std::clamp(o.velocity-12,1,127)});events.push_back({std::max(start+60,end),false,note,0});}}return encode(std::move(events),o);}
bool MidiFile::write(const Progression&p,const std::filesystem::path&f,const MidiOptions&o){return writeBytes(bytes(p,o),f);}bool MidiFile::writeBassline(const Progression&p,const std::filesystem::path&f,const MidiOptions&o){return writeBytes(basslineBytes(p,o),f);}
}
