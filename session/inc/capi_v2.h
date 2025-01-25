#ifndef CAPI_V2_H
#define CAPI_V2_H

/* ========================================================================*/
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/
/* ========================================================================*/
/*
 @file capi_v2.h
 Common Audio Processing Interface v2 header file

 This file defines a generalized C interface that can wrap a wide
 variety of audio processing modules, so that they can be treated the
 same way by control code.
 */
/*=========================================================================*/


#include "stdint.h"

/** General failure. */
#define CAPI_V2_EFAILED             ((uint32_t)1)
#define CAPI_V2_EOK 0

typedef struct capi_v2_t capi_v2_t;

typedef struct capi_v2_vtbl_t capi_v2_vtbl_t;

typedef uint32_t capi_v2_err_t;


typedef unsigned char bool_t;

typedef struct stage2_uv_wrapper_scratch_param {
    uint32_t reserved;
    uint32_t scratch_size;
    int8_t *scratch_ptr;
} stage2_uv_wrapper_scratch_param_t;

typedef struct pdk_stage2_result {
    uint32_t struct_size;
    uint32_t reserved;
    int32_t is_detected;
    int32_t best_confidence;
    int32_t start_position;
    int32_t end_position;  // relative position from current frame
    int32_t current_confidence;
} sva_result_t;

typedef enum PDK_STAGE2_STRUCT_ID {
    SVA_ID_CONFIG,
    SVA_ID_VERSION,
    SVA_ID_MEM_CONFIG,
    SVA_ID_SCRATCH_PARAM,
    SVA_ID_THRESHOLD_CONFIG,  /** threshold configuration */
    SVA_ID_REINIT_ALL,        /** initialize all internal variables */
    SVA_ID_REINIT_NETWORK,    /** initialize network layers */
    SVA_ID_RESULT,            /** retrieve detection result */
    SVA_ID_RMS_THRESHOLD_CONFIG,  /** RMS threshold configuration */
    SVA_ID_END_DETECTION_CONFIG,  /** keyword end detection configuration */
    SVA_ID_THRESHOLD_TABLE_CONFIG, /** threshold table */
} SVA_STRUCT_ID;


typedef struct pdk_stage2_threshold_config {
    uint32_t struct_size;
    int32_t smm_threshold;
} sva_threshold_config_t;

typedef struct stage2_uv_wrapper_threshold_config {
    uint32_t reserved[2];
    int32_t threshold;
    int32_t anti_spoofing_enabled;
    float anti_spoofing_threshold;
} stage2_uv_wrapper_threshold_config_t;

typedef struct stage2_uv_wrapper_result {
    uint32_t reserved[8];
    int32_t is_detected;
    int32_t is_anti_spoofing_passed;
    int32_t final_user_score;
    float vop2_user_score;
    float gmmsva_user_score;
    float antispoofing_score;
} stage2_uv_wrapper_result_t;

typedef struct stage2_uv_wrapper_stage1_uv_score {
    int32_t stage1_uv_score;
} stage2_uv_wrapper_stage1_uv_score_t;

typedef enum _STAGE2_UV_WRAPPER_STRUCT_ID {
    STAGE2_UV_WRAPPER_ID_VERSION = 0,
    STAGE2_UV_WRAPPER_ID_CONFIG,
    STAGE2_UV_WRAPPER_ID_MEM_INFO,
    STAGE2_UV_WRAPPER_ID_SCRATCH_PARAM,
    STAGE2_UV_WRAPPER_ID_INMODEL_BUFFER_SIZE,
    STAGE2_UV_WRAPPER_ID_MODEL_TYPE,
    STAGE2_UV_WRAPPER_ID_SCORE_PARAM,
    STAGE2_UV_WRAPPER_ID_THRESHOLD,
    STAGE2_UV_WRAPPER_ID_THRESHOLD_TYPE,
    STAGE2_UV_WRAPPER_ID_THRESHOLD_TABLE,
    STAGE2_UV_WRAPPER_ID_SVA_UV_SCORE,
    STAGE2_UV_WRAPPER_ID_REINIT,
    STAGE2_UV_WRAPPER_ID_RESULT,
    STAGE2_UV_WRAPPER_ID_RELEASE,
    STAGE2_UV_WRAPPER_ID_ENROLLMENT,
    STAGE2_UV_WRAPPER_ID_MATCHING,
    STAGE2_UV_WRAPPER_ID_INFO_ALL,
}STAGE2_UV_WRAPPER_STRUCT_ID;

