#pragma once
#include <cstdint>
namespace turn {
enum class Mode { Normal, Enrollment, Animation };
enum class Gesture { None, Threshold, Tap, Claim, Enroll, Select, Finalize };
struct Timing { uint32_t debounce=30, hold=3000, window=1500; };
// All intervals must be < 2^31 ms; unsigned subtraction tolerates clock wrap.
class Button {
 Timing timing; bool raw=false, stable=false, armed=false, held=false;
 bool ready=false, initialized=false, windowPress=false, enrollmentPress=false, animationPress=false; uint8_t taps=0;
 uint32_t changed=0, pressed=0, windowStart=0;
public:
 explicit Button(Timing t={}):timing(t){}
 Gesture tick(uint32_t now, bool down, Mode mode, bool initiator=false) {
  if (!initialized) { initialized=true; raw=stable=down; changed=now; ready=!down; return Gesture::None; }
  if (armed && uint32_t(now-windowStart)>=timing.window) {armed=false;taps=0;}
  if (down!=raw) {raw=down;changed=now;}
  if (raw!=stable && uint32_t(now-changed)>=timing.debounce) {
   stable=raw;
   if (!ready) { if (!stable) ready=true; return Gesture::None; }
   if (stable) {pressed=now;held=false;windowPress=armed;enrollmentPress=mode==Mode::Enrollment;animationPress=mode==Mode::Animation;}
   else {
    if(enrollmentPress!=(mode==Mode::Enrollment)){windowPress=false;return Gesture::None;}
    if (held || uint32_t(now-pressed)>=timing.hold) {
     windowPress=false;
     if (mode==Mode::Enrollment) return initiator?Gesture::Finalize:Gesture::None;
     armed=true;taps=0;windowStart=now;return Gesture::Claim;
    }
    if(windowPress&&!armed){windowPress=false;return Gesture::None;}
    windowPress=false;
    if (mode==Mode::Enrollment) return Gesture::Select;
    // Escape gesture takes precedence over animation input suppression.
    if (armed) {if (++taps==2) {armed=false;taps=0;return Gesture::Enroll;}return Gesture::None;}
    return mode==Mode::Normal&&!animationPress?Gesture::Tap:Gesture::None;
   }
  }
  if (ready && stable && !held && uint32_t(now-pressed)>=timing.hold) {held=true;return Gesture::Threshold;}
  return Gesture::None;
 }
 // Mode changes consume an in-progress press, preventing accidental selection.
 void transition() {ready=!stable;armed=false;taps=0;windowPress=false;}
};
}
