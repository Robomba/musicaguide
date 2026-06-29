#pragma once
#include <JuceHeader.h>
#include "LLMProvider.h"
#include "FreemiumManager.h"

// ─── A single chat bubble ────────────────────────────────────────────────────
struct ChatEntry
{
    enum class Role { User, Assistant, Error };
    Role         role;
    juce::String text;
};

// ─── Scrollable message list ─────────────────────────────────────────────────
class MessageListContent : public juce::Component
{
public:
    void addEntry (const ChatEntry& entry);
    void clear();
    void rebuildLayout (int availableWidth);

private:
    struct LayoutRow
    {
        ChatEntry        entry;
        float            y      = 0.f;
        float            height = 0.f;
        juce::TextLayout layout;
    };

    juce::Array<LayoutRow> rows;
    float totalHeight = 0.f;

    void paint (juce::Graphics& g) override;
};

// ─── Main chat component ──────────────────────────────────────────────────────
class ChatComponent : public juce::Component,
                      private juce::TextEditor::Listener
{
public:
    ChatComponent (FreemiumManager& fm, LLMProvider& llm);
    ~ChatComponent() override;

    void resized() override;
    void paint  (juce::Graphics& g) override;

    void addWelcomeMessage();
    void keyStateChanged();   // call after API key is saved or cleared

private:
    void sendMessage();
    void appendEntry (ChatEntry::Role role, const juce::String& text);
    void updateStatus();
    void textEditorReturnKeyPressed (juce::TextEditor&) override;

    FreemiumManager& freemiumManager;
    LLMProvider&     llmProvider;

    juce::Array<LLMProvider::Message> history;

    MessageListContent content;
    juce::Viewport     viewport;

    juce::TextEditor inputBox;
    juce::TextButton sendButton  { "Send" };
    juce::Label      statusLabel;
    juce::Label      thinkingLabel;

    bool isWaiting = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChatComponent)
};
