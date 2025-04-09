#!/usr/bin/env sh
find . -name '*.[hcsS]' | xargs dos2unix
find . -name '*.icf' | xargs dos2unix
find . -name '*.cpp' | xargs dos2unix
find . -name '*.ld' | xargs dos2unix
find . -name '*.md' | xargs dos2unix

