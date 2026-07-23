/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef MANGOS_LOCALES_H
#define MANGOS_LOCALES_H

#include <string>
#include <vector>

/** Client locales, in the order the client itself numbers them. */
enum LocaleConstant
{
    LOCALE_enUS = 0,                                        // also enGB
    LOCALE_koKR = 1,
    LOCALE_frFR = 2,
    LOCALE_deDE = 3,
    LOCALE_zhCN = 4,
    LOCALE_zhTW = 5,
    LOCALE_esES = 6,
    LOCALE_esMX = 7,
#if !defined(CLASSIC)
    LOCALE_ruRU = 8,
#endif
};

#if defined(CLASSIC)
#define MAX_LOCALE 8
#else
#define MAX_LOCALE 9
#endif

#define DEFAULT_LOCALE LOCALE_enUS

extern char const* localeNames[MAX_LOCALE];

struct LocaleNameStr
{
    char const* name;
    LocaleConstant locale;
};

extern LocaleNameStr const fullLocaleNameList[];

LocaleConstant GetLocaleByName(const std::string& name);

typedef std::vector<std::string> StringVector;

#endif
