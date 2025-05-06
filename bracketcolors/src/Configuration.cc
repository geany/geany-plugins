/*
 *      Configuration.cc
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


/* --------------------------------- INCLUDES ------------------------------- */

#include <geanyplugin.h>
#include "Configuration.h"


/* ------------------------------ IMPLEMENTATION ---------------------------- */


// -----------------------------------------------------------------------------
    BracketColorsPluginSetting::BracketColorsPluginSetting(
        std::string group,
        std::string key,
        gpointer value
    )
/*
    Constructor
----------------------------------------------------------------------------- */
:   mGroup(group),
    mKey(key),
    mValue(value)
{
    // nothing to do
}



// -----------------------------------------------------------------------------
    BooleanSetting::BooleanSetting(
        std::string group,
        std::string key,
        gpointer value
    )
/*
    Constructor
----------------------------------------------------------------------------- */
:   BracketColorsPluginSetting(group, key, value)
{
    // nothing to do
}



// -----------------------------------------------------------------------------
    ColorSetting::ColorSetting(
        std::string group,
        std::string key,
        gpointer value
    )
/*
    Constructor
----------------------------------------------------------------------------- */
:   BracketColorsPluginSetting(group, key, value)
{
    // nothing to do
}



// -----------------------------------------------------------------------------
    bool BooleanSetting::read(GKeyFile *kf)
/*

----------------------------------------------------------------------------- */
{
    gboolean *aBool = static_cast<gboolean *>(mValue);
    *aBool = utils_get_setting_boolean(
        kf, mGroup.c_str(), mKey.c_str(), *aBool
    );
    return true;
}



// -----------------------------------------------------------------------------
    bool BooleanSetting::write(GKeyFile *kf)
/*

----------------------------------------------------------------------------- */
{
    const gboolean *aBool = static_cast<gboolean *>(mValue);
    g_key_file_set_boolean(
        kf, mGroup.c_str(), mKey.c_str(), *aBool
    );
    return true;
}



// -----------------------------------------------------------------------------
    bool ColorSetting::read(GKeyFile *kf)
/*

----------------------------------------------------------------------------- */
{
    std::string *strPtr = reinterpret_cast<std::string *>(mValue);

    gchar *str = utils_get_setting_string(
        kf, mGroup.c_str(), mKey.c_str(), strPtr->c_str()
    );

    /*
     * Make sure the color is valid
     */
    bool ret = false;

    GdkColor color;
    if (utils_parse_color(str, &color)) {
        *strPtr = std::string(str);
        ret = true;
    }
    else {
        g_debug("%s: Failed to parse color '%s'", __FUNCTION__, str);
    }

    g_free(str);
    return ret;
}



// -----------------------------------------------------------------------------
    bool ColorSetting::write(GKeyFile *kf)
/*

----------------------------------------------------------------------------- */
{
    std::string *strPtr = reinterpret_cast<std::string *>(mValue);
    g_key_file_set_string(
        kf, mGroup.c_str(), mKey.c_str(), strPtr->c_str()
    );
    return true;
}



// -----------------------------------------------------------------------------
    static gboolean read_keyfile(
        GKeyFile *kf,
        std::string filename,
        GKeyFileFlags flags
    )
/*

----------------------------------------------------------------------------- */
{
    GError *error = NULL;
    if (!g_key_file_load_from_file(kf, filename.c_str(), flags, &error)) {
        if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOENT) {
            g_debug("%s: Failed to load configuration file: %s", __FUNCTION__, error->message);
        }
        g_error_free(error);
        return FALSE;
    }

    return TRUE;
}



// -----------------------------------------------------------------------------
    static gboolean write_keyfile(
        GKeyFile *kf,
        std::string filename
    )
/*

----------------------------------------------------------------------------- */
{
    gchar *dirname = g_path_get_dirname(filename.c_str());

    gsize length;
    gchar *data = g_key_file_to_data(kf, &length, NULL);

    GError *error = NULL;
    gint err;
    gboolean success = FALSE;

    if ((err = utils_mkdir(dirname, TRUE)) != 0) {
        g_warning(
            "Failed to create configuration directory \"%s\": %s",
            dirname, g_strerror(err)
        );
    }
    else if (!g_file_set_contents(filename.c_str(), data, (gssize)length, &error)) {
        g_warning("Failed to save configuration file: %s", error->message);
        g_error_free(error);
    } else {
        success = TRUE;
    }

    g_free(data);
    g_free(dirname);

    return success;
}



// -----------------------------------------------------------------------------
    BracketColorsPluginConfiguration::BracketColorsPluginConfiguration(
        gboolean useDefaults,
        BracketColorArray colors
    )
/*

----------------------------------------------------------------------------- */
:   mUseDefaults(useDefaults),
    mColors(colors),
    mCustomColors(mColors)
{
    mPluginSettings.push_back(
        std::make_shared<BooleanSetting>("general", "defaults", &mUseDefaults)
    );

    for (guint i = 0; i < mCustomColors.size(); i++) {
        std::string key = "order_" + std::to_string(i);
        mPluginSettings.push_back(
            std::make_shared<ColorSetting>("colors", key, &mCustomColors[i])
        );
    }
}



// -----------------------------------------------------------------------------
    void BracketColorsPluginConfiguration::LoadConfig(std::string fileName)
/*

----------------------------------------------------------------------------- */
{
    GKeyFile *kf = g_key_file_new();
    bool success = true;

    if (read_keyfile(kf, fileName, G_KEY_FILE_NONE)) {
        for (auto &it : mPluginSettings) {
            if (not it->read(kf)) {
                success = false;
            }
        }
    }
    else {
        g_debug("%s: Unable to load '%s''", __FUNCTION__, fileName.c_str());
        success = false;
    }

    if (not success) {
        mUseDefaults = true;
    }

    g_key_file_free(kf);
}



// -----------------------------------------------------------------------------
    void BracketColorsPluginConfiguration::SaveConfig(std::string fileName)
/*

----------------------------------------------------------------------------- */
{
    GKeyFile *kf = g_key_file_new();

    read_keyfile(kf, fileName, G_KEY_FILE_KEEP_COMMENTS);

    for (auto &it : mPluginSettings) {
        it->write(kf);
    }
    write_keyfile(kf, fileName);

    g_key_file_free(kf);
}
