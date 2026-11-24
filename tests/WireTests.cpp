// Unit tests for the model manifest and provider wire helpers. No network, no host.
// Build: cmake -B build -DMUSICAGUIDE_TESTS=ON && cmake --build build --target MusicaguideTests
#include <JuceHeader.h>
#include "ModelManifest.h"
#include "LLMWire.h"

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << "\n";
    if (! ok) ++failures;
}

static juce::var J (const char* s) { return juce::JSON::parse (juce::String (s)); }

static juce::String builtinJson()
{
    return juce::String (BinaryData::models_json, (size_t) BinaryData::models_jsonSize);
}

static void manifestTests()
{
    auto& m = ModelManifest::get();
    check (ModelManifest::validate (builtinJson()), "shipped models.json parses");

    auto openai = m.forProvider ("openai");
    check (openai.defaultModel == "gpt-6-luna", "openai default");
    check (openai.tokenParam == "max_completion_tokens", "openai uses max_completion_tokens");
    check (! openai.sendTemperature, "openai omits temperature");
    check (m.forProvider ("anthropic").tokenParam == "max_tokens", "anthropic uses max_tokens");
    check (m.forProvider ("nonsense").defaultModel == openai.defaultModel, "unknown provider falls back to openai config");

    check (m.replacementFor ("openai", "gpt-4o-mini") == "gpt-6-luna", "gpt-4o-mini retired -> luna");
    check (m.replacementFor ("openai", "gpt-4o-mini-2024-07-18") == "gpt-6-luna", "dated 4o-mini snapshot -> luna");
    check (m.replacementFor ("openai", "gpt-4o") == "gpt-6-luna", "gpt-4o -> luna");
    check (m.replacementFor ("openai", "gpt-5") == "gpt-6.1-sol", "bare gpt-5 -> 6.1-sol");
    check (m.replacementFor ("openai", "gpt-5-mini-2025-08-07") == "gpt-6.1-terra", "gpt-5-mini snapshot -> 6.1-terra");
    check (m.replacementFor ("openai", "gpt-5.6-sol").isEmpty(), "gpt-5.6-sol is NOT caught by the gpt-5 rule");
    check (m.replacementFor ("openai", "gpt-6-luna").isEmpty(), "current default is not retired");
    check (m.replacementFor ("groq", "llama-3.3-70b-versatile") == "openai/gpt-oss-120b", "groq llama -> gpt-oss-120b");
    check (m.replacementFor ("groq", "meta-llama/llama-4-scout-17b-16e-instruct") == "openai/gpt-oss-120b", "groq meta-llama/* -> 120b");
    check (m.replacementFor ("groq", "openai/gpt-oss-120b").isEmpty(), "groq default not retired");
    check (m.replacementFor ("anthropic", "claude-haiku-5-5").isEmpty(), "anthropic default not retired");
    check (m.replacementFor ("openai", "").isEmpty(), "empty model -> no replacement");

    // Every default and suggestion must be valid and not itself retired.
    for (auto prov : { "openai", "anthropic", "groq" })
    {
        auto cfg = m.forProvider (prov);
        check (m.replacementFor (prov, cfg.defaultModel).isEmpty(), juce::String (prov) + " default not on retired list");
        check (m.replacementFor (prov, cfg.fallbackModel).isEmpty(), juce::String (prov) + " fallback not on retired list");
        for (auto& s : cfg.suggested)
            check (m.replacementFor (prov, s).isEmpty(), juce::String (prov) + " suggestion " + s + " not retired");
    }

    check (ModelManifest::isValidModelId ("openai/gpt-oss-120b"), "valid id with slash");
    check (! ModelManifest::isValidModelId ("gpt\"; rm -rf"), "id with quotes/spaces rejected");
    check (! ModelManifest::isValidModelId (""), "empty id rejected");
    check (! ModelManifest::isValidModelId (juce::String::repeatedString ("a", 101)), "101-char id rejected");

    // Rejections: the whole file must be thrown away, never half-applied.
    auto base = builtinJson();
    check (! ModelManifest::validate ("not json"), "garbage rejected");
    check (! ModelManifest::validate (base.replace ("\"schema\": 1", "\"schema\": 2")), "future schema rejected");
    check (! ModelManifest::validate (base.replace ("\"max_completion_tokens\"", "\"max_output_tokens_typo\"")), "unknown token_param rejected");
    check (! ModelManifest::validate (base.replace ("\"default\": \"gpt-6-luna\"", "\"default\": \"bad id\"")), "bad default id rejected");
    check (! ModelManifest::validate (base.replace ("\"groq\": {", "\"groqx\": {")), "missing provider rejected");
}

