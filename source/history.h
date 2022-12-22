#pragma once

#include <string>
#include <array>

struct history {
    // move up in the history
    void get_previous();
    // move down in the history
    void get_next();

    // copy last to current
    void copy_to_current();

    // current string is sent, rotate and have a brand new current
    void validate();

    bool is_hovering() const;

    // get currently editing string
    std::string& get_current();
    std::string& get_hover();

private:
    static constexpr inline std::size_t MAX_SIZE = 20;
    std::array<std::string, MAX_SIZE> contents;
    std::size_t current{0};
    std::size_t last{0};
};
