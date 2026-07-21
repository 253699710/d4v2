#!/bin/bash
# Baut interactive_counter separat
# Alle cpp ausser Main.cpp (InteractiveCounter hat eigene main)

set -e
cd "$(dirname "$0")"

BUILD_DIR=build
INCLUDES=-I../../
LIBS="-Wl,-R../../build/ ../../build/libd4.a ../../3rdParty/patoh/libpatoh.a ../../3rdParty/glucose-3.0/core/lib_glucose.a ../../3rdParty/flowCutter/libflowCutter.a ../../3rdParty/bipe/build/libbipe.a ../../3rdParty/bipe/3rdParty/glucose-3.0/core/libglucose_static.a -lboost_program_options -lz -lgmpxx -lgmp"

mkdir -p $BUILD_DIR/src

OBJS=""
for CPP in $(find src -name "*.cpp" | grep -v "Main.cpp"); do
    OBJ="$BUILD_DIR/ic_$(echo $CPP | tr '/' '_').o"
    echo "Compiling: $CPP"
    g++ -O3 -MMD -MP $INCLUDES -c $CPP -o $OBJ
    OBJS="$OBJS $OBJ"
done

echo "Linking: interactive_counter"
g++ $OBJS -o $BUILD_DIR/interactive_counter $INCLUDES $LIBS

echo "Fertig: $BUILD_DIR/interactive_counter"
