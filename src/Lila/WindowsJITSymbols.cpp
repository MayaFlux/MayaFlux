#ifdef MAYAFLUX_PLATFORM_WINDOWS

// Force export of MSVC runtime symbols needed by JIT
#pragma comment(linker, "/export:??_7type_info@@6B@")
#pragma comment(linker, "/export:??3@YAXPEAX_K@Z")
#pragma comment(linker, "/export:??3@YAXPEAX@Z")
#pragma comment(linker, "/export:??2@YAPEAX_K@Z")
#pragma comment(linker, "/export:?_Facet_Register@std@@YAXPEAV_Facet_base@1@@Z")

#endif
