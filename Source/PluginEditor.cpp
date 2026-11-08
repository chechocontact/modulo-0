#include "PluginEditor.h"

#include "Core/ParameterIds.h"
#include "UI/Components/LevelSlider.h"
#include "UI/Modules/ModuleSection.h"
#include "UI/Theme/Palette.h"
#include "UI/Theme/Textures.h"
#include "UI/Theme/VintageLookAndFeel.h"

#include <vector>

namespace
{
    using namespace vm::ui;
    namespace id = vm::param;

    // Three-line "hamburger" button.
    class MenuButton : public juce::Button
    {
    public:
        MenuButton() : juce::Button ("menu") {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto area = getLocalBounds().toFloat().reduced (0.5f);
            g.setGradientFill (juce::ColourGradient (down ? colour::recess : juce::Colour (0xff2f3236), 0.0f, area.getY(),
                                                     colour::recess, 0.0f, area.getBottom(), false));
            g.fillRoundedRectangle (area, 4.0f);
            g.setColour (highlighted ? colour::cream : colour::creamDim);
            g.drawRoundedRectangle (area, 4.0f, 1.2f);

            g.setColour (colour::cream);
            for (int i = -1; i <= 1; ++i)
                g.fillRoundedRectangle (juce::Rectangle<float> (area.getWidth() * 0.48f, 2.5f)
                                            .withCentre (area.getCentre().translated (0.0f, (float) i * 8.0f)), 1.2f);
        }
    };
}

// Everything lives in a fixed design space (designWidth x designHeight) that the
// editor scales to the window.
class VintageMakerEditor::Content final : public juce::Component
{
public:
    explicit Content (VintageMakerAudioProcessor& p)
        : processor (p),
          wood (createWoodTexture (designWidth, designHeight)),
          metal (createBrushedMetalTexture (designWidth, designHeight))
    {
        setLookAndFeel (&lookAndFeel);
        setOpaque (true);

        auto& state = p.getParameterManager().getState();

        texture = makeSection (state, { "01", "Texture", id::texEnable, id::texMix, 56, false });
        texture->addGroup ("Wow");
        texture->addKnob (id::texWowDepth, "Depth");
        texture->addKnob (id::texWowRate, "Rate");
        texture->addGroup ("Flutter");
        texture->addKnob (id::texFlutterDepth, "Depth");
        texture->addKnob (id::texFlutterRate, "Rate");
        texture->addGroup ("Vinyl");
        texture->addKnob (id::vinylAmount, "Amount");
        texture->addKnob (id::texVinylDrop, "Drop");
        texture->addChoice (id::crackleType, "Crackle");
        texture->addGroup ("Degrade");
        texture->addKnob (id::texDegradeAmount, "Amount");

        color = makeSection (state, { "02", "Color", id::colEnable, id::colMix, 76, false });
        color->addGroup ("Saturation");
        color->addKnob (id::colDrive, "Drive");
        color->addChoice (id::colType, "Type");
        color->addGroup ("Tone");
        color->addKnob (id::colTone, "Tone");
        color->addGroup ("Magic Chorus");
        color->addKnob (id::colMagicChorus, "Amount");

        movement = makeSection (state, { "03", "Movement", id::movEnable, id::movMix, 70, false });
        movement->addGroup ("Granular");
        movement->addKnob (id::movGrain, "Grain");
        movement->addKnob (id::movFeedback, "Feedback");
        movement->addChoice (id::movStyle, "Style");
        movement->addGroup ("Tremolo");
        movement->addKnob (id::movTremolo, "Depth");
        movement->addKnob (id::movRate, "Rate");

        space = makeSection (state, { "04", "Space", id::spcEnable, id::spcMix, 56, false });
        space->addGroup ("Delay");
        space->addKnob (id::spcDelay, "Amount");
        space->addKnob (id::spcTime, "Time");
        space->addKnob (id::spcFeedback, "Feedback");
        space->addToggle (id::spcSync, "Sync");
        space->addGroup ("Reverb");
        space->addKnob (id::spcReverbMix, "Amount");
        space->addKnob (id::spcDecay, "Decay");
        space->addChoice (id::spcType, "Type");
        space->addGroup ("Stereo field");
        space->addKnob (id::spcWidth, "Width");

        engine = makeSection (state, { "05", "Vintage Engine", id::engEnable, nullptr, 52, true });
        engine->addGroup ("Organic variation");
        engine->addKnob (id::engFlux, "Flux");
        engine->addKnob (id::engRate, "Rate");
        engine->addKnob (id::engDepth, "Depth");
        engine->addChoice (id::engCharacter, "Character");

        master = makeSection (state, { {}, "Master", nullptr, nullptr, 46, false });
        master->addGroup ("Master");
        master->addKnob (id::masterMix, "Dry/Wet");
        master->addKnob (id::magnitude, "Magnitude");
        master->addToggle (id::masterBypass, "Bypass");

        inputLevel = std::make_unique<LevelSlider> (state, id::inputGain, "IN", p.getInputMeter());
        outputLevel = std::make_unique<LevelSlider> (state, id::outputGain, "OUT", p.getOutputMeter());
        addAndMakeVisible (*inputLevel);
        addAndMakeVisible (*outputLevel);

        // Preset bar: laid out as in the guide but disabled until Module 8
        // (presets) exists; nothing here pretends to work.
        addAndMakeVisible (menuButton);
        menuButton.onClick = [this] { showMainMenu(); };

        for (auto* button : { &loadButton, &saveButton })
        {
            addAndMakeVisible (*button);
            button->setEnabled (false);
        }

        for (auto* arrow : { &previousButton, &nextButton })
        {
            addAndMakeVisible (*arrow);
            arrow->setEnabled (false);
            arrow->setAlpha (0.4f);
        }
    }

