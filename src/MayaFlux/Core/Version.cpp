/**
 * The version of the MayaFluxLib binary that is loaded, as the library was built.
 *
 * Not declared in any header on purpose. The only caller is the live interpreter,
 * which looks it up by its unmangled name after it loads the headers and compares
 * it with the MAYAFLUX_VERSION those headers declare, so a header that every
 * translation unit includes does not have to change for it. Keep this in a
 * translation unit that only MayaFluxLib contains: Transitive is also linked
 * into MayaFluxHost, and a second copy there would be found first.
 */
extern "C" MAYAFLUX_API const char* mayaflux_version()
{
    return MAYAFLUX_VERSION;
}
