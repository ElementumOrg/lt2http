#include "i18n.h"

#include <algorithm>
#include <map>

#include <boost/algorithm/string.hpp>

#include <oatpp/core/base/Environment.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>

namespace lh {

namespace {

using Translations = std::map<std::string, std::map<std::string, std::string>>;

const Translations &translations() {
    static const Translations all = [] {
        Translations result;
        auto mapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        for (const auto &file : web_files) {
            auto const prefix = std::string_view("i18n/");
            auto const suffix = std::string_view(".json");
            if (file.path.size() <= prefix.size() + suffix.size() || file.path.substr(0, prefix.size()) != prefix ||
                file.path.substr(file.path.size() - suffix.size()) != suffix)
                continue;

            auto code = std::string(file.path.substr(prefix.size(), file.path.size() - prefix.size() - suffix.size()));
            try {
                auto texts = mapper->readFromString<oatpp::Fields<oatpp::String>>(oatpp::String(file.content.data(), file.content.size()));
                for (const auto &[key, text] : *texts) {
                    if (text)
                        result[code][*key] = *text;
                }
            } catch (const std::exception &e) {
                OATPP_LOGE("i18n", "Invalid translation %s: %s", code.c_str(), e.what());
            }
        }
        return result;
    }();
    return all;
}

} // namespace

const WebFile *find_web_file(std::string_view path) {
    auto match = std::find_if(web_files.begin(), web_files.end(), [&path](const WebFile &f) { return f.path == path; });
    return match == web_files.end() ? nullptr : &*match;
}

const std::vector<Language> &languages() {
    static const std::vector<Language> all = [] {
        std::vector<Language> result;
        for (const auto &[code, texts] : translations()) {
            auto name = texts.find("_name");
            result.push_back({code, name == texts.end() ? code : name->second});
        }
        return result;
    }();
    return all;
}

std::string resolve_language(const std::string &requested, const std::string &fallback) {
    auto code = boost::to_lower_copy(boost::trim_copy(requested));
    std::replace(code.begin(), code.end(), '-', '_');
    auto const &all = translations();
    if (all.count(code))
        return code;

    // Only a language ("de"), or a country without a translation ("es_co"): the first translation of the language
    auto language = code.substr(0, code.find('_'));
    if (!language.empty()) {
        // "es" is es_es rather than es_ar
        if (all.count(language + "_" + language))
            return language + "_" + language;
        auto match = std::find_if(all.begin(), all.end(), [&language](const auto &t) { return t.first.rfind(language + "_", 0) == 0; });
        if (match != all.end())
            return match->first;
    }
    return fallback;
}

std::string translate(const std::string &language, const std::string &key, std::initializer_list<std::string> args) {
    auto const &all = translations();
    auto find = [&all, &key](const std::string &code) -> const std::string * {
        auto texts = all.find(code);
        if (texts == all.end())
            return nullptr;
        auto text = texts->second.find(key);
        return text == texts->second.end() || text->second.empty() ? nullptr : &text->second;
    };

    const std::string *text = find(language);
    if (!text)
        text = find(default_language);

    std::string result = text ? *text : key;
    int index = 0;
    for (const auto &arg : args)
        boost::replace_all(result, "{" + std::to_string(index++) + "}", arg);
    return result;
}

} // namespace lh