    ~Content() override
    {
        setLookAndFeel (nullptr);
    }

    void refreshMeters()
    {
        inputLevel->refresh();
        outputLevel->refresh();
    }

    void resized() override
    {
        constexpr int margin = 14;
        constexpr int gap = 8;
        constexpr int headerHeight = 54;
        constexpr int engineHeight = 130;
        constexpr int bottomHeight = 112;

        const int rowHeight = (designHeight - 2 * margin - headerHeight - 3 * gap - engineHeight - gap - bottomHeight - gap) / 2;
        const int columnWidth = (designWidth - 2 * margin - gap) / 2;

        // Header.
        menuButton.setBounds (margin, margin + 4, 46, 46);
        loadButton.setBounds (430, margin + 4, 96, 46);
        previousButton.setBounds (540, margin + 4, 40, 46);
        presetDisplay = { 580, margin + 4, 300, 46 };
        nextButton.setBounds (880, margin + 4, 40, 46);
        saveButton.setBounds (934, margin + 4, 96, 46);

        // Modules.
        const int row1 = margin + headerHeight + gap;
        const int row2 = row1 + rowHeight + gap;
        const int engineY = row2 + rowHeight + gap;
        const int bottomY = engineY + engineHeight + gap;
        const int rightX = margin + columnWidth + gap;

        texture->setBounds (margin, row1, columnWidth, rowHeight);
        color->setBounds (rightX, row1, columnWidth, rowHeight);
        movement->setBounds (margin, row2, columnWidth, rowHeight);
        space->setBounds (rightX, row2, columnWidth, rowHeight);
        engine->setBounds (margin, engineY, designWidth - 2 * margin, engineHeight);
        master->setBounds (margin, bottomY, 240, bottomHeight);

        // IN / OUT plate (bottom right).
        const int plateWidth = 660, plateHeight = 76;
        ioPlate = { designWidth - margin - plateWidth, bottomY + (bottomHeight - plateHeight) / 2, plateWidth, plateHeight };
        inputLevel->setBounds (ioPlate.getX() + 20, ioPlate.getY() + 16, 300, 46);
        outputLevel->setBounds (ioPlate.getX() + 340, ioPlate.getY() + 16, 300, 46);
    }

