#!/usr/bin/env bash
cd "$(dirname "$0")"
pandoc w25-report/report.md \
    # --pdf-engine=xelatex \
    -V geometry:margin=1in \
    -o w25-report.pdf