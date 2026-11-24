#include "ModelManifest.h"
#include "BinaryData.h"

namespace
{
    constexpr const char* kManifestUrl   = "https://www.leosolproducts.com/musicaguide/models.json";
    constexpr int         kMaxManifestBytes = 64 * 1024;
    constexpr int         kSupportedSchema  = 1;

    juce::StringArray readStringArray (const juce::var& v, int maxItems = 32)
    {
        juce::StringArray out;
        if (auto* arr = v.getArray())
            for (auto& item : *arr)
            {
                if (out.size() >= maxItems) break;
                auto s = item.toString().trim();
                if (s.isNotEmpty() && s.length() <= 100)
                    out.add (s);
            }
        return out;
    }
}

// Owned thread rather than Thread::launch: a plugin binary can be unloaded by the host
// while a detached thread is still running inside it. The singleton's destructor runs
// at unload and stops this thread first.
class ModelManifest::FetchThread : public juce::Thread
{
public:
    explicit FetchThread (ModelManifest& m) : juce::Thread ("Musicaguide model manifest"), owner (m) {}

    void run() override
    {
        juce::URL url (kManifestUrl);
        int status = 0;
        auto stream = url.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (8000)
                .withStatusCode (&status));

        if (threadShouldExit() || stream == nullptr || status != 200)
            return;  // offline or site down: keep the cached/builtin copy

        juce::MemoryBlock body;
        stream->readIntoMemoryBlock (body, kMaxManifestBytes + 1);
        if (threadShouldExit() || (int) body.getSize() > kMaxManifestBytes)
            return;

        auto json = body.toString();
        Data fresh;
        if (! parse (json, fresh))
            return;

        {
            const juce::ScopedLock sl (owner.lock);
            owner.data = std::move (fresh);
        }

        auto file = cacheFile();
        file.getParentDirectory().createDirectory();
        file.replaceWithText (json);
    }

private:
    ModelManifest& owner;
};

ModelManifest& ModelManifest::get()
{
    static ModelManifest instance;
    return instance;
}

ModelManifest::ModelManifest()
{
    // 1. Compiled-in snapshot: always valid, so there is never a "no manifest" state.
    Data builtin;
    const juce::String builtinJson (BinaryData::models_json, (size_t) BinaryData::models_jsonSize);
    const bool builtinOk = parse (builtinJson, builtin);
    jassert (builtinOk);  // the shipped resources/models.json must always parse
    juce::ignoreUnused (builtinOk);
    data = std::move (builtin);

    // 2. Last good copy from the website, if any. It wins over the snapshot because it
    //    is newer than the build. A corrupt cache is ignored, never fatal.
    auto cached = cacheFile();
    if (cached.existsAsFile() && cached.getSize() <= kMaxManifestBytes)
    {
        Data fromCache;
        if (parse (cached.loadFileAsString(), fromCache))
            data = std::move (fromCache);
    }
}

juce::File ModelManifest::cacheFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Musicaguide")
             .getChildFile ("models.json");
}

bool ModelManifest::isValidModelId (const juce::String& id)
{
    if (id.isEmpty() || id.length() > 100)
        return false;
    return id.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._:/-");
}

