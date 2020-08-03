/** @file
  Defines private last attempt status codes used in FmpDevicePkg.

  Copyright (c) Microsoft Corporation.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FMP_LAST_ATTEMPT_STATUS_H__
#define __FMP_LAST_ATTEMPT_STATUS_H__

///
/// Last attempt status codes defined for additional granularity in FmpDevicePkg components.
///
/// These codes are defined within the last attempt status FMP reserved range which extends from
/// LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE to LAST_ATTEMPT_STATUS_FMP_RESERVED_MAX_ERROR_CODE_VALUE.
///
/// The following last attempt status code ranges are currently defined for the corresponding component:
///   * LAST_ATTEMPT_STATUS_DRIVER - FMP driver
///   * LAST_ATTEMPT_STATUS_DEPENDENCY - FMP dependency functionality
///
/// Future last attempt status code additions in FmpDevicePkg should be added as follows:
///   * FmpDxe driver: Onto the end of the LAST_ATTEMPT_STATUS_DRIVER_ERROR range
///   * FMP dependency functionality: Onto the end of the LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR range
///   * Other components: Add a new range onto the end of the last existing range in the enum within the limits of
///     [LAST_ATTEMPT_STATUS_FMP_RESERVED_MIN_ERROR_CODE_VALUE,LAST_ATTEMPT_STATUS_FMP_RESERVED_MAX_ERROR_CODE_VALUE]
///
/// The value of pre-existing last attempt status codes should never be modified to ensure the values remain
/// consistent over time.
///
enum LAST_ATTEMPT_STATUS_EXPANDED_ERROR_LIST
{
  ///
  /// Last attempt status codes used in FmpDxe
  ///
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_GET_FMP_HEADER                 = LAST_ATTEMPT_STATUS_DRIVER_MIN_ERROR_CODE_VALUE,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_PROGRESS_CALLBACK_ERROR        ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_CHECK_POWER_API                ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_CHECK_SYS_THERMAL_API          ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_THERMAL                        ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_CHECK_SYS_ENV_API              ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_SYSTEM_ENV                     ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_GET_FMP_HEADER_SIZE            ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_GET_ALL_HEADER_SIZE            ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_GET_FMP_HEADER_VERSION         ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_IMAGE_NOT_PROVIDED             ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_IMAGE_NOT_UPDATABLE            ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_INVALID_CERTIFICATE            ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_INVALID_IMAGE_INDEX            ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_INVALID_KEY_LENGTH             ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_INVALID_KEY_LENGTH_VALUE       ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_VERSION_TOO_LOW                ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_DEVICE_LOCKED                  ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_IMAGE_AUTH_FAILURE             ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_PROTOCOL_ARG_MISSING           ,
  LAST_ATTEMPT_STATUS_DRIVER_ERROR_MAX_ERROR_CODE                 = LAST_ATTEMPT_STATUS_DRIVER_MAX_ERROR_CODE_VALUE,

  ///
  /// Last attempt status codes used in FMP dependency related functionality
  ///
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_GET_DEPEX_FAILURE                  = LAST_ATTEMPT_STATUS_DEPENDENCY_MIN_ERROR_CODE_VALUE,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_NO_END_OPCODE                      ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_UNKNOWN_OPCODE                     ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_FMP_PROTOCOL_NOT_FOUND             ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_MEM_ALLOC_FMP_INFO_BUFFER_FAILED   ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_MEM_ALLOC_DESC_VER_BUFFER_FAILED   ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_MEM_ALLOC_DESC_SIZE_BUFFER_FAILED  ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_MEM_ALLOC_FMP_VER_BUFFER_FAILED    ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_GUID_BEYOND_DEPEX                  ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_VERSION_BEYOND_DEPEX               ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_VERSION_STR_BEYOND_DEPEX           ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_FMP_NOT_FOUND                      ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_PUSH_FAILURE                       ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_POP_FAILURE                        ,
  LAST_ATTEMPT_STATUS_DEPENDENCY_ERROR_MAX_ERROR_CODE                     = LAST_ATTEMPT_STATUS_DEPENDENCY_MAX_ERROR_CODE_VALUE

};

#endif
