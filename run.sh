#!/bin/bash
export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH
echo "hi!"
./out/p2p_server tmp.ini -d
