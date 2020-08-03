/** @file
  Defines last attempt status code ranges within the UEFI Specification
  defined unsuccessful vendor range.

  Copyright (c) Microsoft Corporation.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __LAST_ATTEMPT_STATUS_H__
#define __LAST_ATTEMPT_STATUS_H__

///
/// Last Attempt Status Unsuccessful Vendor Range Map
///
/// Update this map any time new ranges are added. Pre-existing range definitions cannot be modified
/// to keep status code definitions consistent over time.
///
/// START     | END       | Usage
/// --------------------------------------------------------------------------------------------------------------|
/// 0x1000    | 0x1FFF    | FmpDxe and other core infrastructure/libraries implemented in FmpDevicePkg            |
///    0x1000 |    0x107F | FmpDxe driver                                                                         |
///    0x1080 |    0x109F | Functionality that determines FMP dependencies (e.g. FmpDependencyLib)                |
/// 0x2000    | 0x3FFF    | FMP device-specific codes defined locally in an FmpDeviceLib instance implementation  |
///

///
/// Total length of the unsuccessful vendor range reserved for FMP ranges to be
/// defined for FmpDevicePkg usage.
///
#define LAST_ATTEMPT_STATUS_FMP_RESERVED_RANGE_LENGTH           0x1000

///
/// The minimum value of the FMP reserved range.
///
#define LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE   LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL_VENDOR_RANGE_MIN

///
/// The maximum value of the FMP reserved range.
///
#define LAST_ATTEMPT_STATUS_FMP_RESERVED_MAX_ERROR_CODE_VALUE   LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_FMP_RESERVED_RANGE_LENGTH - 1

///
/// Length of the error code range for FmpDxe driver-specific errors.
///
#define LAST_ATTEMPT_STATUS_DRIVER_ERROR_RANGE_LENGTH           0x80

///
/// The minimum value allowed for FmpDxe driver-specific errors.
///
#define LAST_ATTEMPT_STATUS_DRIVER_MIN_ERROR_CODE_VALUE         LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE

///
/// The maximum value allowed for FmpDxe driver-specific errors.
///
#define LAST_ATTEMPT_STATUS_DRIVER_MAX_ERROR_CODE_VALUE         LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_DRIVER_ERROR_RANGE_LENGTH - 1
///
/// Length of the error code range for FMP dependency related errors.
///
#define LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_RANGE_LENGTH       0x20

///
/// The maximum value allowed for FMP dependency related errors.
///
#define LAST_ATTEMPT_STATUS_DEPENDENCY_MIN_ERROR_CODE_VALUE     LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_DRIVER_ERROR_RANGE_LENGTH

///
/// The maximum value allowed for FMP dependency related errors.
///
#define LAST_ATTEMPT_STATUS_DEPENDENCY_MAX_ERROR_CODE_VALUE     LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_DRIVER_ERROR_RANGE_LENGTH + \
                                                                  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_RANGE_LENGTH - 1

///
/// Total length of the unsuccessful vendor range reserved for FMP device library
/// usage.
///
#define LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_RESERVED_RANGE_LENGTH  0x2000

///
/// The minimum value allowed for FMP device library errors.
///
#define LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_FMP_RESERVED_RANGE_LENGTH

///
/// The maximum value allowed for FMP device library errors.
///
#define LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MAX_ERROR_CODE_VALUE LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_MIN_ERROR_CODE_VALUE + \
                                                                  LAST_ATTEMPT_STATUS_DEVICE_LIBRARY_RESERVED_RANGE_LENGTH - 1

#endif
