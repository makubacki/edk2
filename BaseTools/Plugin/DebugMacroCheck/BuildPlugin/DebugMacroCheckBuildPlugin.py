# @file DebugMacroCheckBuildPlugin.py
#
# A build plugin that checks if DEBUG macros are formatted properly.
#
# In particular, that print format specifiers are defined
# with the expected number of arguments in the variable
# argument list.
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##

import logging
import os
import pathlib
import sys
import yaml

# Import the build plugin
plugin_file = pathlib.Path(__file__)
sys.path.append(str(plugin_file.parent.parent))

# flake8 (E402): Ignore flake8 module level import not at top of file
import DebugMacroCheck                          # noqa: E402

from edk2toolext import edk2_logging                               # noqa: E402
from edk2toolext.environment.plugintypes.uefi_build_plugin import \
    IUefiBuildPlugin                                               # noqa: E402
from edk2toolext.environment.uefi_build import UefiBuilder         # noqa: E402
from pathlib import Path                                           # noqa: E402
from typing import Any, Dict                                       # noqa: E402


class DebugMacroCheckBuildPlugin(IUefiBuildPlugin):

    def _get_substitutions(self, plugin_cfg: Any, file_name: str = None) -> Dict[str, Dict[str, str]]:
        """Extracts substitutions from a given configuration.

        Args:
            plugin_cfg (Any): The object read from the configuration YAML file.
            file_name (str, optional): A file name used for prints.

        Returns:
            Dict[str, Dict[str, str]]: A dictionary where the key is either a
            string representing a file name or None representing all files. The
            value is a list of string substitutions.
        """
        subs = {}

        if ("StringSubstitutions" in
           plugin_cfg["DebugMacroCheck"]):
            if file_name:
                logging.debug(f"Loading global substitution data in "
                              f"{file_name}.")
            subs[None] = {}
            subs[None] |= plugin_cfg["DebugMacroCheck"]["StringSubstitutions"]

        if ("FileSubstitutions" in
           plugin_cfg["DebugMacroCheck"]):
            if file_name:
                logging.debug(f"Loading file scoped substitution data in "
                              f"{file_name}.")
            subs |= plugin_cfg["DebugMacroCheck"]["FileSubstitutions"]

        return subs

    def do_pre_build(self, builder: UefiBuilder) -> int:
        """Debug Macro Check pre-build functionality.

        The plugin is invoked in pre-build since it can operate independently
        of build tools and to notify the user of any errors earlier in the
        build process to reduce feedback time.

        Args:
            builder (UefiBuilder): A UEFI builder object for this build.

        Returns:
            int: The number of debug macro errors found. Zero indicates the
            check either did not run or no errors were found.
        """

        # Check if disabled in the environment
        env_disable = builder.env.GetValue("DISABLE_DEBUG_MACRO_CHECK")
        if not env_disable and "DISABLE_DEBUG_MACRO_CHECK" in os.environ:
            env_disable = os.environ["DISABLE_DEBUG_MACRO_CHECK"]

        if env_disable:
            return 0

        # Only run on targets with compilation
        build_target = builder.env.GetValue("TARGET").lower()
        if "no-target" in build_target:
            return 0

        edk2 = builder.edk2path
        package = edk2.GetContainingPackage(
            builder.edk2path.GetAbsolutePathOnThisSystemFromEdk2RelativePath(
                builder.env.GetValue("ACTIVE_PLATFORM")
            )
        )
        package_path = Path(
                          edk2.GetAbsolutePathOnThisSystemFromEdk2RelativePath(
                                package))

        # Every debug macro is printed at DEBUG logging level.
        # Ensure the level is above DEBUG while executing the macro check
        # plugin to avoid flooding the log handler.
        handler_level_context = []
        for h in logging.getLogger().handlers:
            if h.level < logging.INFO:
                handler_level_context.append((h, h.level))
                h.setLevel(logging.INFO)

        edk2_logging.log_progress("Checking DEBUG Macros")

        # There are two ways to specify macro substitution data for this
        # plugin. If multiple options are present, data is appended from
        # each option.
        #
        # 1. Specify the substitution data in the package CI YAML file.
        # 2. Specify a standalone substitution data YAML file.
        ##
        sub_data = {}

        # 1. Allow substitution data to be specified in a "DebugMacroCheck" of
        # the package CI YAML file. This is used to provide a familiar per-
        # package customization flow for a package maintainer.
        package_config_file = Path(
                                os.path.join(
                                    package_path, package + ".ci.yaml"))
        if package_config_file.is_file():
            with open(package_config_file, 'r') as cf:
                package_config_file_data = yaml.safe_load(cf)
                sub_data |= self._get_substitutions(package_config_file_data,
                                                    str(package_config_file))

        # 2. Allow a substitution file to be specified as an environment
        # variable. This is used to provide flexibility in how to specify a
        # substitution file. The value can be set anywhere prior to this plugin
        # getting called such as pre-existing build script.
        sub_file = builder.env.GetValue("DEBUG_MACRO_CHECK_SUB_FILE")
        if sub_file:
            with open(sub_file, 'r') as sf:
                sub_file_data = yaml.safe_load(sf)
                sub_data |= self._get_substitutions(sub_file_data, sub_file)

        try:
            error_count = DebugMacroCheck.check_macros_in_directory(
                                            package_path,
                                            ignore_git_submodules=False,
                                            show_progress_bar=False,
                                            macro_subs=sub_data)
        finally:
            for h, l in handler_level_context:
                h.setLevel(l)

        return error_count
