#!/bin/bash

# ------------------------------------------------------
#  build_web.sh
#  Converts index.html, main.css, and app.js into
#  C header files containing valid string literals.
# ------------------------------------------------------

set -e

SRC_HTML="index.html"
SRC_CSS="main.css"
SRC_JS="app.js"

OUT_HTML="index_html.h"
OUT_CSS="main_css.h"
OUT_JS="app_js.h"
OUT_ALL="web_assets.h"

convert_file() {
    infile="$1"
    outfile="$2"
    varname="$3"

    echo "Generating $outfile ..."

    {
        echo "// AUTO-GENERATED — DO NOT EDIT"
        echo "#ifndef ${varname}_H"
        echo "#define ${varname}_H"
        echo ""
        echo "static const char ${varname}[] ="
        sed 's/\\/\\\\/g; s/"/\\"/g; s/^/"/; s/$/\\n"/' "$infile"
        echo ";"
        echo ""
        echo "#endif // ${varname}_H"
    } > "$outfile"
}

convert_file "$SRC_HTML" "$OUT_HTML" "index_html"
convert_file "$SRC_CSS"  "$OUT_CSS"  "main_css"
convert_file "$SRC_JS"   "$OUT_JS"   "app_js"

echo "Generating $OUT_ALL ..."
{
    echo "// AUTO-GENERATED — DO NOT EDIT"
    echo "#ifndef WEB_ASSETS_H"
    echo "#define WEB_ASSETS_H"
    echo ""
    echo '#include "index_html.h"'
    echo '#include "main_css.h"'
    echo '#include "app_js.h"'
    echo ""
    echo "#endif // WEB_ASSETS_H"
} > "$OUT_ALL"

echo "Web asset headers generated successfully!"
