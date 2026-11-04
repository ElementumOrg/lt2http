#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace lh {

// A file of web/, see CMakeLists.txt
struct WebFile {
    std::string_view path;
    std::string_view content;
};
extern const std::vector<WebFile> web_files;

// nullptr if there is no such file
const WebFile *find_web_file(std::string_view path);

constexpr const char *default_language = "en_gb";

struct Language {
    std::string code;
    std::string name;
};
// The translations in web/i18n/<code>.json, sorted by code
const std::vector<Language> &languages();

// "de", "de-DE" or "de_de" give "de_de", the fallback when there is no such translation
std::string resolve_language(const std::string &requested, const std::string &fallback = default_language);

// The text of a key, in English when the language does not have it, or the key itself. {0}, {1}... are replaced by args.
std::string translate(const std::string &language, const std::string &key, std::initializer_list<std::string> args = {});

} // namespace lh