typedef struct capi_v2_buf_t capi_v2_buf_t;

/** @addtogroup capiv2_data_types
@{ */
/** Contains input buffers, output buffers, property payloads, event payloads,
  and parameters that are passed into the CAPIv2 functions.
*/
struct capi_v2_buf_t {
    int8_t *data_ptr;
    /**< Data pointer to the raw data. The alignment depends on the format
         of the raw data. */

    uint32_t actual_data_len;
    /**< Length of the valid data (in bytes).

         For input buffers:
         - The caller fills this field with the number of bytes of valid data in
           the buffer.
         - The callee fills this field with the number of bytes of data it read.

         For output buffers:
         - The caller leaves this field uninitialized.
         - The callee fills it with the number of bytes of data it filled.
         @tablebulletend */

    uint32_t max_data_len;
    /**< Total allocated size of the buffer (in bytes).

         This value is always filled by the caller, and it is not modified by
         the callee. */
};


typedef struct capi_v2_stream_flags_t capi_v2_stream_flags_t;

/** Flags that are passed with every input buffer and must be filled by the
  module for every output buffer. These flags apply only to the buffer with
  which they are associated.
*/
struct capi_v2_stream_flags_t {
    uint32_t is_timestamp_valid :1;
    /**< Specifies whether the timestamp is valid.

         @values
         - 0 -- Not valid
         - 1 -- Valid @tablebulletend */

    uint32_t end_of_frame :1;
    /**< Specifies whether the buffer ends with the end of an encoded frame.

         @values
         - 0 -- The buffer might have an incomplete frame (relevant for
           compressed data only)
         - 1 --The buffer ends with the end of a frame. This allows the module
           to start processing without scanning for the end of frame.
          @tablebulletend */

    uint32_t marker_eos :1;
    /**< Indicates that this data is the last valid data from the upstream
         port. */

    uint32_t marker_1 :1;
    /**< Data marker 1 used by the service to track data.

         The module must propagate this marker from the input port to any output
         port that gets input from this port. */

    uint32_t marker_2 :1;
    /**< Data marker 2 used by the service to track data.

         The module must propagate this marker from the input port to any output
         port that gets input from this port. */

    uint32_t marker_3 :1;
    /**< Data marker 3 used by the service to track data.

         The module must propagate this marker from the input port to any output
         port that gets input from this port. */

    uint32_t reserved :26;
    /**< Reserved for future use. The client must ignore this value for input
         ports and set it to zero for output ports. */
};


typedef struct capi_v2_stream_data_t capi_v2_stream_data_t;

/** Data structure for one stream.
 */
struct capi_v2_stream_data_t {
    capi_v2_stream_flags_t flags;
    /**< Flags that indicate the stream properties. For more information on
         the flags, see the capi_v2_stream_flags_t structure. */

    int64_t timestamp;
    /**< Timestamp of the first data sample, in microseconds.

         The time origin is not fixed; it must be inferred from the timestamp of
         the first buffer. Negative values are allowed. */

    capi_v2_buf_t *buf_ptr;
    /**< Array of CAPI_V2 buffer elements.

         For deinterleaved unpacked uncompressed data, one buffer is to be
         used per channel. For all other cases, only one buffer is to be
         used. */

