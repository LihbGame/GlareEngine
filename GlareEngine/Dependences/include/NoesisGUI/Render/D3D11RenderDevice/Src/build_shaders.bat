@echo off

del Shaders.h

:: Vertex Shaders

CALL :fxc_vs Pos_VS ShaderVS.hlsl /DHAS_POSITION
CALL :fxc_vs PosColor_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_COLOR
CALL :fxc_vs PosTex0_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_UV0
CALL :fxc_vs PosColorCoverage_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_COLOR /DHAS_COVERAGE
CALL :fxc_vs PosTex0Coverage_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_UV0 /DHAS_COVERAGE
CALL :fxc_vs PosColorTex1_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_COLOR /DHAS_UV1
CALL :fxc_vs PosTex0Tex1_VS ShaderVS.hlsl /DHAS_POSITION /DHAS_UV0 /DHAS_UV1

:: Pixel Shaders

CALL :fxc_ps RGBA_FS ShaderPS.hlsl /DEFFECT_RGBA
CALL :fxc_ps Mask_FS ShaderPS.hlsl /DEFFECT_MASK
CALL :fxc_ps PathSolid_FS ShaderPS.hlsl /DEFFECT_PATH_SOLID
CALL :fxc_ps PathLinear_FS ShaderPS.hlsl /DEFFECT_PATH_LINEAR
CALL :fxc_ps PathRadial_FS ShaderPS.hlsl /DEFFECT_PATH_RADIAL
CALL :fxc_ps PathPattern_FS ShaderPS.hlsl /DEFFECT_PATH_PATTERN
CALL :fxc_ps PathAASolid_FS ShaderPS.hlsl /DEFFECT_PATH_AA_SOLID
CALL :fxc_ps PathAALinear_FS ShaderPS.hlsl /DEFFECT_PATH_AA_LINEAR
CALL :fxc_ps PathAARadial_FS ShaderPS.hlsl /DEFFECT_PATH_AA_RADIAL
CALL :fxc_ps PathAAPattern_FS ShaderPS.hlsl /DEFFECT_PATH_AA_PATTERN
CALL :fxc_ps ImagePaintOpacitySolid_FS ShaderPS.hlsl /DEFFECT_IMAGE_PAINT_OPACITY_SOLID
CALL :fxc_ps ImagePaintOpacityLinear_FS ShaderPS.hlsl /DEFFECT_IMAGE_PAINT_OPACITY_LINEAR
CALL :fxc_ps ImagePaintOpacityRadial_FS ShaderPS.hlsl /DEFFECT_IMAGE_PAINT_OPACITY_RADIAL
CALL :fxc_ps ImagePaintOpacityPattern_FS ShaderPS.hlsl /DEFFECT_IMAGE_PAINT_OPACITY_PATTERN
CALL :fxc_ps TextSolid_FS ShaderPS.hlsl /DEFFECT_TEXT_SOLID
CALL :fxc_ps TextLinear_FS ShaderPS.hlsl /DEFFECT_TEXT_LINEAR
CALL :fxc_ps TextRadial_FS ShaderPS.hlsl /DEFFECT_TEXT_RADIAL
CALL :fxc_ps TextPattern_FS ShaderPS.hlsl /DEFFECT_TEXT_PATTERN

:: Clear and Resolve 

CALL :fxc_vs Quad_VS QuadVS.hlsl
CALL :fxc_ps Clear_PS ClearPS.hlsl
CALL :fxc_ps Resolve2_PS ResolvePS.hlsl "/DNUM_SAMPLES=2"
CALL :fxc_ps Resolve4_PS ResolvePS.hlsl "/DNUM_SAMPLES=4"
CALL :fxc_ps Resolve8_PS ResolvePS.hlsl "/DNUM_SAMPLES=8"
CALL :fxc_ps Resolve16_PS ResolvePS.hlsl "/DNUM_SAMPLES=16"

EXIT /B 0

:: -------------------------------------------------------------------------------------------------------
:fxc_vs
    fxc /T vs_4_0 /nologo /O3 /Qstrip_reflect /Fo %1.o %2 %3 %4 %5 > NUL
    bin2h.py %1.o %1 >> Shaders.h
    del %1.o
    EXIT /B 0

:fxc_ps
    fxc /T ps_4_0 /nologo /O3 /Qstrip_reflect /Fo %1.o %2 %3 %4 %5 > NUL
    bin2h.py %1.o %1 >> Shaders.h
    del %1.o
    EXIT /B 0