static void wireTests()
{
    using namespace LLMWire;
    auto& m = ModelManifest::get();

    juce::Array<LLMProvider::Message> msgs;
    msgs.add ({ "user", "What notes are in D dorian?" });

    auto oa = juce::JSON::parse (buildOpenAIRequest ("gpt-6-luna", m.forProvider ("openai"), msgs));
    auto* oaObj = oa.getDynamicObject();
    check (oaObj != nullptr && oaObj->hasProperty ("max_completion_tokens"), "openai body sends max_completion_tokens");
    check (oaObj != nullptr && ! oaObj->hasProperty ("max_tokens"), "openai body does not send max_tokens");
    check (oaObj != nullptr && ! oaObj->hasProperty ("temperature"), "openai body omits temperature");

    auto gq = juce::JSON::parse (buildOpenAIRequest ("openai/gpt-oss-120b", m.forProvider ("groq"), msgs));
    check (gq.getDynamicObject()->hasProperty ("max_tokens"), "groq body keeps max_tokens");
    check (gq.getDynamicObject()->hasProperty ("temperature"), "groq body keeps temperature");

    auto an = juce::JSON::parse (buildAnthropicRequest ("claude-haiku-5-5", m.forProvider ("anthropic"), msgs));
    check ((int) an.getDynamicObject()->getProperty ("max_tokens") == 2048, "anthropic max_tokens from manifest");

    bool len = false;
    check (extractAnthropicContent (J (R"({"content":[{"type":"thinking","thinking":"hmm"},{"type":"text","text":"D E F G A B C"}],"stop_reason":"end_turn"})"), len)
             == "D E F G A B C", "anthropic: answer after a thinking block is found");
    check (extractAnthropicContent (J (R"({"content":[{"type":"text","text":"Part 1. "},{"type":"text","text":"Part 2."}]})"), len)
             == "Part 1. Part 2.", "anthropic: multiple text blocks joined");
    extractAnthropicContent (J (R"({"content":[],"stop_reason":"max_tokens"})"), len);
    check (len, "anthropic: max_tokens stop detected");

    check (extractOpenAIContent (J (R"({"choices":[{"message":{"content":" Dm7 "},"finish_reason":"stop"}]})"), len) == "Dm7", "openai: content trimmed");
    check (extractOpenAIContent (J (R"({"choices":[{"message":{"content":null},"finish_reason":"length"}]})"), len).isEmpty() && len,
           "openai: null content + length limit detected (reasoning ate the budget)");

    check (isModelUnavailableError (404, J (R"({"error":{"message":"The model `gpt-4o` does not exist","code":"model_not_found"}})")), "404 model_not_found -> fallback");
    check (isModelUnavailableError (400, J (R"({"error":{"message":"The model `llama-3.3-70b-versatile` has been decommissioned","code":"model_decommissioned"}})")), "groq decommissioned -> fallback");
    check (isModelUnavailableError (404, J (R"({"type":"error","error":{"type":"not_found_error","message":"model: claude-x"}})")), "anthropic not_found_error -> fallback");
    check (! isModelUnavailableError (401, J (R"({"error":{"message":"Incorrect API key provided"}})")), "bad key is NOT a model error");
    check (! isModelUnavailableError (429, J (R"({"error":{"message":"Rate limit reached for model gpt-6-luna"}})")), "rate limit is NOT a model error");
    check (! isModelUnavailableError (400, J (R"({"error":{"message":"messages: roles must alternate"}})")), "unrelated 400 is NOT a model error");
    check (! isModelUnavailableError (500, J (R"({"error":{"message":"The model is overloaded"}})")), "500 overload is NOT a model error");
    check (apiErrorMessage (J (R"({"error":{"message":"boom"}})")) == "boom", "error message extracted");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;  // MessageManager exists for LLMProvider's statics
    manifestTests();
    wireTests();
    std::cout << "\n" << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