    uint32_t bufs_num;
    /**< Number of buffer elements in the buf_ptr array.

         For deinterleaved unpacked uncompressed data, this is equal to the
         number of channels in the stream. For all other cases, all the data
         is put in one buffer, so this field is set to 1. */
};


typedef struct capi_v2_port_info_t capi_v2_port_info_t;

/** @ingroup capiv2_data_types
  Port information payload structure.
 */
struct capi_v2_port_info_t {
    bool_t is_valid;
    /**< Indicates whether port_index is valid.

         @values
         - 0 -- Not valid
         - 1 -- Valid @tablebulletend */

    bool_t is_input_port;
    /**< Indicates the type of port.

         @values
         - TRUE -- Input port
         - FALSE -- Output port @tablebulletend */

    uint32_t port_index;
    /**< Identifies the port.

         Index values must be sequential numbers starting from zero. There are
         separate sequences for input and output ports. For example, if a
         module has three input ports and two output ports:
         - The input ports have index values of 0, 1, and 2.
         - The output ports have index values of 0 and 1.

         When capi_v2_vtbl_t::process() is called:
         - The data in input[0] is for input port 0.
         - The data in input[1] is for input port 1.
         - Etc.
         - Output port 0 must fill data into output[0].
         - Output port 1 must fill data into output[1].
         - Etc. @tablebulletend @newpagetable */
};

enum capi_v2_property_id_t {
    //----------- Properties that can be queried statically using get_static_properties ---------------------------------
    CAPI_V2_INIT_MEMORY_REQUIREMENT,
    /**< Amount of memory (in bytes) to be passed into the capi_v2_init()
         function. @vertspace{4}

         Payload structure: capi_v2_init_memory_requirement_t @vertspace{6} */

    CAPI_V2_STACK_SIZE,
    /**< Stack size required by this module (in bytes). @vertspace{4}

         Payload structure: capi_v2_stack_size_t @vertspace{6} */

    CAPI_V2_MAX_METADATA_SIZE,
    /**< Maximum size of metadata generated by this module after each call
         to capi_v2_vtbl_t::process(). @vertspace{4}

         Payload structure: capi_v2_max_metadata_size_t @vertspace{4}

         If this value is zero, the module does not generate any metadata.
         The value includes the sizes of different structures used to pack
         metadata (see #CAPI_V2_METADATA). @vertspace{6} */

    CAPI_V2_IS_INPLACE,
    /**< Indicates whether the module can perform in-place computation.
         @vertspace{4}

         Payload structure: capi_v2_is_inplace_t @vertspace{4}

         If this value is TRUE, the caller can provide the same pointers for
         input and output (but this is not guaranteed). In this case, the input
         and output data format be the same, and the requires_data_buffering
         property be set to FALSE. @vertspace{6} */

    CAPI_V2_REQUIRES_DATA_BUFFERING,
    /**< Specifies whether data buffering is TRUE or FALSE. @vertspace{4}

         Payload structure: capi_v2_requires_data_buffering_t @vertspace{4}

         If this value is FALSE and the module does not require a port data
         threshold (#CAPI_V2_PORT_DATA_THRESHOLD), the module must behave as
         follows:
         @vertspace{6}
         - The number of output samples on all output ports must always be the
           same as the number of input samples. The caller must ensure that
           the number of input samples is the same on all input ports.
         - All of the input must be consumed. The caller must ensure that the
           output buffers have enough space.
         - The module must be able to handle any number of samples. @vertspace{6}
         @newpage

         If this value is FALSE and the module requires a port data threshold
          (#CAPI_V2_PORT_DATA_THRESHOLD), the module must behave as follows:
         @vertspace{6}
         - The number of samples per buffer on all input and output ports
           must be the same as the threshold.
         - The caller must call the module with the required number of samples.
         - All of the input must be consumed. The caller must ensure that the
           output buffers have enough space. @vertspace{6}

         If this value is TRUE, the module must behave as follows:
         @vertspace{6}
         - The module must define a threshold in number of bytes for each
           input port and each output port.
         - The module must consume data on its input ports and fill data on its
           output ports until one of the following occurs: @vertspace{4}
             - The amount of remaining data in each buffer of at least one
               input port is less than its threshold @vertspace{-2}
             - Or the amount of free space in each buffer of at least one
               output port is less than its threshold. @vertspace{6}

         When this value is set to TRUE, significant overhead is added. Use the
         TRUE setting only under the following circumstances: @vertspace{6}
         - The module performs encoding/decoding of data.
         - The module performs rate conversion between the input and output.
         @vertspace{6} */

