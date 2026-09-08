/*
 * Copyright (c) 2021-2022, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Changes from Qualcomm Technologies, Inc. are provided under the following license:
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define ATRACE_TAG (ATRACE_TAG_AUDIO | ATRACE_TAG_HAL)
#define LOG_TAG "PAL: ASREngine"

#include "ASREngine.h"

#include <cmath>
#include <cutils/trace.h>
#include <string.h>
#include "Session.h"
#include "SessionAR.h"
#include "Stream.h"
#include "StreamASR.h"
#include "ResourceManager.h"
#include "kvh2xml.h"

#define ASR_DBG_LOGS
#ifdef ASR_DBG_LOGS
#undef PAL_DBG
#define PAL_DBG(LOG_TAG,...)  PAL_INFO(LOG_TAG,__VA_ARGS__)
#endif

#define FILENAME_LEN 128
std::shared_ptr<ASREngine> ASREngine::eng;

ASREngine::ASREngine(Stream *s, std::shared_ptr<ASRStreamConfig> smCfg)
{
    PAL_DBG(LOG_TAG, "Enter");
    int status = 0;
    struct pal_stream_attributes sAttr;
    std::shared_ptr<ResourceManager> rm = nullptr;
    static std::shared_ptr<ASREngine> eng = nullptr;

    isCrrDevUsingExtEc = false;
    exitThread = false;
    loggerModeEnabled = false;
    timestampEnabled = false;
    outputBufSize = 0;
    ecRefCount = 0;
    devDisconnectCount = 0;
    numOutput = 0;
    rxEcDev = nullptr;
    asrInfo = nullptr;
    this->smCfg = smCfg;
    engState = ASR_ENG_IDLE;
    streamHandle = s;
    builder = new PayloadBuilder();

    asrInfo = ASRPlatformInfo::GetInstance();
    if (!asrInfo) {
        PAL_ERR(LOG_TAG, "No ASR platform present");
        throw std::runtime_error("No ASR platform present");
    }

    for (int i = ASR_INPUT_CONFIG; i < ASR_MAX_PARAM_IDS; i++) {
        paramIds[i] = smCfg->GetParamId((asr_param_id_type)i);
        moduleTagIds[i] = smCfg->GetModuleTagId((asr_param_id_type)i);
    }

    status = streamHandle->getStreamAttributes(&sAttr);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to get stream attributes");
        throw std::runtime_error("Failed to get stream attributes");
   }

    rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get ResourceManager instance");
        throw std::runtime_error("Failed to get ResourceManager instance");
    }

    session = Session::makeSession(rm, &sAttr);
    if (!session) {
        PAL_ERR(LOG_TAG, "Failed to create session");
        throw std::runtime_error("Failed to create session");
    }

    session->registerCallBack(HandleSessionCallBack, (uint64_t)this);

    eventThreadHandler = std::thread(ASREngine::EventProcessingThread, this);
    if (!eventThreadHandler.joinable()) {
        PAL_ERR(LOG_TAG, "Error:%d failed to create event processing thread",
               status);
        throw std::runtime_error("Failed to create event processing thread");
    }

    PAL_DBG(LOG_TAG, "Exit");
}

ASREngine::~ASREngine()
{
    PAL_INFO(LOG_TAG, "Enter");

    smCfg = nullptr;
    asrInfo = nullptr;
    session = nullptr;
    streamHandle = nullptr;

    if (builder) {
        delete builder;
        builder = nullptr;
    }

    {
        std::unique_lock<std::mutex> lck(mutexEngine);
        exitThread = true;
        cv.notify_one();
    }

    if (eventThreadHandler.joinable()) {
        eventThreadHandler.join();
    }

    PAL_INFO(LOG_TAG, "Exit");
}

std::shared_ptr<ASREngine> ASREngine::GetInstance(
     Stream *s,
     std::shared_ptr<ASRStreamConfig> smCfg)
{
     if (!eng)
         eng = std::make_shared<ASREngine>(s, smCfg);

     return eng;
}

bool ASREngine::IsEngineActive()
{
    if (engState == ASR_ENG_ACTIVE ||
        engState == ASR_ENG_TEXT_RECEIVED)
        return true;

    return false;
}

int32_t ASREngine::setParameters(Stream *s, asr_param_id_type_t pid, void *paramPayload)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter, param id %d ", pid);

    uint32_t tagId = 0;
    uint32_t paramId = 0;
    uint8_t *payload = nullptr;
    uint8_t *data = nullptr;
    size_t payloadSize = 0;
    size_t dataSize = 0;
    uint32_t sesParamId = 0;
    uint32_t miid = 0;
    uint32_t id = pid;
    StreamASR* sAsr = dynamic_cast<StreamASR *>(s);

    if (pid < ASR_INPUT_CONFIG || pid >= ASR_MAX_PARAM_IDS) {
        PAL_ERR(LOG_TAG, "Invalid param id %d", pid);
        status = -EINVAL;
        goto exit;
    }

    tagId = moduleTagIds[pid];
    paramId = paramIds[pid];

    status = dynamic_cast<SessionAR*>(session)->getMIID(nullptr, tagId, &miid);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to get instance id for tag %x, status = %d",
                tagId, status);
        goto exit;
    }

    switch (id) {
        case ASR_INPUT_CONFIG : {
            param_id_asr_config_t *config = sAsr->GetSpeechConfig();
            if (config == nullptr) {
                PAL_ERR(LOG_TAG, "No config available, can't start the engine!!!");
                goto exit;
            }
            data = (uint8_t *)config;
            dataSize = sizeof(param_id_asr_config_t);
            sesParamId = PAL_PARAM_ID_ASR_CONFIG;
            break;
        }
        case ASR_ABORT_EVENT : {
            /* Abort event is triggered from stream, we need to push dummy event in event queue
             * and leverage the event handling flow.
             */
            event_id_asr_output_event_t *event = (event_id_asr_output_event_t *)
                                 calloc(1, sizeof(event_id_asr_output_event_t));
            if (event == nullptr) {
                PAL_ERR(LOG_TAG, "Failed to allocate memory for ASR output event");
                goto exit;
            }

            event->event_payload_type = ASR_ABORT_TYPE;
            event->output_token = 0;
            event->num_outputs = 0;
            event->payload_size = 0;
            eventQ.push({EVENT_ID_ASR_OUTPUT, event});
            cv.notify_one();
            break;
        }
        case ASR_FORCE_OUTPUT : {
            if (loggerModeEnabled) {
                /* In logger mode, HLOS will not get any event from DSP, HLOS need to
                 * trigger a getParam to get the transcription, hence following dummy event
                 * is pushed in the event queue, so that getParam and all other handling
                 * can be leveraged by logger mode. Number of output is updated with 1,
                 * and payloadSize with outputBufSize, as these will be used by payload
                 * builder during getParam, as DSP will send payload of output buffer size,
                 * which was used while setting up the usecase.
                 */
                event_id_asr_output_event_t *event = (event_id_asr_output_event_t *)
                                     calloc(1, sizeof(event_id_asr_output_event_t));
                if (event == nullptr) {
                    PAL_ERR(LOG_TAG, "Failed to allocate memory for ASR output event");
                    goto exit;
                }
                event->output_token = 0;
                event->num_outputs = 1;
                event->payload_size = outputBufSize;
                eventQ.push({EVENT_ID_ASR_OUTPUT, event});
                cv.notify_one();
                goto exit;
            }

            param_id_asr_force_output_t *param = (param_id_asr_force_output_t *)
                                    calloc(1, sizeof(param_id_asr_force_output_t));
            if (param == nullptr) {
                 PAL_ERR(LOG_TAG, "Failed to allocate memory for ASR force output config!!!");
                 goto exit;
            }
            param->force_output = 1;
            data = (uint8_t *)param;
            dataSize = sizeof(param_id_asr_force_output_t);
            sesParamId = PAL_PARAM_ID_ASR_FORCE_OUTPUT;
            break;
        }
        case SDZ_FORCE_OUTPUT : {
             param_id_sdz_force_output_t *param = (param_id_sdz_force_output_t *)
                                   calloc(1, sizeof(param_id_sdz_force_output_t));
             if (param == nullptr) {
                 PAL_ERR(LOG_TAG, "Failed to allocate memory for SDZ force output config!!!");
                 goto exit;
             }
             param->force_output = 1;
             data = (uint8_t *)param;
             dataSize = sizeof(param_id_sdz_force_output_t);
             sesParamId = PAL_PARAM_ID_SDZ_FORCE_OUTPUT;
             break;
        }
        case ASR_OUTPUT_CONFIG : {
            param_id_asr_output_config_t *opConfig = sAsr->GetOutputConfig();
            if (opConfig == nullptr) {
                PAL_ERR(LOG_TAG, "No output config available, can't start the engine!!!");
                goto exit;
            }
            if (opConfig->output_mode == LOGGER || opConfig->output_mode == TS_LOGGER)
                loggerModeEnabled = true;
            if (opConfig->output_mode == TS_BUFFERED ||
                opConfig->output_mode == TS_NON_BUFFERED ||
                opConfig->output_mode == TS_LOGGER)
                timestampEnabled = true;
            outputBufSize = opConfig->out_buf_size;
            data = (uint8_t *)opConfig;
            dataSize = sizeof(param_id_asr_output_config_t);
            sesParamId = PAL_PARAM_ID_ASR_OUTPUT;
            break;
        }
        case SDZ_OUTPUT_CONFIG : {
            param_id_sdz_output_config_t *opConfigSdz = sAsr->GetSdzOutputConfig();
            if (opConfigSdz == nullptr) {
                PAL_ERR(LOG_TAG, "No output config available for Sdz!!!");
                goto exit;
            }
            data = (uint8_t *)opConfigSdz;
            dataSize = sizeof(param_id_sdz_output_config_t);
            sesParamId = PAL_PARAM_ID_SDZ_OUTPUT;
            break;
        }
        case ASR_INPUT_BUF_DURATON: {
            param_id_asr_input_threshold_t *ipConfig = sAsr->GetInputBufConfig();
            if (ipConfig == nullptr) {
                PAL_ERR(LOG_TAG, "No input config available, can't start the engine!!!");
                goto exit;
            }
            data = (uint8_t *)ipConfig;
            dataSize = sizeof(param_id_asr_input_threshold_t);
            sesParamId = PAL_PARAM_ID_ASR_SET_PARAM;
            break;
        }
        case SDZ_INPUT_BUF_DURATION: {
            param_id_sdz_input_threshold_t *ipConfigSdz = sAsr->GetSdzInputBufferConfig();
            if (ipConfigSdz == nullptr) {
                PAL_ERR(LOG_TAG, "No input config available for Sdz!!!");
                goto exit;
            }
            data = (uint8_t *)ipConfigSdz;
            dataSize = sizeof(param_id_sdz_input_threshold_t);
            sesParamId = PAL_PARAM_ID_SDZ_SET_PARAM;
            break;
        }
        case SDZ_ENABLE : {
            param_id_module_enable_t param;
            param.enable = 1;
            data = (uint8_t *)&param;
            dataSize = sizeof(param_id_module_enable_t);
            sesParamId = PAL_PARAM_ID_SDZ_ENABLE;
            break;
        }
        default : {
            PAL_ERR(LOG_TAG, "Unexpected param ID is sent, not implemented yet");
        }
    }

    status = builder->payloadConfig(&payload, &payloadSize, data, dataSize,
                                        miid, paramId);
    if (status || !payload) {
        PAL_ERR(LOG_TAG, "Failed to construct ASR payload, status = %d",
            status);
        return -ENOMEM;
    }

    status = dynamic_cast<SessionAR*>(session)->setParamWithTag(streamHandle, tagId,
                                                                sesParamId, payload);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to set payload for param id %x, status = %d",
            sesParamId, status);
    }

