////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#include <NsApp/ThemeProviders.h>
#include <NsApp/EmbeddedXamlProvider.h>
#include <NsApp/EmbeddedFontProvider.h>
#include <NsGui/IntegrationAPI.h>


using namespace Noesis;
using namespace NoesisApp;


////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{
#include "NoesisTheme.Fonts.xaml.bin.h"
#include "NoesisTheme.Styles.xaml.bin.h"

#include "NoesisTheme.Colors.Dark.xaml.bin.h"
#include "NoesisTheme.Colors.Light.xaml.bin.h"

#include "NoesisTheme.Brushes.DarkRed.xaml.bin.h"
#include "NoesisTheme.Brushes.LightRed.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkGreen.xaml.bin.h"
#include "NoesisTheme.Brushes.LightGreen.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkBlue.xaml.bin.h"
#include "NoesisTheme.Brushes.LightBlue.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkOrange.xaml.bin.h"
#include "NoesisTheme.Brushes.LightOrange.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkEmerald.xaml.bin.h"
#include "NoesisTheme.Brushes.LightEmerald.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkPurple.xaml.bin.h"
#include "NoesisTheme.Brushes.LightPurple.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkCrimson.xaml.bin.h"
#include "NoesisTheme.Brushes.LightCrimson.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkLime.xaml.bin.h"
#include "NoesisTheme.Brushes.LightLime.xaml.bin.h"
#include "NoesisTheme.Brushes.DarkAqua.xaml.bin.h"
#include "NoesisTheme.Brushes.LightAqua.xaml.bin.h"

#include "NoesisTheme.DarkRed.xaml.bin.h"
#include "NoesisTheme.LightRed.xaml.bin.h"
#include "NoesisTheme.DarkGreen.xaml.bin.h"
#include "NoesisTheme.LightGreen.xaml.bin.h"
#include "NoesisTheme.DarkBlue.xaml.bin.h"
#include "NoesisTheme.LightBlue.xaml.bin.h"
#include "NoesisTheme.DarkOrange.xaml.bin.h"
#include "NoesisTheme.LightOrange.xaml.bin.h"
#include "NoesisTheme.DarkEmerald.xaml.bin.h"
#include "NoesisTheme.LightEmerald.xaml.bin.h"
#include "NoesisTheme.DarkPurple.xaml.bin.h"
#include "NoesisTheme.LightPurple.xaml.bin.h"
#include "NoesisTheme.DarkCrimson.xaml.bin.h"
#include "NoesisTheme.LightCrimson.xaml.bin.h"
#include "NoesisTheme.DarkLime.xaml.bin.h"
#include "NoesisTheme.LightLime.xaml.bin.h"
#include "NoesisTheme.DarkAqua.xaml.bin.h"
#include "NoesisTheme.LightAqua.xaml.bin.h"

