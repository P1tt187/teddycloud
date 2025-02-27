#!/bin/bash

set -o errexit
set -o pipefail
set -o nounset
# set -o xtrace

mkdir -p /teddycloud/certs/server /teddycloud/certs/client
cd /teddycloud

if [ -n "${DOCKER_TEST:-}" ]; then
  echo "Running teddycloud --docker-test..."
  LSAN_OPTIONS=detect_leaks=0 teddycloud --docker-test
else
  while true
  do
    if [ -n "${STRACE:-}" ]; then
      echo "Running teddycloud with strace..."
      strace -t -T teddycloud
    else 
      echo "Running teddycloud..."
      teddycloud
    fi
    retVal=$?
    echo "teddycloud exited with code $retVal"
    if [ $retVal -ne -2 ]; then
      exit $retVal
    fi
    echo "Restarting teddycloud..."
  done
fi
