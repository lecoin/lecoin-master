#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/lecoin.ico

convert ../../src/qt/res/icons/lecoin-16.png ../../src/qt/res/icons/lecoin-32.png ../../src/qt/res/icons/lecoin-48.png ${ICON_DST}
