#include "ChatComponent.h"
#include "Analytics.h"
#include "UIColors.h"

static constexpr float kPadding   = 12.f;
static constexpr float kBubbleR   =  8.f;
static constexpr float kBubbleGap =  8.f;
static constexpr float kMaxBubble = 0.72f;

// ─── MessageListContent ───────────────────────────────────────────────────────
void MessageListContent::addEntry (const ChatEntry& entry)
{
    LayoutRow row;
    row.entry = entry;
    rows.add (std::move (row));
}

void MessageListContent::clear()
{
    rows.clear();
    totalHeight = 0.f;
    setSize (getWidth(), 0);
}

void MessageListContent::rebuildLayout (int availableWidth)
{
    totalHeight = kPadding;
    juce::Font font (14.f);

    for (auto& row : rows)
    {
        float bubbleW = (float) availableWidth * kMaxBubble - kPadding * 2.f;
        bubbleW = juce::jmax (100.f, bubbleW);

        juce::AttributedString as;
        as.setWordWrap (juce::AttributedString::byWord);
        as.setJustification (juce::Justification::topLeft);
        as.append (row.entry.text, font, Colors::textMain);

        row.layout.createLayout (as, bubbleW - kPadding * 2.f);

        row.height = row.layout.getHeight() + kPadding * 2.f;
        row.y      = totalHeight;
        totalHeight += row.height + kBubbleGap;
    }

    totalHeight += kPadding;
    setSize (availableWidth, (int) totalHeight);
}

void MessageListContent::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);

    int w = getWidth();

    for (auto& row : rows)
    {
        bool isUser = row.entry.role == ChatEntry::Role::User;
        bool isErr  = row.entry.role == ChatEntry::Role::Error;

        float bubbleMaxW = (float) w * kMaxBubble - kPadding;
        float bubbleW    = bubbleMaxW;
        float bubbleX    = isUser ? (float) w - bubbleW - kPadding : kPadding;

        juce::Rectangle<float> bubbleRect (bubbleX, row.y, bubbleW, row.height);

        juce::Colour fill = isErr  ? Colors::errBubble
                          : isUser ? Colors::userBubble
                                   : Colors::aiBubble;
        g.setColour (fill);
        g.fillRoundedRectangle (bubbleRect, kBubbleR);

        g.setColour (Colors::border.withAlpha (0.6f));
        g.drawRoundedRectangle (bubbleRect, kBubbleR, 0.5f);

        row.layout.draw (g, bubbleRect.reduced (kPadding, kPadding));
    }
}

// ─── ChatComponent ────────────────────────────────────────────────────────────
ChatComponent::ChatComponent (FreemiumManager& fm, LLMProvider& llm)
    : freemiumManager (fm), llmProvider (llm)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId,
                                               Colors::accent.withAlpha (0.6f));

    addAndMakeVisible (inputBox);
    inputBox.setMultiLine (false);
    inputBox.setReturnKeyStartsNewLine (false);
    inputBox.addListener (this);
    inputBox.setTextToShowWhenEmpty ("Ask about music theory, mixing, production...",
                                     Colors::textSub);
    inputBox.setFont (juce::Font (14.f));
    inputBox.setColour (juce::TextEditor::backgroundColourId,     Colors::surface2);
    inputBox.setColour (juce::TextEditor::textColourId,           Colors::textMain);
    inputBox.setColour (juce::TextEditor::outlineColourId,        Colors::border);
    inputBox.setColour (juce::TextEditor::focusedOutlineColourId, Colors::accent);

    addAndMakeVisible (sendButton);
    sendButton.setColour (juce::TextButton::buttonColourId,  Colors::accent);
    sendButton.setColour (juce::TextButton::textColourOnId,  Colors::textMain);
    sendButton.setColour (juce::TextButton::textColourOffId, Colors::textMain);
    sendButton.onClick = [this] { sendMessage(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (juce::Font (11.f));
    statusLabel.setColour (juce::Label::textColourId, Colors::textSub);
    statusLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (thinkingLabel);
    thinkingLabel.setFont (juce::Font (11.f, juce::Font::italic));
    thinkingLabel.setColour (juce::Label::textColourId, Colors::accentLt);
    thinkingLabel.setJustificationType (juce::Justification::centredRight);
    thinkingLabel.setVisible (false);
}

ChatComponent::~ChatComponent() {}

void ChatComponent::addWelcomeMessage()
{
    appendEntry (ChatEntry::Role::Assistant,
                 "Hey! I'm your Musicaguide. Ask me anything about music theory, "
                 "chords, scales, production, mixing, or arrangement — whatever "
                 "you're working on right now.");

    if (! freemiumManager.hasShownAnalyticsDisclosure())
    {
        freemiumManager.markAnalyticsDisclosureShown();
        juce::MessageManager::callAsync ([]
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "A note on privacy",
                "Musicaguide collects anonymous usage data (your questions and the AI's "
                "responses) to help improve the product. No API key or personal information "
                "is ever sent. You can turn this off at any time in Settings.");
        });
    }
}

