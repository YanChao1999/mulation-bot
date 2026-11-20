#ifndef MULATION_MUTE_LINES_HPP
#define MULATION_MUTE_LINES_HPP

#include <cstddef>
#include <string_view>
#include <vector>

// Build a 1-based line mute mask from source text.
// Directives (anywhere on the line, typically in // or /* */ comments):
//   mulation:off  — mute this line and following lines until mulation:on
//   mulation:on   — resume mutating (region ends)
//   mulation:skip — mute only this line
inline std::vector<bool> mulation_mute_mask(std::string_view text) {
    std::vector<bool> mute;
    mute.push_back(false); // unused index 0
    bool region_off = false;
    std::size_t i = 0;
    while (i <= text.size()) {
        std::size_t end = i;
        while (end < text.size() && text[end] != '\n') {
            ++end;
        }
        std::string_view line = text.substr(i, end - i);
        const bool has_off = line.find("mulation:off") != std::string_view::npos;
        const bool has_on = line.find("mulation:on") != std::string_view::npos;
        const bool has_skip = line.find("mulation:skip") != std::string_view::npos;

        const bool line_mute = region_off || has_skip || has_off;
        mute.push_back(line_mute);

        if (has_off) {
            region_off = true;
        }
        if (has_on) {
            region_off = false;
        }

        if (end >= text.size()) {
            break;
        }
        i = end + 1;
    }
    return mute;
}

inline bool mulation_line_muted(const std::vector<bool> &mute, unsigned line) {
    return line > 0 && static_cast<std::size_t>(line) < mute.size() && mute[line];
}

#endif
