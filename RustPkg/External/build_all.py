#!/usr/bin/env python3

import os

CARGO_MANIFEST_NAME = "Cargo.toml"
CARGO_BUILD_COMMAND = "cargo xbuild --target x86_64-unknown-uefi"

current_script_directory = os.path.dirname(os.path.realpath(__file__))

print("Building all Cargo.toml files in", current_script_directory);

for cargo_file in sorted( [os.path.join(os.getcwd(), item, CARGO_MANIFEST_NAME) for item in next(os.walk(current_script_directory))[1] if os.path.exists(os.path.join(current_script_directory, item, CARGO_MANIFEST_NAME))] ):
  print(" ", cargo_file, ":", sep="")
  os.chdir(os.path.dirname(cargo_file))
  os.system(CARGO_BUILD_COMMAND)
  os.chdir(os.path.dirname(os.getcwd()))