    void paint (juce::Graphics& g) override
    {
        g.drawImageAt (wood, 0, 0);

        // Soft shadows under the faceplates.
        const juce::DropShadow shadow (juce::Colours::black.withAlpha (0.6f), 12, { 0, 5 });
        for (auto* section : { texture.get(), color.get(), movement.get(), space.get(), engine.get(), master.get() })
            shadow.drawForRectangle (g, section->getBounds());
        shadow.drawForRectangle (g, ioPlate);

        // IN / OUT plate.
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path outline;
            outline.addRoundedRectangle (ioPlate.toFloat(), 6.0f);
            g.reduceClipRegion (outline);
            g.drawImage (metal, ioPlate.getX(), ioPlate.getY(), ioPlate.getWidth(), ioPlate.getHeight(),
                         ioPlate.getX(), ioPlate.getY(), ioPlate.getWidth(), ioPlate.getHeight());
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (ioPlate.toFloat().reduced (0.5f), 6.0f, 1.5f);

        // Engraved title.
        const auto titleArea = juce::Rectangle<int> (76, 10, 340, 62);
        g.setFont (makeTitleFont (38.0f));
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawText ("Vintage Maker", titleArea.translated (2, 2), juce::Justification::centredLeft, false);
        g.setColour (colour::cream);
        g.drawText ("Vintage Maker", titleArea, juce::Justification::centredLeft, false);

        // Preset display (the preset system arrives with Module 8).
        g.setColour (colour::recess);
        g.fillRoundedRectangle (presetDisplay.toFloat(), 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawRoundedRectangle (presetDisplay.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (colour::amberDim.brighter (0.5f));
        g.setFont (makeFont (15.0f));
        g.drawText ("No presets yet (Module 8)", presetDisplay, juce::Justification::centred, false);
    }

private:
    std::unique_ptr<ModuleSection> makeSection (juce::AudioProcessorValueTreeState& state, SectionConfig config)
    {
        auto section = std::make_unique<ModuleSection> (state, std::move (config), &metal);
        addAndMakeVisible (*section);
        return section;
    }

    void showMainMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Reset all parameters");
        menu.addSeparator();
        menu.addItem (2, "Presets (available in Module 8)", false);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton),
                            [safe = juce::Component::SafePointer<Content> (this)] (int result)
                            {
                                if (safe != nullptr && result == 1)
                                    safe->resetAllParameters();
                            });
    }

    void resetAllParameters()
    {
        for (auto* parameter : processor.getParameters())
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }

    VintageMakerAudioProcessor& processor;

    VintageLookAndFeel lookAndFeel;
    juce::Image wood, metal;

    std::unique_ptr<ModuleSection> texture, color, movement, space, engine, master;
    std::unique_ptr<LevelSlider> inputLevel, outputLevel;

    MenuButton menuButton;
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE" };
    juce::ArrowButton previousButton { "previous", 0.5f, colour::cream }, nextButton { "next", 0.0f, colour::cream };

    juce::Rectangle<int> presetDisplay, ioPlate;
};

VintageMakerEditor::VintageMakerEditor (VintageMakerAudioProcessor& p)
    : juce::AudioProcessorEditor (p), content (std::make_unique<Content> (p))
{
    addAndMakeVisible (*content);

    setResizable (true, false);
    setResizeLimits (vm::ui::designWidth / 2, vm::ui::designHeight / 2, vm::ui::designWidth * 2, vm::ui::designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) vm::ui::designWidth / (double) vm::ui::designHeight);
    setSize (vm::ui::designWidth, vm::ui::designHeight);

    startTimerHz (30);
}

VintageMakerEditor::~VintageMakerEditor()
{
    stopTimer();
}

void VintageMakerEditor::resized()
{
    const float scale = (float) getWidth() / (float) vm::ui::designWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, vm::ui::designWidth, vm::ui::designHeight);
}

void VintageMakerEditor::timerCallback()
{
    content->refreshMeters();
}