exit:
    if (data != NULL && (pid == ASR_FORCE_OUTPUT || pid == SDZ_FORCE_OUTPUT)) {
        free(data);
        data = nullptr;
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t ASREngine::StartEngine(Stream *s)
{
    PAL_DBG(LOG_TAG, "Enter");

    int32_t status = 0;
    int32_t tempStatus = 0;
    uint8_t *eventPayload = NULL;
    size_t eventPayloadSize = sizeof(struct event_id_asr_output_reg_cfg_t);
    struct event_id_asr_output_reg_cfg_t *eventConfig =  NULL;
    size_t eventPayloadSizeSdz = sizeof(struct event_id_sdz_output_reg_cfg_t);
    struct event_id_sdz_output_reg_cfg_t *eventConfigSdz = NULL;
    StreamASR *sAsr = nullptr;

    std::lock_guard<std::mutex> lck(mutexEngine);

    eventPayload = (uint8_t *)calloc(1, eventPayloadSize);
    if (eventPayload == NULL)
        goto exit;

    eventConfig = (struct event_id_asr_output_reg_cfg_t *)eventPayload;
    eventConfig->event_payload_type = 0;

    status = session->open(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:%d Failed to open session", status);
        goto exit;
    }
    dynamic_cast<SessionAR*>(session)->setEventPayload(
        EVENT_ID_ASR_OUTPUT, (void *)eventPayload, eventPayloadSize);

    status = setParameters(s, ASR_INPUT_CONFIG);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to set engine config, can't start the engine!!!");
        goto err_cleanup;
    }

    status = setParameters(s, ASR_INPUT_BUF_DURATON);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to set input config, can't start the engine!!!");
        goto err_cleanup;
    }

    status = setParameters(s, ASR_OUTPUT_CONFIG);
    if (status) {
        PAL_ERR(LOG_TAG, "Failed to set output config, can't start the engine!!!");
        goto err_cleanup;
    }

    sAsr = dynamic_cast<StreamASR *>(s);
    if (sAsr->EnableSpeakerDiarization()) {
        status = setParameters(s, SDZ_ENABLE);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to enable SDZ module, can't start the engine!!!");
            goto err_cleanup;
        }

        eventPayload = (uint8_t *)calloc(1, eventPayloadSizeSdz);
        if (eventPayload == NULL)
            goto err_cleanup;

        eventConfigSdz = (struct event_id_sdz_output_reg_cfg_t *)eventPayload;
        eventConfigSdz->event_payload_type = 0;

        dynamic_cast<SessionAR*>(session)->setEventPayload(
               EVENT_ID_SDZ_OUTPUT, (void *)eventPayload, eventPayloadSizeSdz);

        status = setParameters(s, SDZ_INPUT_BUF_DURATION);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to set input config, can't start the engine!!!");
            goto err_cleanup;
        }

        status = setParameters(s, SDZ_OUTPUT_CONFIG);
        if (status) {
            PAL_ERR(LOG_TAG, "Failed to set output config, can't start the engine!!!");
            goto err_cleanup;
        }
    }

    status = session->prepare(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:%d Failed to prepare session", status);
        goto err_cleanup;
    }

    status = session->start(s);
    if (0 != status) {
        PAL_ERR(LOG_TAG, "Error:%d Failed to start session", status);
        goto err_cleanup;
    }

    engState = ASR_ENG_ACTIVE;
    goto exit;

