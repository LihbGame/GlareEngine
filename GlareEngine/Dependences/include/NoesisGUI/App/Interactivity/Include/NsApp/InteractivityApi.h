////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __APP_INTERACTIVITYAPI_H__
#define __APP_INTERACTIVITYAPI_H__


#include <NsCore/CompilerSettings.h>


#if defined(NS_APP_INTERACTIVITY_PRIVATE)
    #define NS_APP_INTERACTIVITY_API
#elif defined(NS_APP_INTERACTIVITY_EXPORTS)
    #define NS_APP_INTERACTIVITY_API NS_DLL_EXPORT
#else
    #define NS_APP_INTERACTIVITY_API NS_DLL_IMPORT
#endif


#endif