EmbeddedXaml gXamls[] =
{
    { "Theme/NoesisTheme.Fonts.xaml", NoesisTheme_Fonts_xaml },
    { "Theme/NoesisTheme.Styles.xaml", NoesisTheme_Styles_xaml },

    { "Theme/NoesisTheme.Colors.Dark.xaml", NoesisTheme_Colors_Dark_xaml },
    { "Theme/NoesisTheme.Colors.Light.xaml", NoesisTheme_Colors_Light_xaml },

    { "Theme/NoesisTheme.Brushes.DarkRed.xaml", NoesisTheme_Brushes_DarkRed_xaml },
    { "Theme/NoesisTheme.Brushes.LightRed.xaml", NoesisTheme_Brushes_LightRed_xaml },
    { "Theme/NoesisTheme.Brushes.DarkGreen.xaml", NoesisTheme_Brushes_DarkGreen_xaml },
    { "Theme/NoesisTheme.Brushes.LightGreen.xaml", NoesisTheme_Brushes_LightGreen_xaml },
    { "Theme/NoesisTheme.Brushes.DarkBlue.xaml", NoesisTheme_Brushes_DarkBlue_xaml },
    { "Theme/NoesisTheme.Brushes.LightBlue.xaml", NoesisTheme_Brushes_LightBlue_xaml },
    { "Theme/NoesisTheme.Brushes.DarkOrange.xaml", NoesisTheme_Brushes_DarkOrange_xaml },
    { "Theme/NoesisTheme.Brushes.LightOrange.xaml", NoesisTheme_Brushes_LightOrange_xaml },
    { "Theme/NoesisTheme.Brushes.DarkEmerald.xaml", NoesisTheme_Brushes_DarkEmerald_xaml },
    { "Theme/NoesisTheme.Brushes.LightEmerald.xaml", NoesisTheme_Brushes_LightEmerald_xaml },
    { "Theme/NoesisTheme.Brushes.DarkPurple.xaml", NoesisTheme_Brushes_DarkPurple_xaml },
    { "Theme/NoesisTheme.Brushes.LightPurple.xaml", NoesisTheme_Brushes_LightPurple_xaml },
    { "Theme/NoesisTheme.Brushes.DarkCrimson.xaml", NoesisTheme_Brushes_DarkCrimson_xaml },
    { "Theme/NoesisTheme.Brushes.LightCrimson.xaml", NoesisTheme_Brushes_LightCrimson_xaml },
    { "Theme/NoesisTheme.Brushes.DarkLime.xaml", NoesisTheme_Brushes_DarkLime_xaml },
    { "Theme/NoesisTheme.Brushes.LightLime.xaml", NoesisTheme_Brushes_LightLime_xaml },
    { "Theme/NoesisTheme.Brushes.DarkAqua.xaml", NoesisTheme_Brushes_DarkAqua_xaml },
    { "Theme/NoesisTheme.Brushes.LightAqua.xaml", NoesisTheme_Brushes_LightAqua_xaml },

    { "Theme/NoesisTheme.DarkRed.xaml", NoesisTheme_DarkRed_xaml },
    { "Theme/NoesisTheme.LightRed.xaml", NoesisTheme_LightRed_xaml },
    { "Theme/NoesisTheme.DarkGreen.xaml", NoesisTheme_DarkGreen_xaml },
    { "Theme/NoesisTheme.LightGreen.xaml", NoesisTheme_LightGreen_xaml },
    { "Theme/NoesisTheme.DarkBlue.xaml", NoesisTheme_DarkBlue_xaml },
    { "Theme/NoesisTheme.LightBlue.xaml", NoesisTheme_LightBlue_xaml },
    { "Theme/NoesisTheme.DarkOrange.xaml", NoesisTheme_DarkOrange_xaml },
    { "Theme/NoesisTheme.LightOrange.xaml", NoesisTheme_LightOrange_xaml },
    { "Theme/NoesisTheme.DarkEmerald.xaml", NoesisTheme_DarkEmerald_xaml },
    { "Theme/NoesisTheme.LightEmerald.xaml", NoesisTheme_LightEmerald_xaml },
    { "Theme/NoesisTheme.DarkPurple.xaml", NoesisTheme_DarkPurple_xaml },
    { "Theme/NoesisTheme.LightPurple.xaml", NoesisTheme_LightPurple_xaml },
    { "Theme/NoesisTheme.DarkCrimson.xaml", NoesisTheme_DarkCrimson_xaml },
    { "Theme/NoesisTheme.LightCrimson.xaml", NoesisTheme_LightCrimson_xaml },
    { "Theme/NoesisTheme.DarkLime.xaml", NoesisTheme_DarkLime_xaml },
    { "Theme/NoesisTheme.LightLime.xaml", NoesisTheme_LightLime_xaml },
    { "Theme/NoesisTheme.DarkAqua.xaml", NoesisTheme_DarkAqua_xaml },
    { "Theme/NoesisTheme.LightAqua.xaml", NoesisTheme_LightAqua_xaml },
};

#include "PT Root UI_Regular.otf.bin.h"
#include "PT Root UI_Bold.otf.bin.h"

EmbeddedFont gFonts[] =
{
    { "Theme/Fonts", PT_Root_UI_Regular_otf },
    { "Theme/Fonts", PT_Root_UI_Bold_otf }
};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void NoesisApp::SetThemeProviders(XamlProvider* xamlFallback, FontProvider* fontFallback,
    TextureProvider* textureFallback)
{
    GUI::SetTextureProvider(textureFallback);
    GUI::SetXamlProvider(MakePtr<EmbeddedXamlProvider>(gXamls, xamlFallback));
    GUI::SetFontProvider(MakePtr<EmbeddedFontProvider>(gFonts, fontFallback));

    const char* fonts[] =
    {
        "Theme/Fonts/#PT Root UI",

#if defined(NS_PLATFORM_WINDOWS_DESKTOP) || defined(NS_PLATFORM_WINRT)
        "Arial",
        "Segoe UI Emoji",           // Windows 10 Emojis
        "Arial Unicode MS",         // Almost everything (but part of MS Office, not Windows)
        "Microsoft Sans Serif",     // Unicode scripts excluding Asian scripts
        "Microsoft YaHei",          // Chinese
        "Gulim",                    // Korean
        "MS Gothic",                // Japanese
#endif

#ifdef NS_PLATFORM_OSX
        "Arial",
        "Arial Unicode MS",         // MacOS 10.5+
#endif

#ifdef NS_PLATFORM_IPHONE
        "PingFang SC",              // Simplified Chinese (iOS 9+)
        "Apple SD Gothic Neo",      // Korean (iOS 7+)
        "Hiragino Sans",            // Japanese (iOS 9+)
#endif

#ifdef NS_PLATFORM_ANDROID
        "Noto Sans CJK SC",         // Simplified Chinese
        "Noto Sans CJK KR",         // Korean
        "Noto Sans CJK JP",         // Japanese
#endif

#ifdef NS_PLATFORM_LINUX
        "Noto Sans CJK SC",         // Simplified Chinese
        "Noto Sans CJK KR",         // Korean
        "Noto Sans CJK JP",         // Japanese
#endif
    };

    GUI::SetFontFallbacks(fonts, NS_COUNTOF(fonts));
    GUI::SetFontDefaultProperties(15.0f, FontWeight_Normal, FontStretch_Normal, FontStyle_Normal);
}
