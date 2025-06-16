#!/bin/bash

# # Navigate to the directory
# cd /opt/PrincetonInstruments/picam/samples/server-client || {
#   echo "Directory not found!"
#   exit 1
# }

# # Run make
# echo "Running make..."
# make || {
#   echo "Make failed!"
#   exit 1
# }

# Start the camera server
echo "Starting camserver_cit..."
./bin/camserver_cit