    CAPI_V2_NUM_NEEDED_FRAMEWORK_EXTENSIONS,
    /**< Number of framework extensions required by this module. @vertspace{4}

         Payload structure: capi_v2_num_needed_framework_extensions_t
         @vertspace{6} */

    CAPI_V2_NEEDED_FRAMEWORK_EXTENSIONS,
    /**< List of framework extensions that are required by the module.
         @vertspace{4}

         Payload structure: An array of capi_v2_framework_extension_id_t
         structures @vertspace{6}
         - Each value is the ID of a framework extension.
         - The number of elements of the array is the number returned in the
           query for the #CAPI_V2_NUM_NEEDED_FRAMEWORK_EXTENSIONS property.
           @vertspace{6} */

    CAPI_V2_INTERFACE_EXTENSIONS,
    /**< List of interface extensions provided by the client.
         @vertspace{4}

         The module must set the is_supported flag to TRUE or FALSE to indicate
         whether the module supports this extension.
         Additional data can also be exchanged using an optional buffer per
         extension. @vertspace{4}

         Payload structure: capi_v2_interface_extns_list_t @vertspace{6} */

    CAPI_V2_MAX_STATIC_PROPERTIES = 0x10000,
    /**< Dummy value that indicates the range of properties.
         @vertspace{6} */

    //------------ End of properties that can be queried statically -------------------------

    //-------------- Properties that can be set at init and at any time after init ----------------------------
    CAPI_V2_EVENT_CALLBACK_INFO,
    /**< Function pointer and state required for raising events to the
         framework. @vertspace{4}

         Payload structure: capi_v2_event_callback_info_t @vertspace{6} */

    CAPI_V2_PORT_NUM_INFO,
    /**< Sets the number of input and output ports for the module.
         @vertspace{4}

         Payload structure: capi_v2_port_num_info_t @vertspace{6} */

    CAPI_V2_HEAP_ID,
    /**< Provides the heap IDs for allocating memory. @vertspace{4}

         Payload structure: capi_v2_heap_id_t @vertspace{6} */

    CAPI_V2_INPUT_MEDIA_FORMAT,
    /**< Sets the media format for an input port.
         The caller must set the port ID in the payload. @vertspace{4}

         Payload structure: capi_v2_set_get_media_format_t @vertspace{6} */

    CAPI_V2_LOG_CODE,
    /**< Provides the logging code to the module for logging module data.
         @vertspace{4}

         Payload structure: capi_v2_log_code_t @vertspace{6} */

    CAPI_V2_CUSTOM_INIT_DATA,
    /**< Provides module-specific initialization data.
         This property ID is typically set only at initialization time.
         @vertspace{4}

         Payload structure: Module-specific @vertspace{6} */

    CAPI_V2_SESSION_IDENTIFIER,
    /**< Provides values that allow the module to uniquely identify its
         placement and session.

         Payload structure: capi_v2_session_identifier_t @vertspace{6} */

    CAPI_V2_MAX_INIT_PROPERTIES = 0x20000,
    /**< Dummy value that indicates the range of properties.
         @vertspace{6} */

    //-------------- End of properties that can be set at init and at any time after init ----------------------

