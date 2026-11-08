#include "ParameterManager.h"
#include "Core/ParameterIds.h"

namespace vm
{
    namespace
    {
        const juce::Identifier stateType { "VintageMaker" };
        const juce::Identifier versionAttribute { "stateVersion" };

        juce::ParameterID makeId (const char* id) { return { id, param::idVersion }; }

        juce::String formatDb (float value, int)
        {
            return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
        }

        juce::String formatPercent (float value, int)
        {
            return juce::String (juce::roundToInt (value)) + " %";
        }

        juce::String formatSignedPercent (float value, int)
        {
            return (value > 0.0f ? "+" : "") + juce::String (juce::roundToInt (value)) + " %";
        }

        juce::String formatHz (float value, int)
        {
            return juce::String (value, value < 10.0f ? 2 : 1) + " Hz";
        }

        std::atomic<float>* requireParameter (juce::AudioProcessorValueTreeState& s, const char* id)
        {
            auto* p = s.getRawParameterValue (id);
            jassert (p != nullptr); // every ID in ParameterIds.h must be in createLayout()
            return p;
        }

        void addPercent (juce::AudioProcessorParameterGroup& layout,
                         const char* id, const char* name, float defaultPercent)
        {
            layout.addChild (std::make_unique<juce::AudioParameterFloat> (
                makeId (id), name, juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), defaultPercent,
                juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (formatPercent)));
        }

        void addHz (juce::AudioProcessorParameterGroup& layout,
                    const char* id, const char* name, float low, float high, float defaultHz)
        {
            layout.addChild (std::make_unique<juce::AudioParameterFloat> (
                makeId (id), name, juce::NormalisableRange<float> (low, high, 0.001f, 0.6f), defaultHz,
                juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (formatHz)));
        }

