#pragma once

#include "GrainBuffer.h"
#include "GrainWindow.h"

namespace vm
{
    // One grain: a windowed, possibly pitched read from the capture buffer.
    struct GrainVoice
    {
        bool active = false;
        double position = 0.0; // read position in the buffer (samples)
        double speed = 1.0;    // samples advanced per output sample (pitch)
        int age = 0;
        int length = 1;
        float gainL = 1.0f;
        float gainR = 1.0f;

        // Adds this grain's output for one sample and moves it on.
        void render (const GrainBuffer& buffer, const GrainWindow& window, bool stereo,
                     float& outL, float& outR) noexcept
        {
            const float w = window.get ((float) age / (float) length);

            outL += w * gainL * buffer.read (0, position);
            outR += w * gainR * buffer.read (stereo ? 1 : 0, position);

            position = buffer.wrap (position + speed);

            if (++age >= length)
                active = false;
        }
    };
}
