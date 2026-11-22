#include "Music.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <functional>

namespace ccp { namespace {
constexpr const char* noteNames[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
int wrap(int n){n%=12;return n<0?n+12:n;}
std::array<double,7> degreeWeights(const GenerationSettings&s){
 std::array<double,7>w{9,3,5,5,8,4,6};auto add=[&](int d,double v){w[(size_t)d]+=v;};
 switch(s.style){case Style::pop:add(0,8);add(3,5);add(4,6);add(5,5);break;case Style::trap:add(0,7);add(5,7);add(2,4);break;case Style::hipHop:add(0,5);add(3,4);add(5,6);break;case Style::rnb:add(1,4);add(3,7);add(5,7);break;case Style::edm:add(0,8);add(3,6);add(5,6);break;case Style::house:add(0,6);add(3,5);add(4,5);add(5,5);break;case Style::loFi:add(1,5);add(3,7);add(5,7);break;case Style::phonk:add(0,8);add(1,5);add(5,7);break;case Style::indie:add(0,5);add(2,5);add(3,5);break;case Style::funkBrasileiro:add(0,9);add(3,4);add(4,6);break;case Style::latin:add(0,8);add(3,6);add(4,8);break;case Style::cinematic:add(0,6);add(1,4);add(5,6);add(6,5);break;case Style::jazz:add(1,10);add(4,11);add(0,9);add(5,7);break;default:break;}
 switch(s.mood){case Mood::melancholic:add(5,8);add(3,6);add(1,4);break;case Mood::emotional:add(3,6);add(4,6);add(5,5);break;case Mood::atmospheric:add(1,5);add(3,5);add(5,7);break;case Mood::fun:add(0,9);add(3,6);add(4,7);break;case Mood::dark:add(0,7);add(1,7);add(5,6);break;case Mood::romantic:add(1,5);add(3,7);add(5,8);break;case Mood::nostalgic:add(0,5);add(3,7);add(5,7);break;case Mood::hopeful:add(0,10);add(3,5);add(4,5);break;case Mood::mysterious:add(1,7);add(2,6);add(6,7);break;case Mood::tense:add(1,7);add(4,8);add(6,8);break;case Mood::epic:add(0,9);add(3,6);add(4,8);break;case Mood::aggressive:add(0,9);add(1,6);add(4,7);break;default:break;}return w;
}
std::vector<int> chordIntervals(const std::array<int,7>&scale,int degree,ChordKind kind){
 auto rel=[&](int step){int idx=degree+step;int n=scale[(size_t)(idx%7)]-scale[(size_t)degree]+12*(idx/7);while(n<0)n+=12;return n;};
 // Suspensions and added tones are always the perfect/major intervals the name promises (a diatonic
 // 4th can be a tritone and a diatonic 9th a b9, which would not be a sus4 or an add9).
 if(kind==ChordKind::sus2)return{0,2,rel(4)};if(kind==ChordKind::sus4)return{0,5,rel(4)};if(kind==ChordKind::dominant7)return{0,4,7,10};if(kind==ChordKind::minorMajor7)return{0,3,7,11};
 std::vector<int>v{0,rel(2),rel(4)};if(kind==ChordKind::seventh||kind==ChordKind::ninth||kind==ChordKind::eleventh)v.push_back(rel(6));if(kind==ChordKind::ninth||kind==ChordKind::add9||kind==ChordKind::eleventh)v.push_back(14);if(kind==ChordKind::sixth||kind==ChordKind::sixthNinth)v.push_back(9);if(kind==ChordKind::sixthNinth)v.push_back(14);if(kind==ChordKind::eleventh){v.erase(v.begin()+2);v.push_back(17);}return v;
}
std::string chordName(int root,const std::vector<int>&iv,ChordKind kind){
 const int third=wrap(iv[1]),fifth=kind==ChordKind::eleventh?7:(iv.size()>2?wrap(iv[2]):7);std::string q;if(kind==ChordKind::sus2)return std::string(noteNames[root])+"sus2";if(kind==ChordKind::sus4)return std::string(noteNames[root])+"sus4";if(kind==ChordKind::dominant7)return std::string(noteNames[root])+"7";if(kind==ChordKind::minorMajor7)return std::string(noteNames[root])+"mMaj7";if(third==3&&fifth==6)q="dim";else if(third==4&&fifth==8)q="aug";else if(third==3)q="m";if(kind==ChordKind::triad)return std::string(noteNames[root])+q;if(kind==ChordKind::add9)return std::string(noteNames[root])+q+"add9";if(kind==ChordKind::sixth)return std::string(noteNames[root])+q+"6";if(kind==ChordKind::sixthNinth)return std::string(noteNames[root])+q+"6/9";if(kind==ChordKind::eleventh)return std::string(noteNames[root])+q+"11";const int seventh=iv.size()>3?wrap(iv[3]):0;if(kind==ChordKind::seventh){if(q.empty()&&seventh==11)return std::string(noteNames[root])+"maj7";if(q=="m"&&seventh==10)return std::string(noteNames[root])+"m7";if(q=="dim"&&seventh==10)return std::string(noteNames[root])+"m7b5";return std::string(noteNames[root])+q+"7";}if(q.empty()&&seventh==11)return std::string(noteNames[root])+"maj9";if(q=="m"&&seventh==10)return std::string(noteNames[root])+"m9";return std::string(noteNames[root])+q+"9";
}
int nearestCost(const std::vector<int>&c,const Chord*p){if(!p){int sum=std::accumulate(c.begin(),c.end(),0);return std::abs(sum/(int)c.size()-62);}int cost=0;for(int n:c){int best=99;for(int i=0;i<p->noteCount;++i)best=std::min(best,std::abs(n-p->notes[(size_t)i]));cost+=best;}for(int i=0;i<p->noteCount;++i)for(int n:c)if(wrap(n)==wrap(p->notes[(size_t)i]))cost-=3;return cost+std::abs(c.front()-p->notes[0])/2;}
double transitionScore(int from,int to){static constexpr double t[7][7]={{0,2,2,6,7,6,1},{2,0,1,3,10,2,5},{1,2,0,5,2,9,2},{7,3,1,0,10,4,4},{12,1,4,2,0,9,1},{5,9,2,8,6,0,2},{12,2,6,1,5,2,0}};return t[from][to];}
std::array<std::array<int,4>,3> styleTemplates(Style style){switch(style){case Style::pop:return{{{{0,4,5,3}},{{0,5,3,4}},{{5,3,0,4}}}};case Style::trap:return{{{{0,5,2,6}},{{0,6,5,3}},{{5,2,6,0}}}};case Style::hipHop:return{{{{5,3,0,4}},{{0,2,5,3}},{{1,5,3,4}}}};case Style::rnb:return{{{{1,4,0,5}},{{5,1,4,0}},{{3,2,5,1}}}};case Style::edm:return{{{{0,4,5,3}},{{5,3,0,4}},{{0,3,5,4}}}};case Style::house:return{{{{0,5,3,4}},{{5,3,0,4}},{{0,3,4,5}}}};case Style::loFi:return{{{{1,4,0,5}},{{5,3,1,4}},{{0,2,5,3}}}};case Style::phonk:return{{{{0,5,1,4}},{{0,1,5,3}},{{5,0,1,4}}}};case Style::indie:return{{{{0,2,3,5}},{{0,3,2,5}},{{5,3,0,2}}}};case Style::funkBrasileiro:return{{{{0,3,4,5}},{{5,3,0,4}},{{0,4,5,3}}}};case Style::latin:return{{{{0,6,5,4}},{{0,3,4,5}},{{5,3,0,4}}}};case Style::cinematic:return{{{{0,5,1,6}},{{5,3,0,4}},{{0,1,5,6}}}};case Style::jazz:return{{{{1,4,0,5}},{{5,1,4,0}},{{0,5,1,4}}}};default:return{{{{0,4,5,3}},{{0,5,3,4}},{{5,3,0,4}}}};}}
double sequenceScore(const GenerationSettings&s,const std::array<int,maxChords>&d,int count,const std::array<bool,maxChords>&locked){auto w=degreeWeights(s);double score=0;for(int i=0;i<count;++i){score+=std::log1p(w[(size_t)d[(size_t)i]])*2.2;if(locked[(size_t)i])score+=3; if(i&&d[(size_t)i]==d[(size_t)(i-1)])score-=30;else if(i)score+=transitionScore(d[(size_t)(i-1)],d[(size_t)i]);}for(int base=0;base<count;base+=4){double best=0;for(const auto&t:styleTemplates(s.style)){double q=0;for(int i=0;i<4&&base+i<count;++i){if(d[(size_t)(base+i)]==t[(size_t)i])q+=5;if(i&&d[(size_t)(base+i-1)]==t[(size_t)(i-1)]&&d[(size_t)(base+i)]==t[(size_t)i])q+=3;}best=std::max(best,q);}score+=best;}if(s.style==Style::jazz)for(int i=0;i+2<count;++i)if(d[(size_t)i]==1&&d[(size_t)(i+1)]==4&&d[(size_t)(i+2)]==0)score+=20;return score;}
using KindWeights=std::array<double,(size_t)ChordKind::count>;
KindWeights kindWeights(const GenerationSettings&s,int position,int degree){const Complexity c=HarmonyEngine::resolve(s);KindWeights w{};if(c==Complexity::simple)w={12,1,.2,2,2,2,.4,1,.1,.1,.1};else if(c==Complexity::colorful)w={4,3,1.5,5,3,3,1.2,3,.4,1.5,.8};else if(c==Complexity::rich)w={1.2,6,6,2.5,1.5,1.5,2.5,2.5,1.5,5,4};else w={.3,4,9,3,1,1,2,1.5,1.5,7,6};auto add=[&](ChordKind k,double v){w[(size_t)k]+=v;};if(s.style==Style::jazz){add(ChordKind::seventh,9);add(ChordKind::ninth,10);add(ChordKind::sixthNinth,7);add(ChordKind::eleventh,6);if(degree==4)add(ChordKind::dominant7,22);}if(s.style==Style::rnb||s.style==Style::loFi){add(ChordKind::seventh,6);add(ChordKind::ninth,8);add(ChordKind::sixthNinth,5);add(ChordKind::eleventh,4);}if(s.style==Style::cinematic||s.style==Style::indie||s.mood==Mood::atmospheric){add(ChordKind::add9,6);add(ChordKind::sus2,5);add(ChordKind::eleventh,4);}if(s.style==Style::latin||s.style==Style::funkBrasileiro||s.mood==Mood::fun){add(ChordKind::sixth,6);add(ChordKind::dominant7,5);add(ChordKind::sus4,3);}if(s.mood==Mood::romantic||s.mood==Mood::emotional){add(ChordKind::seventh,5);add(ChordKind::ninth,5);add(ChordKind::sixthNinth,4);}if(s.mood==Mood::mysterious||s.mood==Mood::tense||s.mood==Mood::dark){add(ChordKind::minorMajor7,degree==0||degree==5?7:3);add(ChordKind::sus2,4);add(ChordKind::dominant7,3);}if(s.tension==Tension::soft){add(ChordKind::sixth,4);add(ChordKind::add9,4);add(ChordKind::sus2,3);}if(s.tension==Tension::medium){add(ChordKind::seventh,5);add(ChordKind::dominant7,5);add(ChordKind::sus4,3);}if(s.tension==Tension::high){add(ChordKind::dominant7,12);add(ChordKind::minorMajor7,10);add(ChordKind::eleventh,8);w[(size_t)ChordKind::triad]*=.25;}if(degree==4&&(position%4==1||position%4==2))add(ChordKind::dominant7,4);
 if(c==Complexity::simple)for(size_t k=1;k<w.size();++k)if(k!=(size_t)ChordKind::sus2&&k!=(size_t)ChordKind::sus4)w[k]*=.08;
 if(c==Complexity::full)w[(size_t)ChordKind::triad]*=.2;
 {const auto sc=HarmonyEngine::scaleNotes(s.key,s.scale);const int fifth=wrap(sc[(size_t)((degree+4)%7)]-sc[(size_t)degree]);
  if(fifth!=7)for(auto k:{ChordKind::sus2,ChordKind::sus4,ChordKind::add9,ChordKind::sixth,ChordKind::sixthNinth,ChordKind::ninth,ChordKind::eleventh})w[(size_t)k]=0;}
 return w;}
}

std::vector<int> HarmonyEngine::scaleIntervals(ScaleType s){switch(s){case ScaleType::major:return{0,2,4,5,7,9,11};case ScaleType::naturalMinor:return{0,2,3,5,7,8,10};case ScaleType::harmonicMinor:return{0,2,3,5,7,8,11};case ScaleType::melodicMinor:return{0,2,3,5,7,9,11};case ScaleType::dorian:return{0,2,3,5,7,9,10};case ScaleType::phrygian:return{0,1,3,5,7,8,10};case ScaleType::lydian:return{0,2,4,6,7,9,11};case ScaleType::mixolydian:return{0,2,4,5,7,9,10};case ScaleType::phrygianDominant:return{0,1,4,5,7,8,10};default:return{0,2,4,5,7,9,11};}}
std::array<int,7> HarmonyEngine::scaleNotes(int key,ScaleType s){auto iv=scaleIntervals(s);std::array<int,7>o{};for(int i=0;i<7;++i)o[(size_t)i]=wrap(key+iv[(size_t)i]);return o;}
namespace {
// ---- Voicing (rebuilt 22/11): a bass note on its own, then an upper structure that is spread like a
// keyboard player would: no mud low down, extensions on top, smooth inner voices and a singing top line.
struct Tone { int pc; int interval; int role; };   // role: 0 root, 1 third, 2 fifth, 3 seventh/sixth, 4 extension
int roleOf(int interval){const int i=interval%12;if(interval==0)return 0;if(interval>=13&&interval!=19)return 4;if(i==3||i==4||i==2&&interval<12||i==5&&interval<12)return 1;if(i==7||i==6||i==8)return 2;if(i==9||i==10||i==11)return 3;return 4;}
std::vector<int> upperOf(const Chord*p){std::vector<int>u;if(!p)return u;for(int i=1;i<p->noteCount;++i)if(p->notes[(size_t)i]>=50)u.push_back(p->notes[(size_t)i]);if(u.empty())for(int i=0;i<p->noteCount;++i)u.push_back(p->notes[(size_t)i]);return u;}
std::vector<int> voiceChord(const GenerationSettings&s,int root,const std::vector<int>&iv,const Chord*previous,int position){
 const Complexity cx=HarmonyEngine::resolve(s);
 // bass: the root (INVERTER walks through the third and the fifth), in E1..D#2 for the full sound, F#1.. for the rest
 int bassInterval=0;
 if(s.voicing==Voicing::inverted&&iv.size()>=3){const int pick=1+position%2;bassInterval=iv[(size_t)std::min<int>(pick,(int)iv.size()-1)]%12;}
 const int bassPc=wrap(root+bassInterval);
 int bass=36+bassPc;if(bass<40)bass+=12;   // 40..51 (E2..D#3)
 if(cx==Complexity::full&&bass-12>=36)bass-=12;
 if(previous&&previous->noteCount>0){const int pb=previous->notes[0];if(bass-pb>7&&bass-12>=36)bass-=12;else if(pb-bass>7&&bass+12<=52)bass+=12;}
 // upper tones: everything but a plain root when the chord has colour (the bass has it), no fifth when crowded
 std::vector<Tone>tones;for(int x:iv)tones.push_back({wrap(root+x),x,roleOf(x)});
 const bool coloured=iv.size()>=4;
 std::vector<Tone>upper;
 for(const auto&t:tones){if(t.role==0&&coloured&&cx!=Complexity::full)continue;if(t.role==2&&iv.size()>=5&&cx!=Complexity::full&&t.interval==7)continue;upper.push_back(t);}
 if(s.voicing==Voicing::inverted){bool hasRoot=false;for(auto&t:upper)hasRoot|=t.role==0;if(!hasRoot)upper.push_back({wrap(root),0,0});}
 if(upper.size()<3)for(const auto&t:tones){bool have=false;for(auto&u:upper)have|=u.pc==t.pc;if(!have)upper.push_back(t);if(upper.size()>=3)break;}
 const int n=(int)upper.size();
 // targets per voicing / complexity
 double spanTarget=cx==Complexity::simple?9:13,centre=64;int lo=50,hi=84;
 if(s.voicing==Voicing::closed){spanTarget=std::min(spanTarget,10.0);}
 if(s.voicing==Voicing::open){spanTarget=20;hi=88;}
 if(cx==Complexity::full){spanTarget=std::max(spanTarget,19.0);hi=88;centre=66;}
 const auto prev=upperOf(previous);int prevTop=prev.empty()?0:*std::max_element(prev.begin(),prev.end());
 std::vector<int>best;double bestCost=1e18;std::vector<int>cur((size_t)n);
 // every tone in each octave that lands inside [lo,hi]
 std::vector<std::vector<int>>choices((size_t)n);for(int i=0;i<n;++i)for(int o=36;o<=hi;o+=12){const int note=o+upper[(size_t)i].pc;if(note>=lo&&note<=hi)choices[(size_t)i].push_back(note);}
 std::function<void(int)>walk=[&](int i){
  if(i==n){std::vector<int>v=cur;std::sort(v.begin(),v.end());for(size_t k=1;k<v.size();++k)if(v[k]==v[k-1])return;
   double c=0;const int span=v.back()-v.front();c+=std::abs(span-spanTarget)*1.4;if(s.voicing==Voicing::closed&&span>12)c+=60;
   double mean=0;for(int x:v)mean+=x;mean/=v.size();if(prev.empty())c+=std::abs(mean-centre)*1.2;else c+=std::abs(mean-centre)*.35;
   for(size_t k=1;k<v.size();++k){const int gap=v[k]-v[k-1];if(gap==1)c+=v[k]<72?22:9;if(gap<=2&&v[k-1]<57)c+=14;if(gap>9)c+=(gap-9)*2.5;}
   if(v.front()<bass+7)c+=25;if(v.front()-bass>22)c+=(v.front()-bass-22)*1.5;
   // extensions above the guide tones, the third not doubled-on-top by a b9 below it
   int lowest=v.front();for(int k=0;k<n;++k)if(cur[(size_t)k]==lowest&&upper[(size_t)k].role==4)c+=9;
   int topRole=0;for(int k=0;k<n;++k)if(cur[(size_t)k]==v.back())topRole=upper[(size_t)k].role;if(topRole==0&&coloured)c+=3;
   // voice leading: inner movement and a smooth top line, keeping common tones
   if(!prev.empty()){for(int x:v){int bestD=99;for(int y:prev)bestD=std::min(bestD,std::abs(x-y));c+=bestD*.9;}const int topMove=std::abs(v.back()-prevTop);c+=topMove>5?(topMove-5)*2.0+2.5:topMove*.5;}
   if(c<bestCost){bestCost=c;best=v;}return;}
  for(int note:choices[(size_t)i]){cur[(size_t)i]=note;walk(i+1);}};
 walk(0);
 if(best.empty()){for(const auto&t:upper)best.push_back(60+t.pc);std::sort(best.begin(),best.end());}
 std::vector<int>out{bass};
 if(cx==Complexity::full){
  // low octave and fifth under the hands (only where it stays clear), and a high doubling on top
  const int oct=bass+12,fifth=bass+19;
  if(oct<best.front()-2)out.push_back(oct);
  if(bassInterval==0&&fifth<best.front()-2&&fifth>=55)out.push_back(fifth);
  int topDouble=-1;
  for(const auto&t:tones)if(t.role==0||t.role==2){int note=best.back();while(wrap(note)!=t.pc||note<=best.back()+2)++note;if(note<=91){topDouble=note;break;}}
  for(int x:best)out.push_back(x);if(topDouble>0)out.push_back(topDouble);
 } else for(int x:best)out.push_back(x);
 std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
 if((int)out.size()>maxNotes)out.resize((size_t)maxNotes);
 return out;}
 }
Chord HarmonyEngine::buildChord(const GenerationSettings&s,int degree,ChordKind kind,const Chord*previous,int position){degree=(degree%7+7)%7;auto scale=scaleNotes(s.key,s.scale);{const int fifth=wrap(scale[(size_t)((degree+4)%7)]-scale[(size_t)degree]);if(fifth!=7){const bool dim=fifth==6;if(kind==ChordKind::sus2||kind==ChordKind::sus4||kind==ChordKind::add9||kind==ChordKind::sixth)kind=ChordKind::triad;else if(kind==ChordKind::sixthNinth||kind==ChordKind::ninth||kind==ChordKind::eleventh)kind=dim?ChordKind::seventh:ChordKind::triad;}}auto iv=chordIntervals(scale,degree,kind);const int root=scale[(size_t)degree];auto best=voiceChord(s,root,iv,previous,position);Chord out;out.degree=degree;out.root=root;out.kind=kind;out.noteCount=(int)std::min<size_t>(maxNotes,best.size());for(int i=0;i<out.noteCount;++i)out.notes[(size_t)i]=best[(size_t)i];out.name=chordName(root,iv,kind);if(s.voicing==Voicing::inverted&&out.noteCount&&wrap(out.notes[0])!=root)out.name+=std::string("/")+noteNames[wrap(out.notes[0])];out.velocity=(s.mood==Mood::atmospheric||s.voicing==Voicing::open)?82:88;return out;}
namespace {
struct ChromaticQuality { const char* suffix; std::vector<int> intervals; };
const std::array<ChromaticQuality,chromaticQualityCount>& chromaticQualities(){
 static const std::array<ChromaticQuality,chromaticQualityCount> q{{{"",{0,4,7}},{"m",{0,3,7}},{"7",{0,4,7,10}},{"maj7",{0,4,7,11}},{"m7",{0,3,7,10}},
  {"dim",{0,3,6}},{"m7b5",{0,3,6,10}},{"dim7",{0,3,6,9}},{"aug",{0,4,8}},{"sus2",{0,2,7}},
  {"sus4",{0,5,7}},{"add9",{0,4,7,14}},{"9",{0,4,7,10,14}},{"maj9",{0,4,7,11,14}},{"m9",{0,3,7,10,14}}}};
 return q;
}
}
const char* chromaticQualitySuffix(int quality){return chromaticQualities()[(size_t)std::clamp(quality,0,chromaticQualityCount-1)].suffix;}
Chord HarmonyEngine::buildChromaticChord(const GenerationSettings&s,int root,int quality,const Chord*previous,int position){
 root=wrap(root);quality=std::clamp(quality,0,chromaticQualityCount-1);const auto&q=chromaticQualities()[(size_t)quality];
 auto scale=scaleNotes(s.key,s.scale);int degree=0;
 // Nearest scale degree at or below the root, so degree-based logic keeps working.
 for(int d=0;d<7;++d)if(wrap(root-scale[(size_t)d])<wrap(root-scale[(size_t)degree]))degree=d;
 auto best=voiceChord(s,root,q.intervals,previous,position);Chord out;out.degree=degree;out.root=root;out.kind=ChordKind::triad;out.quality=quality;
 out.noteCount=(int)std::min<size_t>(maxNotes,best.size());for(int i=0;i<out.noteCount;++i)out.notes[(size_t)i]=best[(size_t)i];
 out.name=std::string(noteNames[root])+q.suffix;if(s.voicing==Voicing::inverted&&out.noteCount&&wrap(out.notes[0])!=root)out.name+=std::string("/")+noteNames[wrap(out.notes[0])];out.velocity=(s.mood==Mood::atmospheric||s.voicing==Voicing::open)?82:88;return out;
}
Chord HarmonyEngine::rebuild(const GenerationSettings&s,const Chord&c,const Chord*previous,int position){
 return c.quality>=0?buildChromaticChord(s,c.root,c.quality,previous,position):buildChord(s,c.degree,c.kind,previous,position);
}
Complexity HarmonyEngine::resolve(const GenerationSettings&s){if(s.complexity!=Complexity::automatic)return s.complexity;if(s.style==Style::rnb||s.style==Style::loFi||s.style==Style::jazz||s.mood==Mood::romantic||s.mood==Mood::mysterious)return Complexity::rich;if(s.style==Style::pop||s.style==Style::indie||s.style==Style::cinematic||s.mood==Mood::atmospheric||s.mood==Mood::emotional)return Complexity::colorful;return Complexity::simple;}
Progression HarmonyEngine::transpose(const Progression&p,int newKey){Progression o=p;o.settings.key=wrap(newKey);const Chord*prev=nullptr;for(int i=0;i<o.chordCount;++i){auto c=p.chords[(size_t)i];c.root=wrap(c.root+o.settings.key-p.settings.key);o.chords[(size_t)i]=rebuild(o.settings,c,prev,i);prev=&o.chords[(size_t)i];}return o;}
bool HarmonyEngine::valid(const Progression&p){if(p.chordCount!=2&&p.chordCount!=4&&p.chordCount!=8)return false;for(int ci=0;ci<p.chordCount;++ci){const auto&c=p.chords[(size_t)ci];if(c.noteCount<3||c.noteCount>maxNotes||!nameMatches(c))return false;for(int i=0;i<c.noteCount;++i)if(c.notes[(size_t)i]<0||c.notes[(size_t)i]>127||(i&&c.notes[(size_t)i]<=c.notes[(size_t)(i-1)]))return false;}return true;}
bool HarmonyEngine::nameMatches(const Chord&c){return!c.name.empty();}int HarmonyEngine::movement(const Chord&a,const Chord&b){int total=0;for(int i=0;i<b.noteCount;++i){int best=99;for(int j=0;j<a.noteCount;++j)best=std::min(best,std::abs(b.notes[(size_t)i]-a.notes[(size_t)j]));total+=best;}return total;}
ProgressionGenerator::ProgressionGenerator(uint32_t seed):rng(seed){}uint64_t ProgressionGenerator::nextIdentity(){return counter++;}
ChordKind ProgressionGenerator::chooseKind(const GenerationSettings&s,int position,int degree){auto w=kindWeights(s,position,degree);std::discrete_distribution<int>d(w.begin(),w.end());return(ChordKind)d(rng);}
uint64_t ProgressionGenerator::signature(const Progression&p)const{uint64_t h=1469598103934665603ull;auto mix=[&](uint64_t v){h^=v+1;h*=1099511628211ull;};mix(p.chordCount);for(int i=0;i<p.chordCount;++i){mix((uint64_t)p.chords[(size_t)i].degree);mix((uint64_t)p.chords[(size_t)i].kind);}return h;}
bool ProgressionGenerator::recentlyUsed(uint64_t v)const{return std::find(recentProgressions.begin(),recentProgressions.end(),v)!=recentProgressions.end();}void ProgressionGenerator::remember(const Progression&p){recentProgressions.push_back(signature(p));while(recentProgressions.size()>64)recentProgressions.pop_front();}
Progression ProgressionGenerator::generate(const GenerationSettings&s,const Progression*current,const std::array<bool,maxChords>&locked,int requestedCount){
 const int count=requestedCount>=8?8:(requestedCount<=2?2:4);Progression best;double bestScore=-1e12;std::uniform_real_distribution<double>jitter(-2.5,2.5);auto dw=degreeWeights(s);
 for(int attempt=0;attempt<420;++attempt){Progression p;p.settings=s;p.chordCount=count;p.identity=nextIdentity();std::array<int,maxChords>degrees{};const Chord*previous=nullptr;for(int i=0;i<count;++i){if(current&&i<current->chordCount&&locked[(size_t)i]){p.chords[(size_t)i]=current->chords[(size_t)i];degrees[(size_t)i]=p.chords[(size_t)i].degree;previous=&p.chords[(size_t)i];continue;}auto weights=dw;int group=(i/4)*4;for(int j=group;j<i;++j)weights[(size_t)degrees[(size_t)j]]*=.025;if(i)weights[(size_t)degrees[(size_t)(i-1)]]*=.01;std::discrete_distribution<int>degreePick(weights.begin(),weights.end());int degree=degreePick(rng);auto kind=chooseKind(s,i,degree);if(i>=4&&degree==degrees[(size_t)(i-4)]&&kind==p.chords[(size_t)(i-4)].kind)kind=(ChordKind)(((int)kind+1)%(int)ChordKind::count);degrees[(size_t)i]=degree;p.chords[(size_t)i]=HarmonyEngine::buildChord(s,degree,kind,previous,i);previous=&p.chords[(size_t)i];}if(!HarmonyEngine::valid(p)||recentlyUsed(signature(p)))continue;double score=sequenceScore(s,degrees,count,locked)+jitter(rng);for(int i=1;i<count;++i)score-=HarmonyEngine::movement(p.chords[(size_t)(i-1)],p.chords[(size_t)i])*.08;if(score>bestScore){best=p;bestScore=score;}}
 if(bestScore<-1e11){recentProgressions.clear();return generate(s,current,locked,count);}for(int i=0;i<count;++i)lastDegrees[(size_t)i]=best.chords[(size_t)i].degree;remember(best);++variationSerial;return best;
}
Progression ProgressionGenerator::variation(const Progression&p,const std::array<bool,maxChords>&locked){return generate(p.settings,&p,locked,p.chordCount);}Progression ProgressionGenerator::continueToEight(const Progression&p){std::array<bool,maxChords>locked{};for(int i=0;i<std::min(4,p.chordCount);++i)locked[(size_t)i]=true;return generate(p.settings,&p,locked,8);}
Progression ProgressionGenerator::regenerateChord(const Progression&p,int index){Progression o=p;if(index<0||index>=p.chordCount)return o;std::array<bool,7>used{};const int group=(index/4)*4;for(int i=group;i<std::min(group+4,p.chordCount);++i)if(i!=index)used[(size_t)p.chords[(size_t)i].degree]=true;struct Option{int degree;ChordKind kind;double score;uint64_t id;};std::vector<Option>options;std::array<int,maxChords>degrees{};std::array<bool,maxChords>none{};for(int i=0;i<p.chordCount;++i)degrees[(size_t)i]=p.chords[(size_t)i].degree;for(int degree=0;degree<7;++degree){if(used[(size_t)degree])continue;degrees[(size_t)index]=degree;auto kw=kindWeights(o.settings,index,degree);for(int k=0;k<(int)ChordKind::count;++k){if(kw[(size_t)k]<.25)continue;uint64_t id=((uint64_t)degree<<8)|(uint64_t)k;options.push_back({degree,(ChordKind)k,sequenceScore(o.settings,degrees,o.chordCount,none)+std::log1p(kw[(size_t)k])*3,id});}}std::sort(options.begin(),options.end(),[](const Option&a,const Option&b){return a.score>b.score;});auto&history=recentChordChoices[(size_t)index];uint64_t current=((uint64_t)p.chords[(size_t)index].degree<<8)|(uint64_t)p.chords[(size_t)index].kind;Option*selected=nullptr;size_t start=chordCursor[(size_t)index]++%std::max<size_t>(1,options.size());for(size_t n=0;n<options.size();++n){auto&c=options[(start+n)%options.size()];if(c.id!=current&&std::find(history.begin(),history.end(),c.id)==history.end()){selected=&c;break;}}if(!selected){history.clear();for(auto&c:options)if(c.id!=current){selected=&c;break;}}if(!selected)return o;const Chord*prev=index?&o.chords[(size_t)(index-1)]:nullptr;o.chords[(size_t)index]=HarmonyEngine::buildChord(o.settings,selected->degree,selected->kind,prev,index);for(int i=index+1;i<o.chordCount;++i)o.chords[(size_t)i]=HarmonyEngine::rebuild(o.settings,o.chords[(size_t)i],&o.chords[(size_t)(i-1)],i);o.identity=nextIdentity();history.push_back(selected->id);while(history.size()>32)history.pop_front();remember(o);return o;}
const char* scaleName(ScaleType x){static const char*n[]{"MAIOR","MENOR NATURAL","MENOR HARMÔNICA","MENOR MELÓDICA","DÓRICA","FRÍGIA","LÍDIA","MIXOLÍDIA","FRÍGIA DOMINANTE"};return n[(int)x];}
const char* styleName(Style x){static const char*n[]{"PADRÃO","POP","TRAP","HIP-HOP","R&B","EDM","HOUSE","LO-FI","PHONK","INDIE","FUNK BRASILEIRO","LATIN","CINEMATIC","JAZZ"};return n[(int)x];}
const char* moodName(Mood x){static const char*n[]{"PADRÃO","MELANCÓLICO","EMOCIONAL","ATMOSFÉRICO","DIVERTIDO","SOMBRIO","ROMÂNTICO","NOSTÁLGICO","ESPERANÇOSO","MISTERIOSO","TENSO","ÉPICO","AGRESSIVO"};return n[(int)x];}
const char* complexityName(Complexity x){static const char*n[]{"SIMPLES","COLORIDO","RICO","MUITO RICO","AUTOMÁTICO"};return n[(int)x];}
const char* tensionName(Tension x){static const char*n[]{"PADRÃO","SUAVE","MÉDIA","ALTA"};return n[(int)x];}
const char* voicingName(Voicing x){static const char*n[]{"PADRÃO","FECHADO","ABERTO","INVERTER"};return n[(int)x];}
const char* timbreName(Timbre x){static const char*n[]{"PIANO","PIANO CINEMA","PIANO LO-FI","RHODES","RHODES SUITCASE","WURLITZER","PIANO FM 80s","VIBRAFONE","ÓRGÃO HOUSE","ÓRGÃO","CORDAS","ORQUESTRA","METAIS","PAD ESCURO","PAD JUNO","PAD ATMOSFERA","SYNTHWAVE","SUPERSAW","SAW STAB"};return n[(int)x];}
}
