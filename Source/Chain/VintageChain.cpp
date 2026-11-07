#include "VintageChain.h"
#include "DSP/Engine/Magnitude.h"

namespace vm
{
    VintageChain::VintageChain()
    {
        using Slot = AudioEngine::Slot;
        auto& engine = master.getEngine();

        auto textureModule = std::make_unique<TextureProcessor>();
        texture = textureModule.get();
        engine.setModule (Slot::texture, std::move (textureModule));

        auto colorModule = std::make_unique<ColorProcessor>();
        color = colorModule.get();
        engine.setModule (Slot::color, std::move (colorModule));

        auto movementModule = std::make_unique<MovementProcessor>();
        movement = movementModule.get();
        engine.setModule (Slot::movement, std::move (movementModule));

        auto spaceModule = std::make_unique<SpaceProcessor>();
        space = spaceModule.get();
        engine.setModule (Slot::space, std::move (spaceModule));
    }

    void VintageChain::prepare (const ProcessSpec& spec, const ChainParameters& p)
    {
        master.prepare (spec);
        vintageEngine.prepare (spec.sampleRate);
        inputMeter.prepare (spec.sampleRate);
        outputMeter.prepare (spec.sampleRate);

        // Start from the current values, without ramping from defaults.
        vintageEngine.setParameters (p.engine);
        vintageEngine.reset();
        applyParameters (p);
        master.reset();
    }

    void VintageChain::reset() noexcept
    {
        master.reset();
        vintageEngine.reset();
        inputMeter.reset();
        outputMeter.reset();
    }

    void VintageChain::applyParameters (const ChainParameters& p) noexcept
    {
        using Slot = AudioEngine::Slot;
        const float m = magnitude::sanitise (p.magnitude);

        // Order: host value -> Vintage Engine offset -> Magnitude. Magnitude comes last,
        // so at 0 nothing the engine adds can get through.
        auto masterParameters = p.master;
        masterParameters.vinylAmount = magnitude::scaleVinyl (m, vintageEngine.applyToVinylAmount (p.master.vinylAmount));
        master.setParameters (masterParameters);

        // The delay's Sync needs the tempo before the Space parameters are set.
        if (p.tempoBpm > 0.0)
            space->setTempo (p.tempoBpm);

        auto& engine = master.getEngine();
        const auto setSlot = [&engine] (Slot slot, const SlotSettings& s)
        {
            engine.getSlot (slot).setEnabled (s.enabled);
            engine.getSlot (slot).setMix (s.mix);
        };

        setSlot (Slot::texture, p.textureSlot);
        setSlot (Slot::color, p.colorSlot);
        setSlot (Slot::movement, p.movementSlot);
        setSlot (Slot::space, p.spaceSlot);

        auto t = p.texture;
        vintageEngine.apply (t);
        magnitude::apply (m, t);
        texture->setParameters (t);

        auto c = p.color;
        vintageEngine.apply (c);
        magnitude::apply (m, c);
        color->setParameters (c);

        auto mv = p.movement;
        vintageEngine.apply (mv);
        magnitude::apply (m, mv);
        movement->setParameters (mv);

        auto s = p.space;
        vintageEngine.apply (s);
        magnitude::apply (m, s);
        space->setParameters (s);
    }

    void VintageChain::process (AudioBufferView block, const ChainParameters& p) noexcept
    {
        inputMeter.process (block);

        // Organic drifts go on top of the host's values for this block.
        vintageEngine.setParameters (p.engine);
        vintageEngine.advance (block.getNumSamples());
        applyParameters (p);

        master.process (block);
        outputMeter.process (block);
    }
}
