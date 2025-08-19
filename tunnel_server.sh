#!/bin/sh
stty raw -icanon -echo
./trans -q -m to -p 22
