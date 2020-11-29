////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_EMSCRIPTENDISPLAY_H__
#define __APP_EMSCRIPTENDISPLAY_H__


#include <NsCore/Noesis.h>
#include <NsApp/Display.h>
#include <NsCore/ReflectionDeclare.h>


namespace NoesisApp
{

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A display implementation for Emscripten
////////////////////////////////////////////////////////////////////////////////////////////////////
class EmscriptenDisplay final: public Display
{
public:
    EmscriptenDisplay();

    /// From Display
    //@{
    void Show() override;
    void EnterMessageLoop(bool runInBackground) override;
    void* GetNativeHandle() const override;
    uint32_t GetClientWidth() const override;
    uint32_t GetClientHeight() const override;
    //@}

private:
    void FillKeyTable();
    void UpdateSize();

private:
    uint8_t mKeyTable[256];
    uint32_t mWidth;
    uint32_t mHeight;

    NS_DECLARE_REFLECTION(EmscriptenDisplay, Display)
};

}

#endif