    //-------------- Properties that can only be set after init -------------
    CAPI_V2_ALGORITHMIC_RESET,
    /**< Specifies whether the module is to reset any internal buffers and the
         algorithm state to zero. @vertspace{4}

         Payload structure: Empty buffer @vertspace{4}

         Settings do not need to be reset, and memory allocated by the module
         does not need to be freed. @vertspace{6} */

    CAPI_V2_EXTERNAL_SERVICE_ID,
    /**< Currently not used. @vertspace{6} */

    CAPI_V2_REGISTER_EVENT_DATA_TO_DSP_CLIENT,
    /**< Specifies the registration information for an event of type
         #CAPI_V2_EVENT_DATA_TO_DSP_CLIENT. @vertspace{4}

         Payload structure: capi_v2_register_event_to_dsp_client_t @vertspace{6} */

    CAPI_V2_MAX_SET_PROPERTIES = 0x30000,
    /**< Dummy value that indicates the range of properties. @vertspace{6} */

    //-------------- End of properties that can be set any time -------------

    //-------------- Properties that can only be queried only using get properties ---------------
    CAPI_V2_METADATA,
    /**< Specifies that the module must fill in any metadata that it generated
         during a capi_v2_vtbl_t::process() call when the caller queries this
         parameter. The query is typically done after the module raises
         #CAPI_V2_EVENT_METADATA_AVAILABLE. @vertspace{4}

         Payload structure: capi_v2_metadata_t @vertspace{4}

         Metadata is put in the payload of capi_v2_metadata_t.
         The module must pack the payload of metadata as groups of
         asm_stream_param_data_t (defined in
         @xrefcond{Q3,80-NF774-1,80-NA610-1}). @vertspace{4}

         The buffer is structured as follows: @vertspace{6}
         - First asm_stream_param_data_v2_t
         - First metadata
         - Second asm_stream_param_data_v2_t
         - Second metadata
         - Etc. @vertspace{6} */
    /* asm_stream_param_data_t is in adsp_asm_stream_commands.h */

    CAPI_V2_PORT_DATA_THRESHOLD,
    /**< Threshold of an input or output port (in bytes). @vertspace{4}

         Payload structure: capi_v2_port_data_threshold_t @vertspace{4}

         This property ID is used only for modules that require a specific
         amount of data on any port. The module behavior depends on whether
         the module requires data buffering.
         For more information, see #CAPI_V2_REQUIRES_DATA_BUFFERING.
         @vertspace{6} */

    CAPI_V2_OUTPUT_MEDIA_FORMAT_SIZE,
    /**< Size of the media format payload for an output port.
         The size excludes the size of capi_v2_data_format_header_t.
         @vertspace{4}

         Payload structure: capi_v2_output_media_format_size_t @vertspace{6} */

    CAPI_V2_MAX_GET_PROPERTIES = 0x40000,
    /**< Dummy value that indicates the range of properties.
         @vertspace{6} */
    //------------ End of properties that can only be queried using get properties -------------------------

    //-------------- Properties that can be set or queried using set/get properties only ------------
    CAPI_V2_OUTPUT_MEDIA_FORMAT,
    /**< Queries the media format for a specified output port. @vertspace{4}

         Payload structure: capi_v2_set_get_media_format_t @vertspace{4}

         This property ID can also be used to set the output media format for
         modules that support control of the output media format. @vertspace{4}

         If a module supports control of some aspects such as the sample rate
         only, all other fields can be set to #CAPI_V2_DATA_FORMAT_INVALID_VAL.
         The caller must set the port ID in the payload. @vertspace{6} */

    CAPI_V2_CUSTOM_PROPERTY,
    /**< Sets and gets a property that is specific to a framework
          extension. This property ID can also be used to statically get a
          module-specific property. @vertspace{4}

          Payload structure: capi_v2_custom_property_t @vertspace{4}

          The framework extension must define a secondary property ID and
          corresponding payload structure (capi_v2_custom_property_t) that are
          specific to the information the property is to set or get.
          @vertspace{6} */