err_cleanup:
    tempStatus = session->close(s);
    if (tempStatus)
        PAL_ERR(LOG_TAG, "Error: %d Failed to close session", tempStatus);

exit:
    if (eventConfig) {
        free(eventConfig);
        eventConfig = nullptr;
    }

    if (eventConfigSdz) {
        free(eventConfigSdz);
        eventConfigSdz = nullptr;
    }

    PAL_DBG(LOG_TAG, "Exit, status %d", status);
    return status;
}

int32_t ASREngine::StopEngine(Stream *s)
{
    int32_t status = 0;

    PAL_DBG(LOG_TAG, "Enter");

    status = session->stop(s);
    if (status) {
        PAL_ERR(LOG_TAG, "Error:%d Failed to stop session", status);
    }

    status = session->close(s);
    if (status)
        PAL_ERR(LOG_TAG, "Error: %d Failed to close session", status);

    engState = ASR_ENG_IDLE;
    loggerModeEnabled = false;
    timestampEnabled = false;
exit:
    PAL_DBG(LOG_TAG, "Exit, status = %d", status);
    return status;
}

void ASREngine::ParseEventAndNotifyStream(void* eventData) {

    PAL_DBG(LOG_TAG, "Enter.");

    int32_t status = 0;
    int32_t eventStatus = 0;
    bool abortEvent = false;
    size_t eventSize = 0;
    void *payload = nullptr;
    uint8_t *temp = nullptr;
    event_id_asr_output_event_t *event = nullptr;
    asr_output_status_t *ev = nullptr;
    param_id_asr_output_t *eventHeader = nullptr;
    eventPayload eventToStream {};
    StreamASR *sAsr = nullptr;

    event = (struct event_id_asr_output_event_t *)eventData;
    if (event == nullptr) {
        PAL_ERR(LOG_TAG, "Invalid event!!!");
        goto exit;
    }

    abortEvent = event->event_payload_type == ASR_ABORT_TYPE;

    PAL_INFO(LOG_TAG, "Logger mode : %d, Output mode : %d, output token : %d, num output : %d, payload size : %d",
            loggerModeEnabled, event->event_payload_type, event->output_token, event->num_outputs, event->payload_size);

    if (event->num_outputs == 0) {
        PAL_ERR(LOG_TAG, "event raised without any transcript");
        goto cleanup;
    }

    /**
     * Don't move following variable updates after the getParam, as these variables
     * will be used by payload builder while handling the getParam.
     */
    numOutput = event->num_outputs;
    outputToken = event->output_token;
    payloadSize = event->payload_size;

    if (!abortEvent) {
        status = dynamic_cast<SessionAR*>(session)->getParamWithTag(streamHandle,
                           moduleTagIds[ASR_OUTPUT], PAL_PARAM_ID_ASR_OUTPUT,
                           &payload);
        if (status != 0) {
            PAL_ERR(LOG_TAG, "Failed to get output payload");
            goto cleanup;
        }
    } else {
        payload = calloc(1, sizeof(struct param_id_asr_output_t));
        if (!payload) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for payload");
            goto cleanup;
        }
    }

    numOutput = 0;
    outputToken = 0;
    payloadSize = 0;

    temp = (uint8_t *)payload;
    eventHeader = (struct param_id_asr_output_t *)temp;
    if (timestampEnabled) {
        PAL_INFO(LOG_TAG, "Timestamp based event recieved");
        asr_output_status_v2_t *ev = (asr_output_status_v2_t *)(temp + sizeof(struct param_id_asr_output_t));
        pal_asr_ts_event *eventPayload = nullptr;
        asr_word_status_t *words = nullptr;

        eventToStream.type = TIMESTAMP_BASED_TEXT;
        eventToStream.payloadSize = sizeof(pal_asr_ts_event) + eventHeader->num_outputs *
                                    sizeof(pal_asr_engine_ts_event);
        eventToStream.payload = calloc(1, eventToStream.payloadSize);
        if (eventToStream.payload == nullptr) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for stream event!!!");
            goto cleanup;
        }
        eventPayload = (pal_asr_ts_event *)eventToStream.payload;
        eventPayload->num_events = eventHeader->num_outputs;
        eventPayload->status = abortEvent ? PAL_ASR_EVENT_STATUS_ABORTED :
                                            PAL_ASR_EVENT_STATUS_SUCCESS;

        for (int i = 0; i < eventHeader->num_outputs; i++) {
            if (ev->status == ASR_FAIL) {
                PAL_INFO(LOG_TAG, "Recieved failure event, ignoring this event!!!");
                goto cleanup;
            } else if (ev->num_words >= MAX_NUM_WORDS) {
                PAL_INFO(LOG_TAG, "Recieved event, with more words %d than allowed %d!!!",
                         ev->num_words, MAX_NUM_WORDS);
                goto cleanup;
            }

            eventPayload->status = ev->status == ASR_TIMEOUT ? PAL_ASR_EVENT_STATUS_TIMEOUT :
                                                        PAL_ASR_EVENT_STATUS_SUCCESS;
            eventPayload->event[i].is_final = ev->is_final;
            eventPayload->event[i].confidence = ev->confidence;
            eventPayload->event[i].text_size = ev->text_size < 0 ? 0 : ev->text_size;
            eventPayload->event[i].start_ts = ((uint64_t)ev->segment_start_time_ms_msw << 32 |
                                               (uint64_t)ev->segment_start_time_ms_lsw);
            eventPayload->event[i].end_ts = ((uint64_t)ev->segment_end_time_ms_msw << 32 |
                                             (uint64_t)ev->segment_end_time_ms_lsw);
            eventPayload->event[i].num_words = ev->num_words;
            words = (asr_word_status_t *)(((uint8_t *)ev) + sizeof(asr_output_status_v2_t));
            for (int j = 0; j < eventPayload->event[i].text_size; ++j)
                eventPayload->event[i].text[j] = ev->text[j];
            for (int j = 0; j < ev->num_words; j++) {
                eventPayload->event[i].word[j].word_confidence = words[j].word_confidence;
                eventPayload->event[i].word[j].start_ts = ((uint64_t)words[j].word_start_time_ms_msw << 32 |
                                                          (uint64_t)words[j].word_start_time_ms_lsw);
                eventPayload->event[i].word[j].end_ts = ((uint64_t)words[j].word_end_time_ms_msw << 32 |
                                                        (uint64_t)words[j].word_end_time_ms_lsw);
                for (int k = 0; k < words[j].word_size; k++)
                    eventPayload->event[i].word[j].word[k] = words[j].word[k];
            }
            if (i < eventHeader->num_outputs - 1)
                ev = (asr_output_status_v2_t *)((uint8_t *)ev + sizeof(asr_output_status_v2_t) +
                                                 ev->num_words * sizeof(asr_word_status_t));
        }
    } else {
        asr_output_status_t *ev = (asr_output_status_t *)(temp + sizeof(struct param_id_asr_output_t));
        pal_asr_event *eventPayload = nullptr;

        eventToStream.type = PLAIN_TEXT;
        eventToStream.payloadSize = sizeof(pal_asr_event) + eventHeader->num_outputs * sizeof(pal_asr_engine_event);

        eventToStream.payload = calloc(1, eventToStream.payloadSize);
        if (eventToStream.payload == nullptr) {
            PAL_ERR(LOG_TAG, "Failed to allocate memory for stream event!!!");
            return;
        }
        eventPayload = (pal_asr_event *)eventToStream.payload;
        eventPayload->num_events = eventHeader->num_outputs;
        eventPayload->status = abortEvent ? PAL_ASR_EVENT_STATUS_ABORTED :
                                            PAL_ASR_EVENT_STATUS_SUCCESS;

        for (int i = 0; i < eventHeader->num_outputs; i++) {
            if (ev[i].status == ASR_FAIL) {
                PAL_INFO(LOG_TAG, "Recieved failure event, ignoring this event!!!");
                goto cleanup;
            }
            eventPayload->status = ev[i].status == ASR_TIMEOUT ? PAL_ASR_EVENT_STATUS_TIMEOUT :
                                                        PAL_ASR_EVENT_STATUS_SUCCESS;
            eventPayload->event[i].is_final = ev[i].is_final;
            eventPayload->event[i].confidence = ev[i].confidence;
            eventPayload->event[i].text_size = ev[i].text_size < 0 ? 0 : ev[i].text_size;
            for (int j = 0; j < ev[i].text_size; ++j)
                eventPayload->event[i].text[j] = ev[i].text[j];
        }
    }


    sAsr = dynamic_cast<StreamASR *>(streamHandle);
    sAsr->HandleEventData(eventToStream);

