#pragma once
#include <GLFW/glfw3.h>
#include <cmath>
#include <string>

namespace ecosphere {
// Application-only measurement; never feeds simulation time.
class FpsCounter {
public:
    bool visible = true;
    void key(int key, int action) {
        if(key == GLFW_KEY_F3 && action == GLFW_PRESS) visible = !visible;
    }
    void sample(double now, bool active) {
        if(!active || !std::isfinite(now)) { reset(); return; }
        if(previous_ < 0) { previous_=now; return; }
        const double delta=now-previous_; previous_=now;
        // Discard discontinuities (suspend, debugger, focus changes), not slow frames.
        if(delta<=0 || delta>2) { reset(); previous_=now; return; }
        seconds_+=delta; ++frames_;
        if(seconds_>=.5) {
            text_="FPS: "+std::to_string(std::lround(frames_/seconds_));
            seconds_=0; frames_=0;
        }
    }
    const std::string& text() const { return text_; }
private:
    void reset() { previous_=-1; seconds_=0; frames_=0; text_="FPS: -"; }
    double previous_=-1, seconds_=0;
    unsigned frames_=0;
    std::string text_="FPS: -";
};
}