void ChatComponent::keyStateChanged()
{
    updateStatus();
}

void ChatComponent::resized()
{
    auto area    = getLocalBounds();
    int inputPad = 8;

    auto statusArea = area.removeFromBottom (22).reduced (inputPad, 0);
    auto inputArea  = area.removeFromBottom (44).reduced (inputPad, inputPad / 2);

    int sendW = 64;
    sendButton.setBounds (inputArea.removeFromRight (sendW));
    inputArea.removeFromRight (4);
    inputBox.setBounds (inputArea);

    statusLabel.setBounds   (statusArea.removeFromLeft  (statusArea.getWidth() / 2));
    thinkingLabel.setBounds (statusArea);

    viewport.setBounds (area);
    content.rebuildLayout (area.getWidth());
    viewport.setViewPositionProportionately (0.0, 1.0);
    updateStatus();
}

void ChatComponent::paint (juce::Graphics& g)
{
    g.fillAll (Colors::bg);
    auto footer = getLocalBounds().removeFromBottom (22 + 44 + 8);
    g.setColour (Colors::surface);
    g.fillRect  (footer);
    g.setColour (Colors::border);
    g.drawHorizontalLine (footer.getY(), 0.f, (float) getWidth());
}

void ChatComponent::appendEntry (ChatEntry::Role role, const juce::String& text)
{
    content.addEntry ({ role, text });
    content.rebuildLayout (viewport.getWidth());
    viewport.setViewPositionProportionately (0.0, 1.0);
    repaint();
}

void ChatComponent::updateStatus()
{
    bool hasKey = freemiumManager.hasApiKey();

    inputBox.setEnabled (hasKey && !isWaiting);
    sendButton.setEnabled (hasKey && !isWaiting);

    if (hasKey)
    {
        statusLabel.setText ("API key active  \xc2\xb7  Unlimited", juce::dontSendNotification);
        inputBox.setTextToShowWhenEmpty ("Ask about music theory, mixing, production...",
                                        Colors::textSub);
    }
    else
    {
        statusLabel.setText ("Add your API key in Settings to start chatting.",
                             juce::dontSendNotification);
        inputBox.setTextToShowWhenEmpty ("Add your API key in Settings first.",
                                        Colors::textSub);
    }
}

void ChatComponent::sendMessage()
{
    if (isWaiting) return;
    if (!freemiumManager.hasApiKey()) return;

    auto text = inputBox.getText().trim();
    if (text.isEmpty()) return;

    inputBox.clear();
    appendEntry (ChatEntry::Role::User, text);

    LLMProvider::Message userMsg;
    userMsg.role    = "user";
    userMsg.content = text;
    history.add (userMsg);

    isWaiting = true;
    inputBox.setEnabled (false);
    sendButton.setEnabled (false);
    thinkingLabel.setText ("Thinking...", juce::dontSendNotification);
    thinkingLabel.setVisible (true);

    llmProvider.send (freemiumManager.getApiKey(),
                      freemiumManager.getProvider(),
                      freemiumManager.getModel(),
                      history,
        [this, text] (const juce::String& response, bool success, const juce::String& errMsg)
        {
            isWaiting = false;
            thinkingLabel.setVisible (false);
            updateStatus();

            if (success)
            {
                LLMProvider::Message asstMsg;
                asstMsg.role    = "assistant";
                asstMsg.content = response;
                history.add (asstMsg);
                appendEntry (ChatEntry::Role::Assistant, response);

                if (freemiumManager.isAnalyticsEnabled())
                    Analytics::logEvent (freemiumManager.getInstallId(),
                                         freemiumManager.getModel(),
                                         text,
                                         response);
            }
            else
            {
                appendEntry (ChatEntry::Role::Error, errMsg.isEmpty() ? "Unknown error" : errMsg);
            }
        });
}

void ChatComponent::textEditorReturnKeyPressed (juce::TextEditor&)
{
    sendMessage();
}