cleanup:
    if (eventToStream.payload) {
        free(eventToStream.payload);
        eventToStream.payload = nullptr;
    }

    if (payload) {
        free(payload);
        payload = nullptr;
    }

    if (eventData) {
        event = nullptr;
        free(eventData);
        eventData = nullptr;
    }

exit:
    PAL_DBG(LOG_TAG, "Exit.");
}

void ASREngine::ParseSdzEventAndNotifyStream(void* eventData) {

    PAL_DBG(LOG_TAG, "Enter.");

    int32_t status = 0;
    bool eventStatus = false;
    bool overlapDetected = true;
    void *payload = nullptr;
    uint8_t *temp = nullptr;
    size_t eventSize = 0;
    uint32_t numSpeakers = 0;
    std::vector<std::vector<struct sdz_speaker_info>> sdzOutputVector;
    std::vector<std::pair<bool, uint32_t>> sdzOverlapNumSpeakerVector;
    event_id_sdz_output_event_t *event = nullptr;
    sdz_output_status_t *ev = nullptr;
    param_id_sdz_output_t *eventHeader = nullptr;
    sdz_speaker_info_t *eventSpeakerInfo = nullptr;
    pal_sdz_event *sdzEventPayload = nullptr;
    eventPayload eventToStream;
    StreamASR *sAsr = nullptr;

    event = (event_id_sdz_output_event_t *)eventData;
    if (event == nullptr) {
        PAL_ERR(LOG_TAG, "Invalid event!!!");
        goto exit;
    }

    PAL_INFO(LOG_TAG, "Output mode : %d, output token : %d, num output : %d, payload size : %d",
        event->event_payload_type, event->output_token, event->num_outputs, event->payload_size);

    if (event->num_outputs == 0) {
        PAL_ERR(LOG_TAG, "event raised without any speaker info");
        goto cleanup;
    }

    sdzNumOutput = event->num_outputs;
    sdzOutputToken = event->output_token;
    sdzPayloadSize = sizeof(param_id_sdz_output_t) + event->payload_size;

    status = dynamic_cast<SessionAR*>(session)->getParamWithTag(streamHandle,
                           moduleTagIds[SDZ_OUTPUT], PAL_PARAM_ID_SDZ_OUTPUT,
                           &payload);
    if (status != 0) {
        PAL_ERR(LOG_TAG, "Failed to get output payload");
        goto cleanup;
    }

    eventHeader = (param_id_sdz_output_t *)payload;

    ev = (sdz_output_status_t *)((uint8_t *)payload + sizeof(param_id_sdz_output_t));
    for (int i = 0; i < eventHeader->num_outputs; i++) {
        std::vector<struct sdz_speaker_info> speakerInfoVector;
        eventSpeakerInfo = (sdz_speaker_info_t *)((uint8_t *)ev + sizeof(sdz_output_status_t));
        numSpeakers += ev->num_speakers;
        for (int j = 0; j < ev->num_speakers; j++) {
            struct sdz_speaker_info speakerInfo;
            speakerInfo.speaker_id = eventSpeakerInfo->speaker_id;
            speakerInfo.start_ts = ((uint64_t)eventSpeakerInfo->start_ts_msw << 32 |
                                     (uint64_t)eventSpeakerInfo->start_ts_lsw);
            speakerInfo.end_ts = ((uint64_t)eventSpeakerInfo->end_ts_msw << 32 |
                                     (uint64_t)eventSpeakerInfo->end_ts_lsw);
            speakerInfoVector.push_back(speakerInfo);
            eventSpeakerInfo++;
            temp = (uint8_t *)eventSpeakerInfo;
        }
        sdzOutputVector.push_back(speakerInfoVector);
        sdzOverlapNumSpeakerVector.push_back({ev->overlap_detected, ev->num_speakers});
        ev = (sdz_output_status_t *)temp;
    }

    eventToStream.type = SPEAKER_DIARIZATION;
    eventToStream.payloadSize = sizeof(pal_sdz_event) +
                                sdzOverlapNumSpeakerVector.size() * sizeof(struct sdz_output) +
                                numSpeakers * sizeof(sdz_speaker_info);
    eventToStream.payload = (void *)calloc(1, eventToStream.payloadSize);
    if (eventToStream.payload == nullptr) {
        PAL_ERR(LOG_TAG, "Failed to allocate memory for stream event!!");
        goto cleanup;
    }

    sdzEventPayload = (pal_sdz_event *)eventToStream.payload;
    sdzEventPayload->num_outputs = sdzOutputVector.size();
    for (int i = 0; i < sdzEventPayload->num_outputs; i++) {
        sdzEventPayload->output[i].overlap_detected = sdzOverlapNumSpeakerVector[i].first;
        sdzEventPayload->output[i].num_speakers = sdzOverlapNumSpeakerVector[i].second;

        for (int j = 0; j < sdzEventPayload->output[i].num_speakers; j++) {
            sdzEventPayload->output[i].speakers_list[j].speaker_id = sdzOutputVector[i][j].speaker_id;
            sdzEventPayload->output[i].speakers_list[j].start_ts = sdzOutputVector[i][j].start_ts;
            sdzEventPayload->output[i].speakers_list[j].end_ts = sdzOutputVector[i][j].end_ts;
        }
    }
    sdzOutputVector.clear();
    sdzOverlapNumSpeakerVector.clear();

    PAL_INFO(LOG_TAG, "Total number of speakers : %d",numSpeakers)

    sAsr =  dynamic_cast<StreamASR *>(streamHandle);
    sAsr->HandleEventData(eventToStream);

cleanup:
    if (eventToStream.payload) {
        free(eventToStream.payload);
        eventToStream.payload = nullptr;
    }
    if (payload) {
        free(payload);
        payload = nullptr;
    }
    if (eventData) {
        free(eventData);
        event = nullptr;
        eventData = nullptr;
    }
exit:
    PAL_DBG(LOG_TAG, "Exit.");
}

