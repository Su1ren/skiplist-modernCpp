#########################################################################
# File Name:    stress_test_start.sh
# Author:        程序员Carl
# 微信公众号:    代码随想录
# Created Time: Wed Jan 30 20:05:15 2019
#########################################################################
#!/bin/bash
set -e
g++ stress-test/stress_test.cpp -o ./bin/stress --std=c++17 -I. -pthread
./bin/stress "$@"
