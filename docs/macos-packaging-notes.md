# macOS Packaging Notes

The known-good `/Applications/Starship.app` bundle ships Homebrew dylibs as real
files in `Contents/Frameworks` using the compatibility names loaded by the main
executable, for example:

- `libSDL2-2.0.0.dylib`
- `libGLEW.2.3.dylib`
- `libfmt.12.dylib`
- `libogg.0.dylib`
- `libspdlog.1.17.dylib`
- `libtinyxml2.11.dylib`
- `libzip.5.dylib`
- `libzstd.1.dylib`

The CPack bundle generated before July 8, 2026 included several Homebrew
versioned dylib files plus symlinks, such as `libGLEW.2.3.dylib ->
libGLEW.2.3.1.dylib`. That layout passed `otool` inspection in the staging
directory, but it did not match the installed working app and produced a user
report that the packaged app failed while loading SDL.

The later plain `.app` bundle exposed two more concrete differences from the
working `/Applications/Starship.app`:

- `Contents/Resources/sf64.o2r` was missing, which accounted for most of the
  size difference from the working app.
- Homebrew's `libSDL2-2.0.0.dylib` is `sdl2-compat`. It does not link SDL3
  through a normal Mach-O load command; it loads `libSDL3.dylib` dynamically at
  runtime. The working app survived this because its executable still carried
  an `/opt/homebrew/lib` rpath, but a relocated app with no bundled SDL3 failed
  with `Fatal error! Cannot continue! Failed loading SDL3 library.`

The macOS install step now runs `cmake/MacOSCanonicalizeBundleDylibs.cmake`
after `fixup_bundle`. The script flattens dylib symlinks into real files,
rewrites install IDs and internal dependency references to the compatibility
filenames, copies `libSDL3.dylib` when the bundled SDL2 is `sdl2-compat`, and
removes the extra versioned files. The desired invariant is that the packaged
app can load every non-system dependency from
`@executable_path/../Frameworks/<compatibility-name>.dylib` or SDL2's
`@loader_path/libSDL3.dylib` lookup without relying on Homebrew paths, rpaths,
or symlink preservation.