    CAPI_V2_MAX_SET_GET_PROPERTIES = 0x50000,
    /**< Dummy value that indicates the range of properties.
         @vertspace{6} */

    CAPI_V2_MAX_PROPERTY = 0x7FFFFFFF
    /**< Maximum value that a property ID can take. */

};


typedef struct capi_v2_prop_t capi_v2_prop_t;

/** @addtogroup capiv2_data_types
@{ */
/** Contains properties that can be sent to a module.

  Properties are used for generic set and get commands, which are independent
  of the underlying module.
 */
struct capi_v2_prop_t {
    capi_v2_property_id_t id;
    /**< Unique identifier of the property that is being sent. */

    capi_v2_buf_t payload;
    /**< Payload buffer.

         The buffer must contain the payload corresponding to the property value
         for the set_property call, and it must be sufficiently large to contain
         the payload for a get_property call. */

    capi_v2_port_info_t port_info;
    /**< Information about the port for which the property is applicable.

         If the property is is applicable to any port, the is_valid flag must be
         set to FALSE in the port information. */
};

typedef struct capi_v2_proplist_t capi_v2_proplist_t;
/** Contains a list of CAPI_V2 properties. This structure can be used to send
  a list of properties to the module or query for the properties.
 */struct capi_v2_proplist_t {
    uint32_t props_num;       /**< Number of elements in the array. */
	capi_v2_prop_t *prop_ptr; /**< Array of CAPI_V2 property elements. */
};

typedef enum capi_v2_property_id_t capi_v2_property_id_t;


struct capi_v2_vtbl_t {
    /**
    Processes input data on all input ports and provides output on all output
    ports.

    @datatypes
    capi_v2_t \n
    capi_v2_stream_data_t

    @param[in,out] _pif   Pointer to the module object.
    @param[in,out] input  Array of pointers to the input data for each input
                          port. \n
                          The length of the array is the number of input ports.
                          The client sets the number of input ports using the
                          #CAPI_V2_PORT_NUM_INFO property. \n
                          The function must modify the actual_data_len field
                          to indicate how many bytes were consumed.
    @param[out] output    Array of pointers to the output data for each output
                          port. \n
                          The client sets the number of output ports using the
                          #CAPI_V2_PORT_NUM_INFO property. \n
                          The function sets the actual_data_len field to
                          indicate how many bytes were generated.

    @detdesc
    This is a generic processing function.
    @par
    On each call to capi_v2_vtbl_t::process(), the behavior of the module
    depends on the value it returned for the #CAPI_V2_REQUIRES_DATA_BUFFERING
    property. For a description of the behavior, refer to the comments for
    CAPI_V2_REQUIRES_DATA_BUFFERING.
    @par
    No debug messages are allowed in this function.

    @return
    Indication of success or failure.

    @dependencies
    A valid input media type must have been set on each input port using the
    #CAPI_V2_INPUT_MEDIA_FORMAT property. @newpage
    */
    capi_v2_err_t (*process)(capi_v2_t *_pif,
                             capi_v2_stream_data_t *input[],
                             capi_v2_stream_data_t *output[]);

    /**
     Frees any memory allocated by the module.

     @datatypes
     capi_v2_t

     @param[in,out] _pif    Pointer to the module object.

     @note1hang After calling this function, _pif is no longer a valid CAPIv2
                object. Do not call any CAPIv2 functions after using it.

     @return
     Indication of success or failure.

     @dependencies
     None. @newpage
     */
    capi_v2_err_t (*end)(capi_v2_t *_pif);