        void addTextureParameters (juce::AudioProcessorParameterGroup& layout)
        {
            const TextureParameters d; // defaults live in one place: TextureParameters

            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::texEnable), "Texture On", true));
            addPercent (layout, param::texMix, "Texture Dry/Wet", 100.0f);

            addPercent (layout, param::texWowDepth, "Wow Depth", d.wowDepth * 100.0f);
            addHz (layout, param::texWowRate, "Wow Rate", texture::wowRateMinHz, texture::wowRateMaxHz, d.wowRate);
            addPercent (layout, param::texFlutterDepth, "Flutter Depth", d.flutterDepth * 100.0f);
            addHz (layout, param::texFlutterRate, "Flutter Rate", texture::flutterRateMinHz, texture::flutterRateMaxHz, d.flutterRate);

            addPercent (layout, param::vinylAmount, "Vinyl Amount", 20.0f);
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::crackleType), "Crackle Type",
                juce::StringArray { "Normal", "Dusty", "Heavy" }, (int) CrackleType::normal));
            addPercent (layout, param::texVinylDrop, "Vinyl Drop", d.vinylDrop * 100.0f);
            addPercent (layout, param::texDegradeAmount, "Degrade", d.degrade * 100.0f);
        }

        void addColorParameters (juce::AudioProcessorParameterGroup& layout)
        {
            const ColorParameters d; // defaults live in one place: ColorParameters

            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::colEnable), "Color On", true));
            addPercent (layout, param::colMix, "Color Dry/Wet", 100.0f);

            addPercent (layout, param::colDrive, "Drive", d.drive * 100.0f);
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::colType), "Color Type",
                juce::StringArray { "Tube", "Tape", "Console" }, (int) d.type));

            layout.addChild (std::make_unique<juce::AudioParameterFloat> (
                makeId (param::colTone), "Tone", juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f),
                d.tone * 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (formatSignedPercent)));

            addPercent (layout, param::colMagicChorus, "Magic Chorus", d.magicChorus * 100.0f);

            // Plugin default is 2x (better quality for the price of a little CPU); the Core default stays Off.
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::colOversampling), "Oversampling",
                juce::StringArray { "Off", "2x", "4x" }, (int) OversamplingMode::x2));
        }

        void addMovementParameters (juce::AudioProcessorParameterGroup& layout)
        {
            const MovementParameters d; // defaults live in one place: MovementParameters

            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::movEnable), "Movement On", true));
            addPercent (layout, param::movMix, "Movement Dry/Wet", 100.0f);

            addPercent (layout, param::movGrain, "Grain", d.grain * 100.0f);
            addPercent (layout, param::movFeedback, "Feedback", d.feedback * 100.0f);
            addPercent (layout, param::movTremolo, "Tremolo", d.tremolo * 100.0f);
            addHz (layout, param::movRate, "Rate", movement::rateMinHz, movement::rateMaxHz, d.rateHz);
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::movStyle), "Style",
                juce::StringArray { "Frozen", "Drift", "Cloud" }, (int) d.style));
        }

        // Time is 0..100 %. Free: 20 ms..1.5 s (exponential). With Sync it picks a
        // division, so the text shows both: "215 ms | 1/8" (the division applies when Sync is on).
        juce::String formatSpaceTime (float percent, int)
        {
            const float t = percent / 100.0f;
            const float ms = space::freeTimeSeconds (t) * 1000.0f;
            return (ms < 100.0f ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms | "
                   + space::divisionNames[(std::size_t) space::divisionIndex (t)];
        }

        void addSpaceParameters (juce::AudioProcessorParameterGroup& layout)
        {
            const SpaceParameters d; // defaults live in one place: SpaceParameters

            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::spcEnable), "Space On", true));
            addPercent (layout, param::spcMix, "Space Dry/Wet", 100.0f);

            addPercent (layout, param::spcDelay, "Delay", d.delay * 100.0f);
            layout.addChild (std::make_unique<juce::AudioParameterFloat> (
                makeId (param::spcTime), "Time", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), d.time * 100.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatSpaceTime)));
            addPercent (layout, param::spcFeedback, "Delay Feedback", d.feedback * 100.0f);
            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::spcSync), "Sync", d.sync));

            addPercent (layout, param::spcReverbMix, "Reverb Mix", d.mix * 100.0f);
            addPercent (layout, param::spcDecay, "Decay", d.decay * 100.0f);
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::spcType), "Reverb Type",
                juce::StringArray { "Room", "Plate", "Hall" }, (int) d.type));

            addPercent (layout, param::spcWidth, "Width", d.width * 100.0f);
        }

        void addEngineParameters (juce::AudioProcessorParameterGroup& layout)
        {
            const VintageEngineParameters d; // defaults live in one place: VintageEngineParameters

            layout.addChild (std::make_unique<juce::AudioParameterBool> (makeId (param::engEnable), "Vintage Engine On", d.enabled));
            addPercent (layout, param::engFlux, "Flux", d.flux * 100.0f);
            addPercent (layout, param::engRate, "Engine Rate", d.rate * 100.0f);
            addPercent (layout, param::engDepth, "Engine Depth", d.depth * 100.0f);
            layout.addChild (std::make_unique<juce::AudioParameterChoice> (
                makeId (param::engCharacter), "Character",
                juce::StringArray { "Slow", "Organic", "Random" }, (int) d.character));
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createLayout()
    {
        using Attributes = juce::AudioParameterFloatAttributes;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        const juce::NormalisableRange<float> gainRange { param::gainMinDb, param::gainMaxDb, 0.01f };
        const juce::NormalisableRange<float> mixRange { param::mixMinPercent, param::mixMaxPercent, 0.01f };

        // One group per section, in signal-chain order. Groups only change how hosts
        // and the generic editor list the parameters; IDs and saved states are unchanged.
        const auto makeGroup = [] (const char* groupId, const char* name)
        {
            return std::make_unique<juce::AudioProcessorParameterGroup> (groupId, name, " | ");
        };

        auto master = makeGroup ("grpMaster", "Master");
        master->addChild (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::inputGain), "Input Gain", gainRange, param::gainDefaultDb,
            Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));

        master->addChild (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::outputGain), "Output Gain", gainRange, param::gainDefaultDb,
            Attributes().withLabel ("dB").withStringFromValueFunction (formatDb)));

        master->addChild (std::make_unique<juce::AudioParameterFloat> (
            makeId (param::masterMix), "Master Dry/Wet", mixRange, param::mixDefaultPercent,
            Attributes().withLabel ("%").withStringFromValueFunction (formatPercent)));

        master->addChild (std::make_unique<juce::AudioParameterBool> (
            makeId (param::masterBypass), "Bypass", false));

        addPercent (*master, param::magnitude, "Magnitude", 100.0f);

        auto textureGroup = makeGroup ("grpTexture", "Texture");
        addTextureParameters (*textureGroup);
        auto colorGroup = makeGroup ("grpColor", "Color");
        addColorParameters (*colorGroup);
        auto movementGroup = makeGroup ("grpMovement", "Movement");
        addMovementParameters (*movementGroup);
        auto spaceGroup = makeGroup ("grpSpace", "Space");
        addSpaceParameters (*spaceGroup);
        auto engineGroup = makeGroup ("grpVintageEngine", "Vintage Engine");
        addEngineParameters (*engineGroup);

        layout.add (std::move (master), std::move (textureGroup), std::move (colorGroup),
                    std::move (movementGroup), std::move (spaceGroup), std::move (engineGroup));

        return layout;
    }

    ParameterManager::ParameterManager (juce::AudioProcessor& owner)
        : state (owner, nullptr, stateType, createLayout())
    {
        inputGain = requireParameter (state, param::inputGain);
        outputGain = requireParameter (state, param::outputGain);
        masterMix = requireParameter (state, param::masterMix);
        masterBypass = requireParameter (state, param::masterBypass);
        vinylAmount = requireParameter (state, param::vinylAmount);
        crackleType = requireParameter (state, param::crackleType);
        magnitude = requireParameter (state, param::magnitude);
        bypassParameter = state.getParameter (param::masterBypass);

        tex.enable = requireParameter (state, param::texEnable);
        tex.mix = requireParameter (state, param::texMix);
        tex.wowDepth = requireParameter (state, param::texWowDepth);
        tex.wowRate = requireParameter (state, param::texWowRate);
        tex.flutterDepth = requireParameter (state, param::texFlutterDepth);
        tex.flutterRate = requireParameter (state, param::texFlutterRate);
        tex.vinylDrop = requireParameter (state, param::texVinylDrop);
        tex.degrade = requireParameter (state, param::texDegradeAmount);

        col.enable = requireParameter (state, param::colEnable);
        col.mix = requireParameter (state, param::colMix);
        col.drive = requireParameter (state, param::colDrive);
        col.type = requireParameter (state, param::colType);
        col.tone = requireParameter (state, param::colTone);
        col.oversampling = requireParameter (state, param::colOversampling);
        col.magicChorus = requireParameter (state, param::colMagicChorus);

        mov.enable = requireParameter (state, param::movEnable);
        mov.mix = requireParameter (state, param::movMix);
        mov.grain = requireParameter (state, param::movGrain);
        mov.feedback = requireParameter (state, param::movFeedback);
        mov.tremolo = requireParameter (state, param::movTremolo);
        mov.rate = requireParameter (state, param::movRate);
        mov.style = requireParameter (state, param::movStyle);

        spc.enable = requireParameter (state, param::spcEnable);
        spc.mix = requireParameter (state, param::spcMix);
        spc.delay = requireParameter (state, param::spcDelay);
        spc.time = requireParameter (state, param::spcTime);
        spc.feedback = requireParameter (state, param::spcFeedback);
        spc.sync = requireParameter (state, param::spcSync);
        spc.reverbMix = requireParameter (state, param::spcReverbMix);
        spc.decay = requireParameter (state, param::spcDecay);
        spc.type = requireParameter (state, param::spcType);
        spc.width = requireParameter (state, param::spcWidth);

        eng.enable = requireParameter (state, param::engEnable);
        eng.flux = requireParameter (state, param::engFlux);
        eng.rate = requireParameter (state, param::engRate);
        eng.depth = requireParameter (state, param::engDepth);
        eng.character = requireParameter (state, param::engCharacter);
    }

    MasterParameters ParameterManager::readMasterParameters() const noexcept
    {
        MasterParameters p;
        p.inputGainDb = inputGain->load (std::memory_order_relaxed);
        p.outputGainDb = outputGain->load (std::memory_order_relaxed);
        p.mix = masterMix->load (std::memory_order_relaxed) / param::mixMaxPercent;
        p.vinylAmount = vinylAmount->load (std::memory_order_relaxed) / 100.0f;
        p.bypass = masterBypass->load (std::memory_order_relaxed) >= 0.5f;
        p.crackleType = (CrackleType) juce::jlimit (0, 2, juce::roundToInt (crackleType->load (std::memory_order_relaxed)));
        return p;
    }

    float ParameterManager::readMagnitude() const noexcept
    {
        return magnitude->load (std::memory_order_relaxed) / 100.0f;
    }

    ChainParameters ParameterManager::readChainParameters() const noexcept
    {
        ChainParameters c;
        c.master = readMasterParameters();
        c.magnitude = readMagnitude();
        c.textureSlot = readTextureSlot();
        c.colorSlot = readColorSlot();
        c.movementSlot = readMovementSlot();
        c.spaceSlot = readSpaceSlot();
        c.texture = readTextureParameters();
        c.color = readColorParameters();
        c.movement = readMovementParameters();
        c.space = readSpaceParameters();
        c.engine = readEngineParameters();
        return c;
    }

    ParameterManager::SlotState ParameterManager::readTextureSlot() const noexcept
    {
        return { tex.enable->load (std::memory_order_relaxed) >= 0.5f,
                 tex.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    TextureParameters ParameterManager::readTextureParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };
        const auto raw = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };

        TextureParameters p;
        p.wowDepth = pct (tex.wowDepth);
        p.wowRate = raw (tex.wowRate);
        p.flutterDepth = pct (tex.flutterDepth);
        p.flutterRate = raw (tex.flutterRate);
        p.vinylDrop = pct (tex.vinylDrop);
        p.degrade = pct (tex.degrade);
        return p;
    }

    ParameterManager::SlotState ParameterManager::readColorSlot() const noexcept
    {
        return { col.enable->load (std::memory_order_relaxed) >= 0.5f,
                 col.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    ColorParameters ParameterManager::readColorParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };

        ColorParameters p;
        p.drive = pct (col.drive);
        p.type = (ColorType) juce::jlimit (0, 2, juce::roundToInt (col.type->load (std::memory_order_relaxed)));
        p.tone = pct (col.tone);
        p.magicChorus = pct (col.magicChorus);
        p.oversampling = (OversamplingMode) juce::jlimit (0, 2, juce::roundToInt (col.oversampling->load (std::memory_order_relaxed)));
        return p;
    }

    ParameterManager::SlotState ParameterManager::readMovementSlot() const noexcept
    {
        return { mov.enable->load (std::memory_order_relaxed) >= 0.5f,
                 mov.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    MovementParameters ParameterManager::readMovementParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };

        MovementParameters p;
        p.grain = pct (mov.grain);
        p.feedback = pct (mov.feedback);
        p.tremolo = pct (mov.tremolo);
        p.rateHz = mov.rate->load (std::memory_order_relaxed);
        p.style = (MovementStyle) juce::jlimit (0, 2, juce::roundToInt (mov.style->load (std::memory_order_relaxed)));
        return p;
    }

    ParameterManager::SlotState ParameterManager::readSpaceSlot() const noexcept
    {
        return { spc.enable->load (std::memory_order_relaxed) >= 0.5f,
                 spc.mix->load (std::memory_order_relaxed) / 100.0f };
    }

    SpaceParameters ParameterManager::readSpaceParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };

        SpaceParameters p;
        p.delay = pct (spc.delay);
        p.time = pct (spc.time);
        p.feedback = pct (spc.feedback);
        p.sync = spc.sync->load (std::memory_order_relaxed) >= 0.5f;
        p.mix = pct (spc.reverbMix);
        p.decay = pct (spc.decay);
        p.type = (ReverbType) juce::jlimit (0, 2, juce::roundToInt (spc.type->load (std::memory_order_relaxed)));
        p.width = pct (spc.width);
        return p;
    }

    VintageEngineParameters ParameterManager::readEngineParameters() const noexcept
    {
        const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) / 100.0f; };

        VintageEngineParameters p;
        p.enabled = eng.enable->load (std::memory_order_relaxed) >= 0.5f;
        p.flux = pct (eng.flux);
        p.rate = pct (eng.rate);
        p.depth = pct (eng.depth);
        p.character = (EngineCharacter) juce::jlimit (0, 2, juce::roundToInt (eng.character->load (std::memory_order_relaxed)));
        return p;
    }

    void ParameterManager::saveState (juce::MemoryBlock& destination)
    {
        auto tree = state.copyState();
        tree.setProperty (versionAttribute, param::stateVersion, nullptr);

        if (auto xml = tree.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, destination);
    }

    void ParameterManager::loadState (const void* data, int sizeInBytes)
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);

        if (xml == nullptr || ! xml->hasTagName (stateType))
            return; // not ours or corrupt: keep the current state

        const int version = xml->getIntAttribute (versionAttribute, 1);

        if (version > param::stateVersion)
            return; // saved by a newer Vintage Maker: refuse rather than misread

        // Missing parameters (older states) keep their current values; replaceState
        // applies every parameter found in the tree. Parameters that no longer
        // exist are ignored.
        state.replaceState (juce::ValueTree::fromXml (*xml));
    }
}
