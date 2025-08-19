#!/bin/sh
DIR=$(dirname "$0")
stty raw -icanon -echo
"$DIR/trans" -q -m to -p 22
