/*
 *      Configuration.h
 *
 *      Copyright 2023 Asif Amin <asifamin@utexas.edu>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

/* --------------------------------- INCLUDES ------------------------------- */

#include <vector>
#include <string>
#include <memory>

#include <glib.h>

#include "Utils.h"

/* ----------------------------- CLASS DEFINITIONS -------------------------- */

// -----------------------------------------------------------------------------
    struct BracketColorsPluginSetting
/*
    Purpose:    Settings configuration base class
    This is a bit overkill, but might be useful later for more settings
----------------------------------------------------------------------------- */
{
    std::string mGroup, mKey;
    gpointer mValue;

    BracketColorsPluginSetting(
        std::string group,
        std::string key,
        gpointer value
    );

    virtual bool read(GKeyFile *kf)=0;
    virtual bool write(GKeyFile *kf)=0;
};



// -----------------------------------------------------------------------------
    struct BooleanSetting : public BracketColorsPluginSetting
/*

----------------------------------------------------------------------------- */
{
    BooleanSetting(
        std::string group,
        std::string key,
        gpointer value
    );

    bool read(GKeyFile *kf);
    bool write(GKeyFile *kf);
};



// -----------------------------------------------------------------------------
    struct ColorSetting : public BracketColorsPluginSetting
/*

----------------------------------------------------------------------------- */
{
    ColorSetting(
        std::string group,
        std::string key,
        gpointer value
    );

    bool read(GKeyFile *kf);
    bool write(GKeyFile *kf);
};



// -----------------------------------------------------------------------------
    struct BracketColorsPluginConfiguration
/*

----------------------------------------------------------------------------- */
{
    gboolean mUseDefaults;
    BracketColorArray mColors;
    BracketColorArray mCustomColors;

    std::vector<std::shared_ptr<BracketColorsPluginSetting> > mPluginSettings;

    BracketColorsPluginConfiguration(
        gboolean useDefaults,
        BracketColorArray colors
    );

    void LoadConfig(std::string fileName);
    void SaveConfig(std::string fileName);
};



#endif