void ASREngine::EventProcessingThread(ASREngine *engine)
{
    PAL_INFO(LOG_TAG, "Enter. start thread loop");
    if (!engine) {
        PAL_ERR(LOG_TAG, "Error:%d Invalid engine", -EINVAL);
        return;
    }
    std::pair<uint32_t, void *> event;
    std::unique_lock<std::mutex> lck(engine->mutexEngine);
    while (!engine->exitThread) {
        while (engine->eventQ.empty()) {
            PAL_DBG(LOG_TAG, "waiting on cond");
            engine->cv.wait(lck);
            PAL_DBG(LOG_TAG, "done waiting on cond");

            if (engine->exitThread) {
                PAL_VERBOSE(LOG_TAG, "Exit thread");
                break;
            }
        }
        //Adding this condition, as destructor can also notify this thread without any event
        if (!engine->eventQ.empty()) {
            event = engine->eventQ.front();
            engine->eventQ.pop();
            if (event.first == EVENT_ID_SDZ_OUTPUT) {
                engine->ParseSdzEventAndNotifyStream(event.second);
            } else {
                engine->ParseEventAndNotifyStream(event.second);
            }
        }
    }

    PAL_INFO(LOG_TAG, "Exit");
}

void ASREngine::HandleSessionEvent(uint32_t eventId,
                                   void *data, uint32_t size)
{
    void *eventData = nullptr;

    std::unique_lock<std::mutex> lck(mutexEngine);

    if (engState == ASR_ENG_IDLE) {
        PAL_INFO(LOG_TAG, "Engine not active, ignore");
        lck.unlock();
        return;
    }

    eventData = calloc(1, size);
    if (!eventData) {
        PAL_ERR(LOG_TAG, "Error:failed to allocate mem for event_data");
        return;
    }

    memcpy(eventData, data, size);
    eventQ.push({eventId, eventData});
    cv.notify_one();
}

