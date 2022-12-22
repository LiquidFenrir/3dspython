#include "history.h"
#include <algorithm>

void history::get_previous()
{
    if(last != 0)
    {
        last -= 1;
    }
}
void history::get_next()
{
    if(last != current)
    {
        last += 1;
    }
}

// copy last to current
void history::copy_to_current()
{
    if(last != current)
    {
        contents[current] = contents[last];
        last = current;
    }
}

// current string is sent, rotate and have a brand new current
void history::validate()
{
    if(current == MAX_SIZE - 1)
    {
        std::rotate(contents.begin(), contents.begin() + 1, contents.end());
        contents.back().clear();
    }
    else
    {
        ++current;
        ++last;
    }
}

bool history::is_hovering() const
{
    return last != current;
}

// get currently editing string
std::string& history::get_current()
{
    return contents[current];
}
std::string& history::get_hover()
{
    return contents[last];
}
