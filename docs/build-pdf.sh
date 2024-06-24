#!/usr/bin/env bash

# Run this script with `./docs/build-pdf.sh`
# You need to have pandoc and texlive-full installed.

# In later versions of pandoc, --latex-engine is changed to --pdf-engine
pandoc docs/s5varma.log.md --latex-engine=xelatex -o docs/s5varma.log.pdf -V geometry:margin=1in