    /**
    Sets a parameter value based on a unique parameter ID.

    @datatypes
    capi_v2_t \n
    capi_v2_port_info_t \n
    capi_v2_buf_t

    @param[in,out] _pif      Pointer to the module object.
    @param[in] param_id      ID of the parameter whose value is to be set.
    @param[in] port_info_ptr Pointer to the information about the port on
                             which this function must operate. \n
                             If a valid port ID is not provided, either the
                             port index does not matter for the param_id or
                             the param_id is applicable to all ports.
    @param[in] params_ptr    Pointer to the buffer containing the value of the
                             parameter.
                             The format of the data in the buffer depends on
                             the implementation.

    @detdesc
    In the event of a failure, the appropriate error code is to be returned.
    @par
    The actual_data_len field of the parameter pointer is to be at least the
    size of the parameter structure. Therefore, the following check must be
    exercised for each tuning parameter ID:
    @par
    @code
    if (params_ptr->actual_data_len >= sizeof(asm_gain_struct_t))
    {
    :
    :
    }
    else
    {
    MSG_1(MSG_SSID_QDSP6, DBG_ERROR_PRIO,"CAPI_V2 Libname Set, Bad param size
    %lu",params_ptr->actual_data_len);
    return ADSP_ENEEDMORE;
    }

    @endcode
    @par
    Optionally, some parameter values can be printed for tuning verification.
    @par
    @note1 In this code sample, asm_gain_struct is an example only.
           Use the correct structure based on the parameter ID.

    @return
    None.

    @dependencies
    None. @newpage
    */
    capi_v2_err_t (*set_param)(capi_v2_t *_pif,
                               uint32_t param_id,
                               const capi_v2_port_info_t *port_info_ptr,
                               capi_v2_buf_t *params_ptr);

    /**
    Gets a parameter value based on a unique parameter ID.

    @datatypes
    capi_v2_t \n
    capi_v2_port_info_t \n
    capi_v2_buf_t

    @param[in,out] _pif      Pointer to the module object.
    @param[in]     param_id  Parameter ID of the parameter whose value is
                             being passed in this function. For example:\n
                             - CAPI_V2_LIBNAME_ENABLE
                             - CAPI_V2_LIBNAME_FILTER_COEFF @tablebulletend
    @param[in] port_info_ptr Pointer to the information about the port on which
                             this function must operate. \n
                             If the port is invalid, either the port index does
                             not matter for the param_id or or the param_id
                             is applicable to all ports.
    @param[out] params_ptr   Pointer to the buffer that is to be filled with
                             the value of the parameter. The format depends on
                             the implementation.

    @detdesc
    In the event of a failure, the appropriate error code is to be returned.
    @par
    The max_data_len field of the parameter pointer must be at least the size
    of the parameter structure. Therefore, the following check must be
    exercised for each tuning parameter ID.
    @par
    @code
    if (params_ptr->max_data_len >= sizeof(asm_gain_struct_t))
    {
    :
    :
    }
    else
    {
    MSG_1(MSG_SSID_QDSP6, DBG_ERROR_PRIO,"CAPI_V2 Libname Get, Bad param size
    %lu",params_ptr->max_data_len);
    return ADSP_ENEEDMORE;
    }

    @endcode
    @par
    Before returning, the actual_data_len field must be filled with the number
    of bytes written into the buffer.
    @par
    Optionally, some parameter values can be printed for tuning verification.
    @par
    @note1 In this code sample, asm_gain_struct is an example only.
           Use the correct structure based on the parameter ID.

    @newpage
    @return
    Indication of success or failure.

    @dependencies
    None. @newpage
    */
    capi_v2_err_t (*get_param)(capi_v2_t *_pif,
                               uint32_t param_id,
                               const capi_v2_port_info_t *port_info_ptr,
                               capi_v2_buf_t *params_ptr);

    /**
    Sets a list of property values.

    @datatypes
    capi_v2_t \n
    capi_v2_proplist_t

    @param[in,out] _pif      Pointer to the module object.
    @param[in] proplist_ptr  Pointer to the list of property values.

    @detdesc
    In the event of a failure, the appropriate error code is to be returned.
    @par
    Optionally, some property values can be printed for debugging.

    @return
    None.

    @dependencies
    None. @newpage
    */
    capi_v2_err_t (*set_properties)(capi_v2_t *_pif,
                                    capi_v2_proplist_t *proplist_ptr);