bool ModelManifest::parse (const juce::String& json, Data& out)
{
    auto root = juce::JSON::parse (json);
    auto* obj = root.getDynamicObject();
    if (obj == nullptr)
        return false;

    if ((int) obj->getProperty ("schema") != kSupportedSchema)
        return false;  // a future schema we don't understand: keep what we have

    auto* provs = obj->getProperty ("providers").getDynamicObject();
    if (provs == nullptr)
        return false;

    Data result;
    result.updated = obj->getProperty ("updated").toString().substring (0, 32);

    for (auto& name : { juce::String ("openai"), juce::String ("anthropic"), juce::String ("groq") })
    {
        auto* p = provs->getProperty (name).getDynamicObject();
        if (p == nullptr)
            return false;  // every provider must be present, or the file is rejected whole

        ProviderConfig cfg;
        cfg.defaultModel  = p->getProperty ("default").toString().trim();
        cfg.fallbackModel = p->getProperty ("fallback").toString().trim();
        cfg.suggested     = readStringArray (p->getProperty ("suggested"));
        cfg.listInclude   = readStringArray (p->getProperty ("list_include"));
        cfg.listExclude   = readStringArray (p->getProperty ("list_exclude"), 64);

        if (! isValidModelId (cfg.defaultModel) || ! isValidModelId (cfg.fallbackModel))
            return false;

        juce::StringArray cleanSuggested;
        for (auto& s : cfg.suggested)
            if (isValidModelId (s))
                cleanSuggested.addIfNotAlreadyThere (s);
        cfg.suggested = cleanSuggested;
        if (! cfg.suggested.contains (cfg.defaultModel))
            cfg.suggested.insert (0, cfg.defaultModel);

        // Only the two parameter names the providers actually accept.
        auto tokenParam = p->getProperty ("token_param").toString();
        if (tokenParam != "max_tokens" && tokenParam != "max_completion_tokens")
            return false;
        cfg.tokenParam = tokenParam;

        cfg.maxOutputTokens = juce::jlimit (256, 16384, (int) p->getProperty ("max_output_tokens"));
        cfg.sendTemperature = (bool) p->getProperty ("send_temperature");
        if (p->hasProperty ("temperature"))
            cfg.temperature = juce::jlimit (0.0, 2.0, (double) p->getProperty ("temperature"));

        result.providers[name] = cfg;
    }

    if (auto* retired = obj->getProperty ("retired").getDynamicObject())
    {
        for (auto& provEntry : retired->getProperties())
        {
            auto* map = provEntry.value.getDynamicObject();
            if (map == nullptr) continue;

            auto& list = result.retired[provEntry.name.toString()];
            for (auto& m : map->getProperties())
            {
                auto from = m.name.toString();
                auto to   = m.value.toString().trim();
                auto fromId = from.endsWithChar ('*') ? from.dropLastCharacters (1) : from;
                if (isValidModelId (fromId) && isValidModelId (to) && list.size() < 200)
                    list.emplace_back (from, to);
            }
        }
    }

    out = std::move (result);
    return true;
}

ModelManifest::~ModelManifest()
{
    if (fetchThread != nullptr)
        fetchThread->stopThread (2000);
}

void ModelManifest::refreshInBackground()
{
    if (refreshStarted.exchange (true))
        return;

    fetchThread = std::make_unique<FetchThread> (*this);
    fetchThread->startThread();
}

ModelManifest::ProviderConfig ModelManifest::forProvider (const juce::String& provider) const
{
    const juce::ScopedLock sl (lock);
    auto it = data.providers.find (provider);
    if (it != data.providers.end())
        return it->second;
    return data.providers.at ("openai");
}

juce::String ModelManifest::replacementFor (const juce::String& provider,
                                            const juce::String& model) const
{
    const juce::ScopedLock sl (lock);
    auto it = data.retired.find (provider);
    if (it == data.retired.end() || model.isEmpty())
        return {};

    // Exact match wins; otherwise the longest matching prefix, so file order never matters.
    juce::String best;
    int bestLen = -1;
    for (auto& [from, to] : it->second)
    {
        if (from.endsWithChar ('*'))
        {
            auto prefix = from.dropLastCharacters (1);
            if (model.startsWithIgnoreCase (prefix) && prefix.length() > bestLen)
            {
                best = to;
                bestLen = prefix.length();
            }
        }
        else if (model.equalsIgnoreCase (from))
        {
            return to;
        }
    }

    // Never "replace" a model with itself (would loop on every load).
    return best.equalsIgnoreCase (model) ? juce::String() : best;
}

juce::String ModelManifest::getUpdatedDate() const
{
    const juce::ScopedLock sl (lock);
    return data.updated;
}