void ASREngine::HandleSessionCallBack(uint64_t hdl, uint32_t eventId,
                                      void *data, uint32_t eventSize)
{
    ASREngine *engine = nullptr;

    PAL_INFO(LOG_TAG, "Enter, event detected on SPF, event id = 0x%x", eventId);
    if ((hdl == 0) || !data) {
        PAL_ERR(LOG_TAG, "Error:%d Invalid engine handle or event data", -EINVAL);
        return;
    }

    if (eventId != EVENT_ID_ASR_OUTPUT && eventId != EVENT_ID_SDZ_OUTPUT)
        return;

    engine = (ASREngine *)hdl;
    engine->HandleSessionEvent(eventId, data, eventSize);

    PAL_DBG(LOG_TAG, "Exit");
    return;
}

int32_t ASREngine::setECRef(Stream *s, std::shared_ptr<Device> dev, bool isEnable,
                                        bool setECForFirstTime) {
    int32_t status = 0;
    bool forceEnable = false;
    bool isDevEnabledExtEc = false;

    if (!session) {
        PAL_ERR(LOG_TAG, "Invalid session");
        return -EINVAL;
    }
    PAL_DBG(LOG_TAG, "Enter, EC ref count : %d, enable : %d", ecRefCount, isEnable);
    PAL_DBG(LOG_TAG, "Rx device : %s, stream is setting EC for first time : %d",
            dev ? dev->getPALDeviceName().c_str() :  "Null", setECForFirstTime);

    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();
    if (!rm) {
        PAL_ERR(LOG_TAG, "Failed to get resource manager instance");
        return -EINVAL;
    }

    if (dev)
        isDevEnabledExtEc = rm->isExternalECRefEnabled(dev->getSndDeviceId());
    std::unique_lock<std::recursive_mutex> lck(ecRefMutex);
    if (isEnable) {
        if (isCrrDevUsingExtEc && !isDevEnabledExtEc) {
            PAL_ERR(LOG_TAG, "Internal EC connot be set, when external EC is active");
            return -EINVAL;
        }
        if (setECForFirstTime) {
            ecRefCount++;
        } else if (rxEcDev != dev ){
            forceEnable = true;
        } else {
            return status;
        }
        if (forceEnable || ecRefCount == 1) {
            status = session->setECRef(s, dev, isEnable);
            if (status) {
                PAL_ERR(LOG_TAG, "Failed to set EC Ref for rx device %s",
                        dev ?  dev->getPALDeviceName().c_str() : "Null");
                if (setECForFirstTime) {
                    ecRefCount--;
                }
                if (forceEnable || ecRefCount == 0) {
                    rxEcDev = nullptr;
                }
            } else {
                isCrrDevUsingExtEc = isDevEnabledExtEc;
                rxEcDev = dev;
            }
        }
    } else {
        if (!dev || dev == rxEcDev) {
            if (ecRefCount > 0) {
                ecRefCount--;
                if (ecRefCount == 0) {
                    status = session->setECRef(s, dev, isEnable);
                    if (status) {
                        PAL_ERR(LOG_TAG, "Failed to reset EC Ref");
                    } else {
                        rxEcDev = nullptr;
                        isCrrDevUsingExtEc = false;
                    }
                }
            } else {
                PAL_DBG(LOG_TAG, "Skipping EC disable, as ref count is 0");
            }
        } else {
            PAL_DBG(LOG_TAG, "Skipping EC disable, as EC disable is not for correct device");
        }
    }
    PAL_DBG(LOG_TAG, "Exit, EC ref count : %d", ecRefCount);

    return status;
}