    /**
    Gets a list of property values.

    @datatypes
    capi_v2_t \n
    capi_v2_proplist_t

    @param[in,out] _pif       Pointer to the module object.
    @param[out] proplist_ptr  Pointer to the list of empty structures that
                              must be filled with the appropriate property
                              values, which are based on the property IDs
                              provided. \n \n
                              The client must fill some elements
                              of the structures as input to the module. These
                              elements must be explicitly indicated in the
                              structure definition.

    @detdesc
    In the event of a failure, the appropriate error code is to be returned.

    @return
    Indication of success or failure.

    @dependencies
    None.
    */
    capi_v2_err_t (*get_properties)(capi_v2_t *_pif,
                                    capi_v2_proplist_t *proplist_ptr);
};

/** @ingroup capiv2_func_wrapper
  Plain C interface wrapper for the virtual function table, capi_v2_vtbl_t.

  This capi_v2_t structure appears to the caller as a virtual function table.
  The virtual function table in the instance structure is followed by other
  structure elements, but those are invisible to the users of the CAPI_V2
  object. This capi_v2_t structure is all that is publicly visible.
 */
struct capi_v2_t {
    const capi_v2_vtbl_t *vtbl_ptr; /**< Pointer to the virtual function table. */
};

/** @ingroup capiv2_func_get_static_prop
  Queries for properties as follows:
  - Static properties of the module that are independent of the instance
  - Any property that is part of the set of properties that can be statically
    queried

  @datatypes
  capi_v2_proplist_t

  @param[in] init_set_proplist Pointer to the same properties that are sent in
                               the call to capi_v2_init_f().
  @param[out] static_proplist  Pointer to the property list structure. \n
                               The client fills in the property IDs for which
                               it needs property values. The client also
                               allocates the memory for the payloads.
                               The module must fill in the information in this
                               memory.

  @detdesc
  This function is used to query the memory requirements of the module to
  create an instance. The function must fill in the data for the properties in
  the static_proplist.
  @par
  As an input to this function, the client must pass in the property list that
  it passes to capi_v2_init_f(). The module can use the property
  values in init_set_proplist to calculate its memory requirements.
  @par
  The same properties that are sent to the module in the call to
  %capi_v2_init_f() are also sent to this function to enable the module to
  calculate the memory requirement.

  @return
  Indication of success or failure.

  @dependencies
  None.
 */
typedef capi_v2_err_t (*capi_v2_get_static_properties_f)(
    capi_v2_proplist_t *init_set_proplist,
    capi_v2_proplist_t *static_proplist);

/** @ingroup capiv2_func_init
  Instantiates the module to set up the virtual function table, and also
  allocates any memory required by the module.

  @datatypes
  capi_v2_t \n
  capi_v2_proplist_t

  @param[in,out] _pif          Pointer to the module object. \n
                               The memory has been allocated by the client
                               based on the size returned in the
                               #CAPI_V2_INIT_MEMORY_REQUIREMENT property.
  @param[in] init_set_proplist Pointer to the properties set by the service
                               to be used while initializing.

  @detdesc
  States within the module must be initialized at the same time.
  @par
  All return codes returned by this function, except #CAPI_V2_EOK, are
  considered to be FATAL.
  @par
  For any unsupported property ID passed in the init_set_proplist parameter,
  the function prints a message and continues processing other property IDs.

  @return
  Indication of success or failure.

  @dependencies
  */
typedef capi_v2_err_t (*capi_v2_init_f)(
    capi_v2_t *_pif,
    capi_v2_proplist_t *init_set_proplist);

#endif /* #ifndef CAPI_V2_H */
