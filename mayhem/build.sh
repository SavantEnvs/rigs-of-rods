#!/usr/bin/env bash
# mayhem/build.sh — Rigs of Rods file-format parser harnesses.
#
# The game is one executable (source/main/CMakeLists.txt add_executable(RoR)) that needs a render
# system, GUI and sound. The harnesses instead compile the file-format parsers from source/main
# (resources/*_fileformat, utils/GenericFileFormat, utils/ConfigFile, system/Console,
# utils/bbcode) with the sanitizer flags, plus mayhem/harnesses/ror_app_shim.cpp standing in for
# Application.cpp's globals, and link them against Debian's OgreMain and fmt and AngelScript from
# /opt/toolchains/deps. Objects go to mayhem/build/ (gitignored by upstream's `build/`).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
M="$SRC/source/main"
AS=/opt/toolchains/deps/angelscript-2.38.0
OUT="$SRC/mayhem/build"
mkdir -p "$OUT/fuzz" "$OUT/test"

INC=(-I"$SRC/mayhem/harnesses")
while IFS= read -r d; do INC+=(-I"$d"); done < <(find "$M" -type d | LC_ALL=C sort)
INC+=(-I"$SRC/external/header_only" -I"$SRC/external/angelscript_addons"
      -I/usr/include/OGRE -I/usr/include/OGRE/Overlay -I/usr/include/OGRE/Paging
      -I/usr/include/OGRE/Terrain -I/usr/include/OGRE/RTShaderSystem -I/usr/include/OGRE/Bites
      -I/usr/include/MYGUI -I/usr/include/ois -I"$AS/include")
# Same defines as the game's CMake target; ForwardDeclarations.h is force-included because the
# game builds with a precompiled header that provides it to every TU (OTCFileFormat.h relies on it).
# RefCountingObject.h asserts every AddRef/Release runs on the game's main thread, read from
# App::GetAppContext() (the window/input/render context, which does not exist here). The
# harnesses are single-threaded, so that one thread-affinity check is defined away through the
# hook the header provides (`#if !defined(RefCountingObject_ASSERT)`); every other assert stays.
DEFS=(-std=c++17 -DUSE_ANGELSCRIPT -DAS_USE_NAMESPACE -include ForwardDeclarations.h -w
      "-DRefCountingObject_ASSERT(_Expr_)=((void)0)")
LIBS=(-lOgreMain -lfmt "$AS/lib/libangelscript.a" -lpthread -lm)

PARSER_SRCS=(
  resources/rig_def_fileformat/RigDef_File.cpp
  resources/rig_def_fileformat/RigDef_Node.cpp
  resources/rig_def_fileformat/RigDef_Parser.cpp
  resources/rig_def_fileformat/RigDef_SequentialImporter.cpp
  resources/rig_def_fileformat/RigDef_Serializer.cpp
  resources/rig_def_fileformat/RigDef_Validator.cpp
  resources/odef_fileformat/ODefFileFormat.cpp
  resources/tobj_fileformat/TObjFileFormat.cpp
  resources/terrn2_fileformat/Terrn2FileFormat.cpp
  resources/otc_fileformat/OTCFileFormat.cpp
  utils/GenericFileFormat.cpp
  utils/ConfigFile.cpp
  system/Console.cpp
  system/CVar.cpp
  utils/bbcode/BBDocument.cpp
  resources/skin_fileformat/SkinFileFormat.cpp
  resources/tuneup_fileformat/TuneupFileFormat.cpp
)

# Storage for the App:: CVar pointers Application.cpp declares (CVar.cpp's console-variable
# registry refers to all of them). Regenerated from Application.cpp on every build; the shim
# creates the few the parsers read in ror_fuzz::Init().
{
  echo '#include "CVar.h"'
  echo 'namespace RoR { namespace App {'
  tr -d '\r' < "$M/Application.cpp" | grep -E '^CVar\* [A-Za-z0-9_]+;$'
  echo '} }'
} > "$OUT/ror_cvars.cpp"

# compile <outdir> <flags...> -- <sources...>   (parallel, one object per source)
compile() {
  local outdir="$1"; shift
  local flags=(); while [ "$1" != "--" ]; do flags+=("$1"); shift; done; shift
  printf '%s\n' "$@" | xargs -P "$MAYHEM_JOBS" -I{} sh -c '
    src="$1"; shift; outdir="$1"; shift
    obj="$outdir/$(echo "$src" | tr / _).o"
    exec '"$CXX"' "$@" -c "$src" -o "$obj"' _ {} "$outdir" "${flags[@]}"
}

# 1) Sanitized + instrumented parsers and shim (the fuzzed code).
# UBSan's `vptr` check needs the typeinfo of every polymorphic game class a parser header
# names (CacheEntry, ProceduralRoad, ...), whose key functions live in game sources not linked
# here, so only that check is relaxed; ASan and the rest of UBSan stay on and halting.
case " $SANITIZER_FLAGS " in *undefined*) SANITIZER_FLAGS="$SANITIZER_FLAGS -fno-sanitize=vptr";; esac
FUZZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link "${DEFS[@]}" "${INC[@]}")
compile "$OUT/fuzz" "${FUZZ_FLAGS[@]}" -- $(for s in "${PARSER_SRCS[@]}"; do echo "$M/$s"; done) "$SRC/mayhem/harnesses/ror_app_shim.cpp" "$OUT/ror_cvars.cpp"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/fuzz/standalone_main.o"
FUZZ_OBJS=()
for s in "${PARSER_SRCS[@]}"; do FUZZ_OBJS+=("$OUT/fuzz/$(echo "$M/$s" | tr / _).o"); done
FUZZ_OBJS+=("$OUT/fuzz/$(echo "$SRC/mayhem/harnesses/ror_app_shim.cpp" | tr / _).o" "$OUT/fuzz/$(echo "$OUT/ror_cvars.cpp" | tr / _).o" "$OUT/fuzz/lsan_off.o")

for h in rigdef tobj odef terrn2 otc genericdoc bbcode skin tuneup; do
  src="$SRC/mayhem/harnesses/fuzz_$h.cpp"
  $CXX "${FUZZ_FLAGS[@]}" -c "$src" -o "$OUT/fuzz/fuzz_$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/fuzz/fuzz_$h.o" "${FUZZ_OBJS[@]}" "${LIBS[@]}" -o "$SRC/fuzz_$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/fuzz/standalone_main.o" "$OUT/fuzz/fuzz_$h.o" "${FUZZ_OBJS[@]}" "${LIBS[@]}" -o "$SRC/fuzz_$h-standalone"
done

# 2) Known-answer self-test over the same parsers, built with normal flags (the test.sh oracle).
TEST_FLAGS=(-O1 $COVERAGE_FLAGS "${DEFS[@]}" "${INC[@]}")
compile "$OUT/test" "${TEST_FLAGS[@]}" -- $(for s in "${PARSER_SRCS[@]}"; do echo "$M/$s"; done) "$SRC/mayhem/harnesses/ror_app_shim.cpp" "$SRC/mayhem/harnesses/ror_selftest.cpp" "$OUT/ror_cvars.cpp"
$CXX $COVERAGE_FLAGS "$OUT"/test/*.o "${LIBS[@]}" -o "$SRC/mayhem/build/ror_selftest"
echo "build.sh: done"