int32_t ASREngine::ConnectSessionDevice(
    Stream* streamHandle, pal_stream_type_t streamType,
    std::shared_ptr<Device> deviceToConnect)
{
    PAL_DBG(LOG_TAG, "Enter, devDisconnectCount: %d", devDisconnectCount);
    int32_t status = 0;

    if (!session) {
        PAL_ERR(LOG_TAG, "Invalid session");
        return -EINVAL;
    }

    if (devDisconnectCount == 0)
        status = session->connectSessionDevice(streamHandle, streamType,
                                            deviceToConnect);
    if (status != 0)
        devDisconnectCount++;

    PAL_DBG(LOG_TAG, "Exit, devDisconnectCount: %d", devDisconnectCount);
    return status;
}

int32_t ASREngine::DisconnectSessionDevice(
    Stream* streamHandle, pal_stream_type_t streamType,
    std::shared_ptr<Device> deviceToDisconnect)
{
    PAL_DBG(LOG_TAG, "Enter, devDisconnectCount: %d", devDisconnectCount);
    int32_t status = 0;

    if (!session) {
        PAL_ERR(LOG_TAG, "Invalid session");
        return -EINVAL;
    }

    devDisconnectCount++;
    status = session->disconnectSessionDevice(streamHandle, streamType,
                                               deviceToDisconnect);
    if (status != 0)
        devDisconnectCount--;
    PAL_DBG(LOG_TAG, "Exit, devDisconnectCount: %d", devDisconnectCount);
    return status;
}

int32_t ASREngine::SetupSessionDevice(
    Stream* streamHandle, pal_stream_type_t streamType,
    std::shared_ptr<Device> deviceToDisconnect)
{
    PAL_DBG(LOG_TAG, "Enter, devDisconnectCount: %d", devDisconnectCount);
    int32_t status = 0;

    if (!session) {
        PAL_ERR(LOG_TAG, "Invalid session");
        return -EINVAL;
    }

    devDisconnectCount--;
    if (devDisconnectCount < 0)
        devDisconnectCount = 0;

    if (devDisconnectCount == 0)
        status = session->setupSessionDevice(streamHandle, streamType,
                                          deviceToDisconnect);
    if (status != 0)
        devDisconnectCount++;

    PAL_DBG(LOG_TAG, "Enter, devDisconnectCount: %d", devDisconnectCount);
    return status;
}
