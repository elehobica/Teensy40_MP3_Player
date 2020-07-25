#include "utf_conv.h"
#include <codecvt>
#include <cassert>
#include <locale>

extern "C"{
    int __exidx_start(){ return -1;}
    int __exidx_end(){ return -1; }
}

std::string utf16_to_utf8(std::u16string const& src)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.to_bytes(src);
}
