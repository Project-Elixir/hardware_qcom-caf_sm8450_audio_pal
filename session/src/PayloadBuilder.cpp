/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
 */

#define LOG_TAG "QAL: PayloadBuilder"
#include "ResourceManager.h"
#include "PayloadBuilder.h"
#include "SessionGsl.h"
#include "StreamSoundTrigger.h"
#include "plugins/codecs/bt_intf.h"
#include "spr_api.h"
#include <bt_intf.h>
#include "sp_vi.h"

#define QAL_ALIGN_8BYTE(x) (((x) + 7) & (~7))
#define QAL_PADDING_8BYTE_ALIGN(x)  ((((x) + 7) & 7) ^ 7)
#define XML_FILE "/vendor/etc/hw_ep_info.xml"
#define PARAM_ID_DISPLAY_PORT_INTF_CFG   0x8001154

#define PARAM_ID_USB_AUDIO_INTF_CFG                               0x080010D6

/* ID of the Output Media Format parameters used by MODULE_ID_MFC */
#define PARAM_ID_MFC_OUTPUT_MEDIA_FORMAT            0x08001024
#include "spf_begin_pack.h"
#include "spf_begin_pragma.h"
/* Payload of the PARAM_ID_MFC_OUTPUT_MEDIA_FORMAT parameter in the
 Media Format Converter Module. Following this will be the variable payload for channel_map. */

struct param_id_mfc_output_media_fmt_t
{
   int32_t sampling_rate;
   /**< @h2xmle_description  {Sampling rate in samples per second\n
                              ->If the resampler type in the MFC is chosen to be IIR,
                              ONLY the following sample rates are ALLOWED:
                              PARAM_VAL_NATIVE =-1;\n
                              PARAM_VAL_UNSET = -2;\n
                              8 kHz = 8000;\n
                              16kHz = 16000;\n
                              24kHz = 24000;\n
                              32kHz = 32000;\n
                              48kHz = 48000 \n
                              -> For resampler type FIR, all values in the range
                              below are allowed}
        @h2xmle_rangeList   {"PARAM_VAL_UNSET" = -2;
                             "PARAM_VAL_NATIVE" =-1;
                             "8 kHz"=8000;
                             "11.025 kHz"=11025;
                             "12 kHz"=12000;
                             "16 kHz"=16000;
                             "22.05 kHz"=22050;
                             "24 kHz"=24000;
                             "32 kHz"=32000;
                             "44.1 kHz"=44100;
                             "48 kHz"=48000;
                             "88.2 kHz"=88200;
                             "96 kHz"=96000;
                             "176.4 kHz"=176400;
                             "192 kHz"=192000;
                             "352.8 kHz"=352800;
                             "384 kHz"=384000}
        @h2xmle_default      {-1} */

   int16_t bit_width;
   /**< @h2xmle_description  {Bit width of audio samples \n
                              ->Samples with bit width of 16 (Q15 format) are stored in 16 bit words \n
                              ->Samples with bit width 24 bits (Q27 format) or 32 bits (Q31 format) are stored in 32 bit words}
        @h2xmle_rangeList    {"PARAM_VAL_NATIVE"=-1;
                              "PARAM_VAL_UNSET"=-2;
                              "16-bit"= 16;
                              "24-bit"= 24;
                              "32-bit"=32}
        @h2xmle_default      {-1}
   */

   int16_t num_channels;
   /**< @h2xmle_description  {Number of channels. \n
                              ->Ranges from -2 to 32 channels where \n
                              -2 is PARAM_VAL_UNSET and -1 is PARAM_VAL_NATIVE}
        @h2xmle_range        {-2..32}
        @h2xmle_default      {-1}
   */

   uint16_t channel_type[0];
   /**< @h2xmle_description  {Channel mapping array. \n
                              ->Specify a channel mapping for each output channel \n
                              ->If the number of channels is not a multiple of four, zero padding must be added
                              to the channel type array to align the packet to a multiple of 32 bits. \n
                              -> If num_channels field is set to PARAM_VAL_NATIVE (-1) or PARAM_VAL_UNSET(-2)
                              this field will be ignored}
        @h2xmle_variableArraySize {num_channels}
        @h2xmle_range        {1..63}
        @h2xmle_default      {1}    */
}
#include "spf_end_pragma.h"
#include "spf_end_pack.h"
;
/* Structure type def for above payload. */
typedef struct param_id_mfc_output_media_fmt_t param_id_mfc_output_media_fmt_t;

struct param_id_usb_audio_intf_cfg_t
{
   uint32_t usb_token;
   uint32_t svc_interval;
};

std::vector<std::pair<uint32_t, uint32_t>> VSIDtoKV {
    /*for now map everything to default */
    { VOICEMMODE1,   0},
    { VOICEMMODE2,   0},
    { VOICELBMMODE1, 0},
    { VOICELBMMODE2, 0},
};

template <typename T>
void PayloadBuilder::populateChannelMap(T pcmChannel, uint8_t numChannel)
{
    if (numChannel == 1) {
        pcmChannel[0] = PCM_CHANNEL_C;
    } else if (numChannel == 2) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
    } else if (numChannel == 3) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_C;
    } else if (numChannel == 4) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_LB;
        pcmChannel[3] = PCM_CHANNEL_RB;
    } else if (numChannel == 5) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_C;
        pcmChannel[3] = PCM_CHANNEL_LB;
        pcmChannel[4] = PCM_CHANNEL_RB;
    } else if (numChannel == 6) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_C;
        pcmChannel[3] = PCM_CHANNEL_LFE;
        pcmChannel[4] = PCM_CHANNEL_LB;
        pcmChannel[5] = PCM_CHANNEL_RB;
    } else if (numChannel == 7) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_C;
        pcmChannel[3] = PCM_CHANNEL_LS;
        pcmChannel[4] = PCM_CHANNEL_RS;
        pcmChannel[5] = PCM_CHANNEL_LB;
        pcmChannel[6] = PCM_CHANNEL_RB;
    } else if (numChannel == 8) {
        pcmChannel[0] = PCM_CHANNEL_L;
        pcmChannel[1] = PCM_CHANNEL_R;
        pcmChannel[2] = PCM_CHANNEL_C;
        pcmChannel[3] = PCM_CHANNEL_LS;
        pcmChannel[4] = PCM_CHANNEL_RS;
        pcmChannel[5] = PCM_CHANNEL_CS;
        pcmChannel[6] = PCM_CHANNEL_LB;
        pcmChannel[7] = PCM_CHANNEL_RB;
    }
}

void PayloadBuilder::payloadUsbAudioConfig(uint8_t** payload, size_t* size,
    uint32_t miid, struct usbAudioConfig *data)
{
    struct apm_module_param_data_t* header;
    struct param_id_usb_audio_intf_cfg_t *usbConfig;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0;

    payloadSize = sizeof(struct apm_module_param_data_t) +
       sizeof(struct param_id_usb_audio_intf_cfg_t);


    if (payloadSize % 8 != 0)
        payloadSize = payloadSize + (8 - payloadSize % 8);

    payloadInfo = (uint8_t*)malloc((size_t)payloadSize);

    header = (struct apm_module_param_data_t*)payloadInfo;
    usbConfig = (struct param_id_usb_audio_intf_cfg_t*)(payloadInfo + sizeof(struct apm_module_param_data_t));
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_USB_AUDIO_INTF_CFG;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_ERR(LOG_TAG,"%s: header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                     __func__, header->module_instance_id, header->param_id,
                     header->error_code, header->param_size);

    usbConfig->usb_token = data->usb_token;
    usbConfig->svc_interval = data->svc_interval;
    QAL_VERBOSE(LOG_TAG,"customPayload address %p and size %d", payloadInfo, payloadSize);

    *size = payloadSize;
    *payload = payloadInfo;

}

void PayloadBuilder::payloadDpAudioConfig(uint8_t** payload, size_t* size,
    uint32_t miid, struct dpAudioConfig *data)
{
    QAL_DBG(LOG_TAG, "%s Enter:", __func__);
    struct apm_module_param_data_t* header;
    struct dpAudioConfig *dpConfig;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0;

    payloadSize = sizeof(struct apm_module_param_data_t) +
        sizeof(struct dpAudioConfig);

    if (payloadSize % 8 != 0)
        payloadSize = payloadSize + (8 - payloadSize % 8);

    payloadInfo = (uint8_t*)malloc((size_t)payloadSize);

    header = (struct apm_module_param_data_t*)payloadInfo;
    dpConfig = (struct dpAudioConfig*)(payloadInfo + sizeof(struct apm_module_param_data_t));
    header->module_instance_id = miid;
    header->param_id = PARAM_ID_DISPLAY_PORT_INTF_CFG;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_ERR(LOG_TAG,"%s: header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                      __func__, header->module_instance_id, header->param_id,
                      header->error_code, header->param_size);

    dpConfig->channel_allocation = data->channel_allocation;
    dpConfig->mst_idx = data->mst_idx;
    dpConfig->dptx_idx = data->dptx_idx;
    QAL_ERR(LOG_TAG,"customPayload address %p and size %d", payloadInfo, payloadSize);

    *size = payloadSize;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "%s Exit:", __func__);
}

void PayloadBuilder::payloadMFCConfig(uint8_t** payload, size_t* size,
        uint32_t miid, struct sessionToPayloadParam* data)
{
    struct apm_module_param_data_t* header = NULL;
    struct param_id_mfc_output_media_fmt_t *mfcConf;
    int numChannels = data->numChannel;
    uint16_t* pcmChannel = NULL;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    if (!data) {
        QAL_ERR(LOG_TAG, "Invalid input parameters");
        return;
    }
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct param_id_mfc_output_media_fmt_t) +
                  sizeof(uint16_t)*numChannels;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = (uint8_t*) calloc(1, payloadSize + padBytes);
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    mfcConf = (struct param_id_mfc_output_media_fmt_t*)(payloadInfo +
               sizeof(struct apm_module_param_data_t));
    pcmChannel = (uint16_t*)(payloadInfo + sizeof(struct apm_module_param_data_t) +
                                       sizeof(struct param_id_mfc_output_media_fmt_t));

    header->module_instance_id = miid;
    header->param_id = PARAM_ID_MFC_OUTPUT_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_DBG(LOG_TAG, "header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                      header->module_instance_id, header->param_id,
                      header->error_code, header->param_size);

    mfcConf->sampling_rate = data->sampleRate;
    mfcConf->bit_width = data->bitWidth;
    mfcConf->num_channels = data->numChannel;
    if (data->ch_info) {
        for (int i = 0; i < data->numChannel; ++i) {
            pcmChannel[i] = (uint16_t) data->ch_info->ch_map[i];
        }
    } else {
        populateChannelMap(pcmChannel, data->numChannel);
    }

    if ((2 == data->numChannel) && (QAL_SPEAKER_ROTATION_RL == data->rotation_type))
    {
        // Swapping the channel
        pcmChannel[0] = PCM_CHANNEL_R;
        pcmChannel[1] = PCM_CHANNEL_L;
    }

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "sample_rate:%d bit_width:%d num_channels:%d Miid:%d",
                      mfcConf->sampling_rate, mfcConf->bit_width,
                      mfcConf->num_channels, header->module_instance_id);
    QAL_DBG(LOG_TAG, "customPayload address %pK and size %d", payloadInfo,
                *size);
}

PayloadBuilder::PayloadBuilder()
{

}

PayloadBuilder::~PayloadBuilder()
{

}

uint16_t numOfBitsSet(uint32_t lines)
{
    uint16_t numBitsSet = 0;
    while (lines) {
        numBitsSet++;
        lines = lines & (lines - 1);
    }
    return numBitsSet;
}


void PayloadBuilder::startTag(void *userdata __unused, const XML_Char *tag_name __unused,
    const XML_Char **attr __unused)
{
	return;
}

void PayloadBuilder::endTag(void *userdata __unused, const XML_Char *tag_name __unused)
{
    return;
}

int PayloadBuilder::init()
{
    XML_Parser parser;
    FILE *file = NULL;
    int ret = 0;
    int bytes_read;
    void *buf = NULL;

    QAL_DBG(LOG_TAG, "Enter.");
    file = fopen(XML_FILE, "r");
    if (!file) {
        QAL_ERR(LOG_TAG, "Failed to open xml");
        ret = -EINVAL;
        goto done;
    }

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        QAL_ERR(LOG_TAG, "Failed to create XML");
        goto closeFile;
    }

    XML_SetElementHandler(parser, startTag, endTag);

    while (1) {
        buf = XML_GetBuffer(parser, 1024);
        if (buf == NULL) {
            QAL_ERR(LOG_TAG, "XML_Getbuffer failed");
            ret = -EINVAL;
            goto freeParser;
        }

        bytes_read = fread(buf, 1, 1024, file);
        if (bytes_read < 0) {
            QAL_ERR(LOG_TAG, "fread failed");
            ret = -EINVAL;
            goto freeParser;
        }

        if (XML_ParseBuffer(parser, bytes_read, bytes_read == 0) == XML_STATUS_ERROR) {
            QAL_ERR(LOG_TAG, "XML ParseBuffer failed ");
            ret = -EINVAL;
            goto freeParser;
        }
        if (bytes_read == 0)
            break;
    }
    QAL_DBG(LOG_TAG, "Exit.");

freeParser:
    XML_ParserFree(parser);
closeFile:
    fclose(file);
done:
    return ret;
}

void PayloadBuilder::payloadTimestamp(uint8_t **payload, size_t *size, uint32_t moduleId)
{
    size_t payloadSize, padBytes;
    uint8_t *payloadInfo = NULL;
    struct apm_module_param_data_t* header;
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct param_id_spr_session_time_t);
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);
    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_SPR_SESSION_TIME;
    header->error_code = 0x0;
    header->param_size = payloadSize -  sizeof(struct apm_module_param_data_t);
    QAL_VERBOSE(LOG_TAG,"header params IID:%x param_id:%x error_code:%d param_size:%d\n",
                  header->module_instance_id, header->param_id,
                  header->error_code, header->param_size);
    *size = payloadSize + padBytes;;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

int PayloadBuilder::payloadCustomParam(uint8_t **alsaPayload, size_t *size,
            uint32_t *customPayload, uint32_t customPayloadSize,
            uint32_t moduleInstanceId, uint32_t paramId) {
    struct apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t alsaPayloadSize = 0;

    alsaPayloadSize = QAL_ALIGN_8BYTE(sizeof(struct apm_module_param_data_t)
                                        + customPayloadSize);
    payloadInfo = (uint8_t *)calloc(1, (size_t)alsaPayloadSize);
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "failed to allocate memory.");
        return -ENOMEM;
    }

    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleInstanceId;
    header->param_id = paramId;
    header->error_code = 0x0;
    header->param_size = customPayloadSize;
    if (customPayloadSize)
        ar_mem_cpy(payloadInfo + sizeof(struct apm_module_param_data_t),
                         customPayloadSize,
                         customPayload,
                         customPayloadSize);
    *size = alsaPayloadSize;
    *alsaPayload = payloadInfo;

    QAL_DBG(LOG_TAG, "ALSA payload %u size %d", *alsaPayload, *size);

    return 0;
}

void PayloadBuilder::payloadSVASoundModel(uint8_t **payload, size_t *size,
                       uint32_t moduleId, struct qal_st_sound_model *soundModel)
{
    struct apm_module_param_data_t* header;
    uint8_t *phrase_sm;
    uint8_t *sm_data;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    size_t soundModelSize = 0;

    if (!soundModel) {
        QAL_ERR(LOG_TAG, "Invalid soundModel param");
        return;
    }
    soundModelSize = soundModel->data_size;
    payloadSize = sizeof(struct apm_module_param_data_t) + soundModelSize;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);
    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_DETECTION_ENGINE_SOUND_MODEL;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    phrase_sm = (uint8_t *)payloadInfo + sizeof(struct apm_module_param_data_t);
    sm_data = (uint8_t *)soundModel + soundModel->data_offset;
    ar_mem_cpy(phrase_sm, soundModelSize, sm_data, soundModelSize);
    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadSVAWakeUpConfig(uint8_t **payload, size_t *size,
        uint32_t moduleId, struct detection_engine_config_voice_wakeup *pWakeUp)
{
    struct apm_module_param_data_t* header;
    struct detection_engine_config_voice_wakeup *wakeUpConfig;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    uint8_t *confidence_level;
    uint8_t *kw_user_enable;
    uint32_t fixedConfigVoiceWakeupSize = 0;

    if (!pWakeUp) {
        QAL_ERR(LOG_TAG, "Invalid pWakeUp param");
        return;
    }
    fixedConfigVoiceWakeupSize = sizeof(struct detection_engine_config_voice_wakeup) -
                                  QAL_SOUND_TRIGGER_MAX_USERS * 2;

    payloadSize = sizeof(struct apm_module_param_data_t) +
                  fixedConfigVoiceWakeupSize +
                     pWakeUp->num_active_models * 2;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_DETECTION_ENGINE_CONFIG_VOICE_WAKEUP;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    wakeUpConfig = (struct detection_engine_config_voice_wakeup*)
                   (payloadInfo + sizeof(struct apm_module_param_data_t));
    ar_mem_cpy(wakeUpConfig, fixedConfigVoiceWakeupSize, pWakeUp,
                     fixedConfigVoiceWakeupSize);
    confidence_level = (uint8_t*)((uint8_t*)wakeUpConfig + fixedConfigVoiceWakeupSize);
    kw_user_enable = (uint8_t*)((uint8_t*)wakeUpConfig +
                     fixedConfigVoiceWakeupSize +
                     pWakeUp->num_active_models);

    QAL_VERBOSE(LOG_TAG, "mode=%d custom_payload_size=%d", wakeUpConfig->mode,
                wakeUpConfig->custom_payload_size);
    QAL_VERBOSE(LOG_TAG, "num_active_models=%d reserved=%d",
                wakeUpConfig->num_active_models, wakeUpConfig->reserved);

    for (int i = 0; i < pWakeUp->num_active_models; i++) {
        confidence_level[i] = pWakeUp->confidence_levels[i];
        kw_user_enable[i] = pWakeUp->keyword_user_enables[i];
        QAL_VERBOSE(LOG_TAG, "confidence_level[%d] = %d KW_User_enable[%d] = %d",
                                  i, confidence_level[i], i, kw_user_enable[i]);
    }

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadSVAWakeUpBufferConfig(uint8_t **payload, size_t *size,
  uint32_t moduleId, struct detection_engine_voice_wakeup_buffer_config *pWakeUpBufConfig)
{
    struct apm_module_param_data_t* header;
    struct detection_engine_voice_wakeup_buffer_config *pWakeUpBufCfg;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    if (!pWakeUpBufConfig) {
        QAL_ERR(LOG_TAG, "Invalid pWakeUpBufConfig param");
        return;
    }
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct detection_engine_voice_wakeup_buffer_config);
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_VOICE_WAKEUP_BUFFERING_CONFIG;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    pWakeUpBufCfg = (struct detection_engine_voice_wakeup_buffer_config *)
                    (payloadInfo + sizeof(struct apm_module_param_data_t));
    ar_mem_cpy(pWakeUpBufCfg,sizeof(struct detection_engine_voice_wakeup_buffer_config),
                     pWakeUpBufConfig, sizeof(struct detection_engine_voice_wakeup_buffer_config));

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadSVAStreamSetupDuration(uint8_t **payload, size_t *size,
  uint32_t moduleId, struct audio_dam_downstream_setup_duration *pSetupDuration)
{
    struct apm_module_param_data_t* header;
    struct audio_dam_downstream_setup_duration *pDownStreamSetupDuration;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    if (!pSetupDuration) {
        QAL_ERR(LOG_TAG, "Invalid pSetupDuration param");
        return;
    }
    size_t structSize = sizeof(struct audio_dam_downstream_setup_duration) +
                        (pSetupDuration->num_output_ports *
                         sizeof(struct audio_dam_downstream_setup_duration_t));

    payloadSize = sizeof(struct apm_module_param_data_t) + structSize;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_AUDIO_DAM_DOWNSTREAM_SETUP_DURATION;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    pDownStreamSetupDuration = (struct audio_dam_downstream_setup_duration *)
                               (payloadInfo + sizeof(struct apm_module_param_data_t));
    ar_mem_cpy(pDownStreamSetupDuration, structSize, pSetupDuration, structSize);

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadSVAEventConfig(uint8_t **payload, size_t *size,
     uint32_t moduleId, struct detection_engine_generic_event_cfg *pEventConfig)
{
    struct apm_module_param_data_t* header;
    struct detection_engine_generic_event_cfg *pEventCfg;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    if (!pEventConfig) {
        QAL_ERR(LOG_TAG, "Invalid pEventConfig param");
        return;
    }
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct detection_engine_generic_event_cfg);
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_DETECTION_ENGINE_GENERIC_EVENT_CFG;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    pEventCfg = (struct detection_engine_generic_event_cfg *)
                (payloadInfo + sizeof(struct apm_module_param_data_t));
    ar_mem_cpy(pEventCfg, sizeof(struct detection_engine_generic_event_cfg),
                     pEventConfig, sizeof(struct detection_engine_generic_event_cfg));

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadSVAEngineReset(uint8_t **payload, size_t *size,
                                           uint32_t moduleId)
{
    struct apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    payloadSize = sizeof(struct apm_module_param_data_t);
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_DETECTION_ENGINE_RESET;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}


void PayloadBuilder::payloadQuery(uint8_t **payload, size_t *size,
                    uint32_t moduleId, uint32_t paramId, uint32_t querySize)
{
    struct apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    payloadSize = sizeof(struct apm_module_param_data_t) + querySize;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }

    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = paramId;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
}

void PayloadBuilder::payloadDOAInfo(uint8_t **payload, size_t *size, uint32_t moduleId)
{
    struct apm_module_param_data_t* header;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct ffv_doa_tracking_monitor_t);
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = moduleId;
    header->param_id = PARAM_ID_FFV_DOA_TRACKING_MONITOR;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "payload %u size %d", *payload, *size);
}

void PayloadBuilder::payloadTWSConfig(uint8_t** payload, size_t* size,
        uint32_t miid, bool isTwsMonoModeOn, uint32_t codecFormat)
{
    struct apm_module_param_data_t* header = NULL;
    uint8_t* payloadInfo = NULL;
    uint32_t param_id = 0, val = 2;
    size_t payloadSize = 0, customPayloadSize = 0;
    param_id_aptx_classic_switch_enc_pcm_input_payload_t *aptx_classic_payload;
    param_id_aptx_adaptive_enc_switch_to_mono_t *aptx_adaptive_payload;

    if (codecFormat == CODEC_TYPE_APTX_DUAL_MONO) {
        param_id = PARAM_ID_APTX_CLASSIC_SWITCH_ENC_PCM_INPUT;
        customPayloadSize = sizeof(param_id_aptx_classic_switch_enc_pcm_input_payload_t);
    } else {
        param_id = PARAM_ID_APTX_ADAPTIVE_ENC_SWITCH_TO_MONO;
        customPayloadSize = sizeof(param_id_aptx_adaptive_enc_switch_to_mono_t);
    }
    payloadSize = QAL_ALIGN_8BYTE(sizeof(struct apm_module_param_data_t)
                                        + customPayloadSize);
    payloadInfo = (uint8_t *)calloc(1, (size_t)payloadSize);
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "failed to allocate memory.");
        return;
    }

    header = (struct apm_module_param_data_t*)payloadInfo;
    header->module_instance_id = miid;
    header->param_id = param_id;
    header->error_code = 0x0;
    header->param_size = customPayloadSize;
    val = (isTwsMonoModeOn == true) ? 1 : 2;
    if (codecFormat == CODEC_TYPE_APTX_DUAL_MONO) {
        aptx_classic_payload =
            (param_id_aptx_classic_switch_enc_pcm_input_payload_t*)(payloadInfo +
             sizeof(struct apm_module_param_data_t));
        aptx_classic_payload->transition_direction = val;
        ar_mem_cpy(payloadInfo + sizeof(struct apm_module_param_data_t),
                         customPayloadSize,
                         aptx_classic_payload,
                         customPayloadSize);
    } else {
        aptx_adaptive_payload =
            (param_id_aptx_adaptive_enc_switch_to_mono_t*)(payloadInfo +
             sizeof(struct apm_module_param_data_t));
        aptx_adaptive_payload->switch_between_mono_and_stereo = val;
        ar_mem_cpy(payloadInfo + sizeof(struct apm_module_param_data_t),
                         customPayloadSize,
                         aptx_adaptive_payload,
                         customPayloadSize);
    }

    *size = payloadSize;
    *payload = payloadInfo;
}

void PayloadBuilder::payloadRATConfig(uint8_t** payload, size_t* size,
        uint32_t miid, struct qal_media_config *data)
{
    struct apm_module_param_data_t* header = NULL;
    struct param_id_rat_mf_t *ratConf;
    int numChannel;
    uint32_t bitWidth;
    uint16_t* pcmChannel = NULL;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    if (!data) {
        QAL_ERR(LOG_TAG, "Invalid input parameters");
        return;
    }

    numChannel = data->ch_info.channels;
    bitWidth = data->bit_width;
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct param_id_rat_mf_t) +
                  sizeof(uint16_t)*numChannel;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    ratConf = (struct param_id_rat_mf_t*)(payloadInfo +
               sizeof(struct apm_module_param_data_t));
    pcmChannel = (uint16_t*)(payloadInfo + sizeof(struct apm_module_param_data_t) +
                                       sizeof(struct param_id_rat_mf_t));

    header->module_instance_id = miid;
    header->param_id = PARAM_ID_RAT_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_DBG(LOG_TAG, "header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                      header->module_instance_id, header->param_id,
                      header->error_code, header->param_size);

    ratConf->sample_rate = data->sample_rate;
    if (bitWidth == 16 || bitWidth == 32) {
        ratConf->bits_per_sample = bitWidth;
        ratConf->q_factor =  bitWidth - 1;
    } else if (bitWidth == 24) {
        ratConf->bits_per_sample = 32;
        ratConf->q_factor = 27;
    }
    ratConf->data_format = DATA_FORMAT_FIXED_POINT;
    ratConf->num_channels = numChannel;
    populateChannelMap(pcmChannel, numChannel);
    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "sample_rate:%d bits_per_sample:%d q_factor:%d data_format:%d num_channels:%d",
                      ratConf->sample_rate, ratConf->bits_per_sample, ratConf->q_factor,
                      ratConf->data_format, ratConf->num_channels);
    QAL_DBG(LOG_TAG, "customPayload address %pK and size %d", payloadInfo,
                *size);
}

void PayloadBuilder::payloadPcmCnvConfig(uint8_t** payload, size_t* size,
        uint32_t miid, struct qal_media_config *data)
{
    struct apm_module_param_data_t* header = NULL;
    struct media_format_t *mediaFmtHdr;
    struct payload_pcm_output_format_cfg_t *mediaFmtPayload;
    int numChannels;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    uint8_t *pcmChannel;

    if (!data) {
        QAL_ERR(LOG_TAG, "Invalid input parameters");
        return;
    }

    numChannels = data->ch_info.channels;
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct media_format_t) +
                  sizeof(struct payload_pcm_output_format_cfg_t) +
                  sizeof(uint8_t)*numChannels;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }
    header          = (struct apm_module_param_data_t*)payloadInfo;
    mediaFmtHdr     = (struct media_format_t*)(payloadInfo +
                      sizeof(struct apm_module_param_data_t));
    mediaFmtPayload = (struct payload_pcm_output_format_cfg_t*)(payloadInfo +
                      sizeof(struct apm_module_param_data_t) +
                      sizeof(struct media_format_t));
    pcmChannel      = (uint8_t*)(payloadInfo + sizeof(struct apm_module_param_data_t) +
                      sizeof(struct media_format_t) +
                      sizeof(struct payload_pcm_output_format_cfg_t));

    header->module_instance_id = miid;
    header->param_id           = PARAM_ID_PCM_OUTPUT_FORMAT_CFG;
    header->error_code         = 0x0;
    header->param_size         = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_DBG(LOG_TAG, "header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                      header->module_instance_id, header->param_id,
                      header->error_code, header->param_size);

    mediaFmtHdr->data_format  = DATA_FORMAT_FIXED_POINT;
    mediaFmtHdr->fmt_id       = MEDIA_FMT_ID_PCM;
    mediaFmtHdr->payload_size = sizeof(payload_pcm_output_format_cfg_t) +
                                sizeof(uint8_t) * numChannels;
    QAL_DBG(LOG_TAG, "mediaFmtHdr data_format:%x fmt_id:%x payload_size:%d channels:%d",
                      mediaFmtHdr->data_format, mediaFmtHdr->fmt_id,
                      mediaFmtHdr->payload_size, numChannels);

    mediaFmtPayload->endianness      = PCM_LITTLE_ENDIAN;
    mediaFmtPayload->num_channels    = data->ch_info.channels;
    if ((data->bit_width == 16) || (data->bit_width == 32)) {
        mediaFmtPayload->bit_width       = data->bit_width;
        mediaFmtPayload->bits_per_sample = data->bit_width;
        mediaFmtPayload->q_factor        = data->bit_width - 1;
        mediaFmtPayload->alignment       = PCM_LSB_ALIGNED;
    } else if (data->bit_width == 24) {
        // convert to Q31 that's expected by HD encoders.
        mediaFmtPayload->bit_width       = BIT_WIDTH_24;
        mediaFmtPayload->bits_per_sample = BITS_PER_SAMPLE_32;
        mediaFmtPayload->q_factor        = PCM_Q_FACTOR_31;
        mediaFmtPayload->alignment       = PCM_MSB_ALIGNED;
    } else {
        QAL_ERR(LOG_TAG, "invalid bit width %d", data->bit_width);
        delete[] payloadInfo;
        *size = 0;
        *payload = NULL;
        return;
    }
    mediaFmtPayload->interleaved     = PCM_INTERLEAVED;
    QAL_DBG(LOG_TAG, "interleaved:%d bit_width:%d bits_per_sample:%d q_factor:%d",
                  mediaFmtPayload->interleaved, mediaFmtPayload->bit_width,
                  mediaFmtPayload->bits_per_sample, mediaFmtPayload->q_factor);
    populateChannelMap(pcmChannel, numChannels);
    *size = (payloadSize + padBytes);
    *payload = payloadInfo;

    QAL_DBG(LOG_TAG, "customPayload address %pK and size %d", payloadInfo,
                *size);
}

void PayloadBuilder::payloadCopPackConfig(uint8_t** payload, size_t* size,
        uint32_t miid, struct qal_media_config *data)
{
    struct apm_module_param_data_t* header = NULL;
    struct param_id_cop_pack_output_media_fmt_t *copPack  = NULL;
    int numChannel;
    uint16_t* pcmChannel = NULL;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;

    if (!data) {
        QAL_ERR(LOG_TAG, "Invalid input parameters");
        return;
    }

    numChannel = data->ch_info.channels;
    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(struct param_id_cop_pack_output_media_fmt_t) +
                  sizeof(uint16_t)*numChannel;
    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = new uint8_t[payloadSize + padBytes]();
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo alloc failed %s", strerror(errno));
        return;
    }
    header = (struct apm_module_param_data_t*)payloadInfo;
    copPack = (struct param_id_cop_pack_output_media_fmt_t*)(payloadInfo +
               sizeof(struct apm_module_param_data_t));
    pcmChannel = (uint16_t*)(payloadInfo +
                          sizeof(struct apm_module_param_data_t) +
                          sizeof(struct param_id_cop_pack_output_media_fmt_t));

    header->module_instance_id = miid;
    header->param_id = PARAM_ID_COP_PACKETIZER_OUTPUT_MEDIA_FORMAT;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);
    QAL_DBG(LOG_TAG, "header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                      header->module_instance_id, header->param_id,
                      header->error_code, header->param_size);

    copPack->sampling_rate = data->sample_rate;
    copPack->bits_per_sample = data->bit_width;
    copPack->num_channels = numChannel;
    populateChannelMap(pcmChannel, numChannel);
    *size = payloadSize + padBytes;
    *payload = payloadInfo;
    QAL_DBG(LOG_TAG, "sample_rate:%d bits_per_sample:%d num_channels:%d",
                      copPack->sampling_rate, copPack->bits_per_sample, copPack->num_channels);
    QAL_DBG(LOG_TAG, "customPayload address %pK and size %d", payloadInfo,
                *size);
}

/** Used for Loopback stream types only */
int PayloadBuilder::populateStreamKV(Stream* s, std::vector <std::pair<int,int>> &keyVectorRx,
        std::vector <std::pair<int,int>> &keyVectorTx, struct vsid_info vsidinfo)
{
    int status = 0;
    struct qal_stream_attributes *sattr = NULL;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    sattr = new struct qal_stream_attributes();
    if (!sattr) {
        QAL_ERR(LOG_TAG,"sattr alloc failed %s status %d", strerror(errno), status);
        status = -ENOMEM;
        goto exit;
    }
    status = s->getStreamAttributes(sattr);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"getStreamAttributes Failed status %d\n", __func__, status);
        goto free_sattr;
    }

    QAL_DBG(LOG_TAG, "stream attribute type %d", sattr->type);
    switch (sattr->type) {
        case QAL_STREAM_LOOPBACK:
            if (sattr->info.opt_stream_info.loopback_type == QAL_STREAM_LOOPBACK_HFP_RX) {
                keyVectorRx.push_back(std::make_pair(STREAMRX, HFP_RX_PLAYBACK));
                keyVectorTx.push_back(std::make_pair(STREAMTX, HFP_RX_CAPTURE));
            } else if (sattr->info.opt_stream_info.loopback_type == QAL_STREAM_LOOPBACK_HFP_TX) {
                /** no StreamKV for HFP TX */
            } else /** pcm loopback*/ {
                keyVectorRx.push_back(std::make_pair(STREAMRX, PCM_RX_LOOPBACK));
            }
            break;
    case QAL_STREAM_VOICE_CALL:
            /*need to update*/
            for (int size= 0; size < vsidinfo.modepair.size(); size++) {
                for (int count1 = 0; count1 < VSIDtoKV.size(); count1++) {
                    if (vsidinfo.modepair[size].key == VSIDtoKV[count1].first)
                        VSIDtoKV[count1].second = vsidinfo.modepair[size].value;
                }
            }

            keyVectorRx.push_back(std::make_pair(STREAMRX,VOICE_CALL_RX));
            keyVectorTx.push_back(std::make_pair(STREAMTX,VOICE_CALL_TX));
            for (int index = 0; index < VSIDtoKV.size(); index++) {
                if (sattr->info.voice_call_info.VSID == VSIDtoKV[index].first) {
                    keyVectorRx.push_back(std::make_pair(vsidinfo.vsid,VSIDtoKV[index].second));
                    keyVectorTx.push_back(std::make_pair(vsidinfo.vsid,VSIDtoKV[index].second));
                }
            }
            break;
        default:
            status = -EINVAL;
            QAL_ERR(LOG_TAG,"unsupported stream type %d", sattr->type);
    }
free_sattr:
    delete sattr;
exit:
    return status;
}

/** Used for Loopback stream types only */
int PayloadBuilder::populateStreamPPKV(Stream* s, std::vector <std::pair<int,int>> &keyVectorRx,
        std::vector <std::pair<int,int>> &keyVectorTx __unused)
{
    int status = 0;
    struct qal_stream_attributes *sattr = NULL;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    sattr = new struct qal_stream_attributes();
    if (!sattr) {
        QAL_ERR(LOG_TAG,"sattr alloc failed %s status %d", strerror(errno), status);
        status = -ENOMEM;
        goto exit;
    }
    status = s->getStreamAttributes(sattr);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"getStreamAttributes Failed status %d\n", __func__, status);
        goto free_sattr;
    }

    QAL_DBG(LOG_TAG, "stream attribute type %d", sattr->type);
    switch (sattr->type) {
        case QAL_STREAM_VOICE_CALL:
            /*need to update*/
            keyVectorRx.push_back(std::make_pair(STREAMPP_RX, STREAMPP_RX_DEFAULT));
            break;
        default:
            QAL_ERR(LOG_TAG,"unsupported stream type %d", sattr->type);
    }
free_sattr:
    delete sattr;
exit:
    return status;
}

int PayloadBuilder::populateStreamKV(Stream* s,
        std::vector <std::pair<int,int>> &keyVector)
{
    int status = -EINVAL;
    uint32_t instance_id = 0;
    struct qal_stream_attributes *sattr = NULL;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    sattr = new struct qal_stream_attributes;
    if (!sattr) {
        status = -ENOMEM;
        QAL_ERR(LOG_TAG,"sattr malloc failed %s status %d", strerror(errno), status);
        goto exit;
    }
    memset (sattr, 0, sizeof(struct qal_stream_attributes));

    status = s->getStreamAttributes(sattr);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"getStreamAttributes Failed status %d\n", __func__, status);
        goto free_sattr;
    }

    //todo move the keys to a to an xml of stream type to key
    //something like stream_type=QAL_STREAM_LOW_LATENCY, key=PCM_LL_PLAYBACK
    //from there create a map and retrieve the right keys
    QAL_DBG(LOG_TAG, "stream attribute type %d", sattr->type);
    switch (sattr->type) {
        case QAL_STREAM_LOW_LATENCY:
            if (sattr->direction == QAL_AUDIO_OUTPUT) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_LL_PLAYBACK));
                keyVector.push_back(std::make_pair(INSTANCE, INSTANCE_1));
            } else if (sattr->direction == QAL_AUDIO_INPUT) {
                keyVector.push_back(std::make_pair(STREAMTX,RAW_RECORD));
            } else if (sattr->direction == (QAL_AUDIO_OUTPUT | QAL_AUDIO_INPUT)) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_RX_LOOPBACK));
            } else {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid direction status %d", status);
                goto free_sattr;
            }
            break;
        case QAL_STREAM_ULTRA_LOW_LATENCY:
            if (sattr->direction == QAL_AUDIO_OUTPUT) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_ULL_PLAYBACK));
                //keyVector.push_back(std::make_pair(INSTANCE,INSTANCE_1));
            } else if (sattr->direction == QAL_AUDIO_INPUT) {
                keyVector.push_back(std::make_pair(STREAMTX,PCM_ULL_RECORD));
            } else {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid direction status %d", status);
                goto free_sattr;
            }
            break;
        case QAL_STREAM_PROXY:
            if (sattr->direction == QAL_AUDIO_OUTPUT) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_PROXY_PLAYBACK));
                //keyVector.push_back(std::make_pair(INSTANCE,INSTANCE_1));
            } else if (sattr->direction == QAL_AUDIO_INPUT) {
                keyVector.push_back(std::make_pair(STREAMTX,PCM_PROXY_RECORD));
            } else {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid direction status %d", status);
                goto free_sattr;
            }
            break;
        case QAL_STREAM_DEEP_BUFFER:
            if (sattr->direction == QAL_AUDIO_OUTPUT) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_DEEP_BUFFER));
            } else if (sattr->direction == QAL_AUDIO_INPUT) {
                keyVector.push_back(std::make_pair(STREAMTX,PCM_RECORD));
            } else {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid direction status %d", status);
                goto free_sattr;
            }
            break;
        case QAL_STREAM_PCM_OFFLOAD:
            if (sattr->direction == QAL_AUDIO_OUTPUT) {
                keyVector.push_back(std::make_pair(STREAMRX,PCM_OFFLOAD_PLAYBACK));
                keyVector.push_back(std::make_pair(INSTANCE, INSTANCE_1));
            } else {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid direction status %d", status);
                goto free_sattr;
            }
            break;
        case QAL_STREAM_GENERIC:
            break;
        case QAL_STREAM_COMPRESSED:
           if (sattr->direction == QAL_AUDIO_OUTPUT) {
               QAL_VERBOSE(LOG_TAG,"%s: Stream compressed \n", __func__);
               keyVector.push_back(std::make_pair(STREAMRX, COMPRESSED_OFFLOAD_PLAYBACK));
               keyVector.push_back(std::make_pair(INSTANCE, INSTANCE_1));
           }
            break;
        case QAL_STREAM_VOIP_TX:
            keyVector.push_back(std::make_pair(STREAMTX, VOIP_TX_RECORD));
            break;
        case QAL_STREAM_VOIP_RX:
            keyVector.push_back(std::make_pair(STREAMRX, VOIP_RX_PLAYBACK));
            break;
        case QAL_STREAM_VOICE_UI:
            if (!s) {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid stream");
                goto free_sattr;
            }
            keyVector.push_back(std::make_pair(STREAMTX, VOICE_UI));

            // add key-vector for stream configuration
            for (auto& kv: s->getStreamModifiers()) {
                keyVector.push_back(kv);
            }

            instance_id = s->getInstanceId();
            if (instance_id < INSTANCE_1) {
                status = -EINVAL;
                QAL_ERR(LOG_TAG, "Invalid instance id %d for Voice UI stream",
                    instance_id);
                goto free_sattr;
            }
            keyVector.push_back(std::make_pair(INSTANCE, instance_id));
            break;
        case QAL_STREAM_VOICE_CALL_RECORD:
            keyVector.push_back(std::make_pair(STREAMTX,INCALL_RECORD));
            break;
        case QAL_STREAM_VOICE_CALL_MUSIC:
            keyVector.push_back(std::make_pair(STREAMRX,INCALL_MUSIC));
            break;
        default:
            status = -EINVAL;
            QAL_ERR(LOG_TAG,"unsupported stream type %d", sattr->type);
            goto free_sattr;
        }

free_sattr:
    delete sattr;
exit:
    return status;

}

int PayloadBuilder::populateStreamDeviceKV(Stream* s __unused, int32_t beDevId __unused,
        std::vector <std::pair<int,int>> &keyVector __unused)
{
    int status = 0;

    QAL_VERBOSE(LOG_TAG,"%s: enter", __func__);
    return status;
}

int PayloadBuilder::populateStreamDeviceKV(Stream* s, int32_t rxBeDevId,
        std::vector <std::pair<int,int>> &keyVectorRx, int32_t txBeDevId,
        std::vector <std::pair<int,int>> &keyVectorTx, struct vsid_info vsidinfo,
                                           sidetone_mode_t sidetoneMode)
{
    int status = 0;

    QAL_VERBOSE(LOG_TAG,"%s: enter", __func__);
    status = populateStreamKV(s, keyVectorRx, keyVectorTx, vsidinfo);
    if (status)
        goto exit;

    status = populateDeviceKV(s, rxBeDevId, keyVectorRx, txBeDevId,
            keyVectorTx, sidetoneMode);

exit:
    return status;
}

int PayloadBuilder::populateDeviceKV(Stream* s __unused, int32_t beDevId,
        std::vector <std::pair<int,int>> &keyVector)
{
    int status = 0;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    //todo move the keys to a to an xml  of device type to key
    //something like device_type=DEVICETX, key=SPEAKER
    //from there create a map and retrieve the right keys

//TODO change this mapping to xml
    switch (beDevId) {
        case QAL_DEVICE_OUT_SPEAKER :
            keyVector.push_back(std::make_pair(DEVICERX, SPEAKER));
            break;
        case QAL_DEVICE_OUT_HANDSET :
            keyVector.push_back(std::make_pair(DEVICERX, HANDSET));
            break;
        case QAL_DEVICE_OUT_BLUETOOTH_A2DP:
            // device gkv of A2DP is sent elsewhere, skip here.
            break;
        case QAL_DEVICE_OUT_BLUETOOTH_SCO:
            keyVector.push_back(std::make_pair(DEVICERX, BT_RX));
            keyVector.push_back(std::make_pair(BT_PROFILE, SCO));
            break;
        case QAL_DEVICE_OUT_AUX_DIGITAL:
        case QAL_DEVICE_OUT_AUX_DIGITAL_1:
        case QAL_DEVICE_OUT_HDMI:
           keyVector.push_back(std::make_pair(DEVICERX, HDMI_RX));
           break;
        case QAL_DEVICE_OUT_WIRED_HEADSET:
        case QAL_DEVICE_OUT_WIRED_HEADPHONE:
            keyVector.push_back(std::make_pair(DEVICERX,HEADPHONES));
            break;
        case QAL_DEVICE_OUT_USB_HEADSET:
        case QAL_DEVICE_OUT_USB_DEVICE:
            keyVector.push_back(std::make_pair(DEVICERX, USB_RX));
            break;
        case QAL_DEVICE_IN_SPEAKER_MIC:
            keyVector.push_back(std::make_pair(DEVICETX, SPEAKER_MIC));
            break;
        case QAL_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
            keyVector.push_back(std::make_pair(DEVICETX, BT_TX));
            keyVector.push_back(std::make_pair(BT_PROFILE, SCO));
            break;
        case QAL_DEVICE_IN_WIRED_HEADSET:
           keyVector.push_back(std::make_pair(DEVICETX, HEADPHONE_MIC));
           break;
        case QAL_DEVICE_IN_USB_DEVICE:
        case QAL_DEVICE_IN_USB_HEADSET:
            keyVector.push_back(std::make_pair(DEVICETX, USB_TX));
            break;
        case QAL_DEVICE_IN_HANDSET_MIC:
           keyVector.push_back(std::make_pair(DEVICETX, HANDSETMIC));
           break;
        case QAL_DEVICE_IN_HANDSET_VA_MIC:
            keyVector.push_back(std::make_pair(DEVICETX, HANDSETMIC_VA));
            break;
        case QAL_DEVICE_IN_HEADSET_VA_MIC:
            keyVector.push_back(std::make_pair(DEVICETX, HEADSETMIC_VA));
            break;
        case QAL_DEVICE_IN_PROXY:
            keyVector.push_back(std::make_pair(DEVICETX, PROXY_TX));
            break;
        case QAL_DEVICE_OUT_PROXY:
            keyVector.push_back(std::make_pair(DEVICERX, PROXY_RX));
            break;
        default:
            QAL_DBG(LOG_TAG,"%s: Invalid device id %d\n", __func__,beDevId);
            break;
    }

    return status;

}

int PayloadBuilder::populateDeviceKV(Stream* s, int32_t rxBeDevId,
        std::vector <std::pair<int,int>> &keyVectorRx, int32_t txBeDevId,
        std::vector <std::pair<int,int>> &keyVectorTx, sidetone_mode_t sidetoneMode)
{
    int status = 0;
    struct qal_stream_attributes sAttr;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);

    status = s->getStreamAttributes(&sAttr);
    if(0 != status) {
        QAL_ERR(LOG_TAG,"%s: getStreamAttributes Failed \n", __func__);
        return status;
    }

    populateDeviceKV(s, rxBeDevId, keyVectorRx);
    populateDeviceKV(s, txBeDevId, keyVectorTx);

    /*add sidetone kv if needed*/
    if (sAttr.type == QAL_STREAM_VOICE_CALL && sidetoneMode == SIDETONE_SW) {
        QAL_DBG(LOG_TAG, "SW sidetone mode push kv");
        keyVectorTx.push_back(std::make_pair(SW_SIDETONE, SW_SIDETONE_ON));
    }

    return status;
}

int PayloadBuilder::populateDevicePPKV(Stream* s, int32_t rxBeDevId,
        std::vector <std::pair<int,int>> &keyVectorRx, int32_t txBeDevId,
        std::vector <std::pair<int,int>> &keyVectorTx, std::vector<kvpair_info> kvpair, bool is_lpi __unused)
{
    int status = 0;
    struct qal_stream_attributes *sattr = NULL;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct qal_device dAttr;
    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    sattr = new struct qal_stream_attributes;
    if (!sattr) {
        status = -ENOMEM;
        QAL_ERR(LOG_TAG,"sattr malloc failed %s status %d", strerror(errno), status);
        goto exit;
    }
    memset (&dAttr, 0, sizeof(struct qal_device));
    memset (sattr, 0, sizeof(struct qal_stream_attributes));

    status = s->getStreamAttributes(sattr);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"getStreamAttributes Failed status %d\n", __func__, status);
        goto free_sattr;
    }
    status = s->getAssociatedDevices(associatedDevices);
    if (0 != status) {
       QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
       return status;
    }
    for (int i = 0; i < associatedDevices.size();i++) {
       status = associatedDevices[i]->getDeviceAttributes(&dAttr);
       if (0 != status) {
          QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
          return status;
       }
       if ((dAttr.id == rxBeDevId) || (dAttr.id == txBeDevId)) {
          QAL_DBG(LOG_TAG,"channels %d, id %d\n",dAttr.config.ch_info.channels, dAttr.id);
       }

        //todo move the keys to a to an xml of stream type to key
        //something like stream_type=QAL_STREAM_LOW_LATENCY, key=PCM_LL_PLAYBACK
        //from there create a map and retrieve the right keys
        QAL_DBG(LOG_TAG, "stream attribute type %d", sattr->type);
        switch (sattr->type) {
            case QAL_STREAM_VOICE_CALL:
                if (dAttr.id == rxBeDevId){
                    keyVectorRx.push_back(std::make_pair(DEVICEPP_RX, DEVICEPP_RX_VOICE_DEFAULT));
                }
                if (dAttr.id == txBeDevId){
                    for (int32_t kvsize = 0; kvsize < kvpair.size(); kvsize++) {
                         keyVectorTx.push_back(std::make_pair(kvpair[kvsize].key,
                                               kvpair[kvsize].value));
                    }
                }
                break;
            case QAL_STREAM_LOW_LATENCY:
            case QAL_STREAM_COMPRESSED:
            case QAL_STREAM_DEEP_BUFFER:
            case QAL_STREAM_PCM_OFFLOAD:
                if (sattr->direction == QAL_AUDIO_OUTPUT) {
                  if(dAttr.id == QAL_DEVICE_OUT_PROXY) {
                    QAL_DBG(LOG_TAG,"Device PP for Proxy is Rx Default");
                    keyVectorRx.push_back(std::make_pair(DEVICEPP_RX, DEVICEPP_RX_DEFAULT));
                  }
                  else {
                    keyVectorRx.push_back(std::make_pair(DEVICEPP_RX, DEVICEPP_RX_AUDIO_MBDRC));
                  }
                }
                else if (sattr->direction == QAL_AUDIO_INPUT) {
                    for (int32_t kvsize = 0; kvsize < kvpair.size(); kvsize++) {
                         keyVectorTx.push_back(std::make_pair(kvpair[kvsize].key,
                                               kvpair[kvsize].value));
                    }
                }
                break;
            case QAL_STREAM_VOIP_RX:
                keyVectorRx.push_back(std::make_pair(DEVICEPP_RX, DEVICEPP_RX_VOIP_MBDRC));
                break;
            case QAL_STREAM_LOOPBACK:
                if (sattr->info.opt_stream_info.loopback_type ==
                                                    QAL_STREAM_LOOPBACK_HFP_RX) {
                    keyVectorRx.push_back(std::make_pair(DEVICEPP_RX,
                                                         DEVICEPP_RX_HFPSINK));
                } else if(sattr->info.opt_stream_info.loopback_type ==
                                                    QAL_STREAM_LOOPBACK_HFP_TX) {
                    keyVectorTx.push_back(std::make_pair(DEVICEPP_TX,
                                                         DEVICEPP_TX_HFP_SINK_FLUENCE_SMECNS));
                }
                break;
            case QAL_STREAM_VOIP_TX:
                for (int32_t kvsize = 0; kvsize < kvpair.size(); kvsize++) {
                     keyVectorTx.push_back(std::make_pair(kvpair[kvsize].key,
                                           kvpair[kvsize].value));
                }
                break;
            case QAL_STREAM_VOICE_UI:
                /*
                 * add key-vector for the device pre-proc that was selected
                 * by the stream
                 */
                for (auto& kv: s->getDevPpModifiers())
                    keyVectorTx.push_back(kv);
                break;
            default:
                QAL_ERR(LOG_TAG,"stream type %d doesn't support populateDevicePPKV ", sattr->type);
                goto free_sattr;
        }
    }
    populateDeviceKV(s, rxBeDevId, keyVectorRx);
    populateDeviceKV(s, txBeDevId, keyVectorTx);
free_sattr:
    delete sattr;
exit:
    return status;
}

int PayloadBuilder::populateStreamCkv(Stream *s __unused, std::vector <std::pair<int,int>> &keyVector __unused, int tag __unused,
        struct qal_volume_data **volume_data __unused)
{
    int status = 0;

    QAL_DBG(LOG_TAG, "Enter");

    /*
     * Sending volume minimum as we want to ramp up instead of ramping
     * down while setting the desired volume. Thus avoiding glitch
     * TODO: Decide what to send as ckv in graph open
     */
    keyVector.push_back(std::make_pair(VOLUME,LEVEL_15));
    QAL_DBG(LOG_TAG, "Entered default %x %x", VOLUME, LEVEL_0);

    return status;
}

int PayloadBuilder::populateDevicePPCkv(Stream *s, std::vector <std::pair<int,int>> &keyVector)
{
    int status = 0;
    struct qal_stream_attributes *sattr = NULL;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    struct qal_device dAttr;

    QAL_DBG(LOG_TAG,"%s: enter", __func__);
    sattr = new struct qal_stream_attributes;
    if (!sattr) {
        status = -ENOMEM;
        QAL_ERR(LOG_TAG,"sattr malloc failed %s status %d", strerror(errno), status);
        goto exit;
    }
    memset (&dAttr, 0, sizeof(struct qal_device));
    memset (sattr, 0, sizeof(struct qal_stream_attributes));

    status = s->getStreamAttributes(sattr);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"getStreamAttributes Failed status %d\n", __func__, status);
        goto free_sattr;
    }
    status = s->getAssociatedDevices(associatedDevices);
    if (0 != status) {
       QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
       return status;
    }
    for (int i = 0; i < associatedDevices.size();i++) {
        status = associatedDevices[i]->getDeviceAttributes(&dAttr);
        if (0 != status) {
            QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
            return status;
        }

        switch (sattr->type) {
            case QAL_STREAM_VOICE_UI:
                QAL_INFO(LOG_TAG,"channels %d, id %d\n",dAttr.config.ch_info.channels, dAttr.id);
                /* Push Channels CKV for FFNS or FFECNS channel based calibration */
                keyVector.push_back(std::make_pair(CHANNELS,
                                                   dAttr.config.ch_info.channels));
                break;
            default:
                QAL_VERBOSE(LOG_TAG,"stream type %d doesn't support DevicePP CKV ", sattr->type);
                goto free_sattr;
        }
    }
free_sattr:
    delete sattr;
exit:
    return status;
}

int PayloadBuilder::populateCalKeyVector(Stream *s, std::vector <std::pair<int,int>> &ckv, int tag) {
    int status = 0;
    QAL_VERBOSE(LOG_TAG,"%s: enter \n", __func__);
    std::vector <std::pair<int,int>> keyVector;
    struct qal_stream_attributes sAttr;
    std::shared_ptr<CaptureProfile> cap_prof = nullptr;
    KeyVect_t stream_config_kv;
    struct qal_device dAttr;
    std::vector<std::shared_ptr<Device>> associatedDevices;
    std::shared_ptr<ResourceManager> rm = ResourceManager::getInstance();

    status = s->getStreamAttributes(&sAttr);
    if(0 != status) {
        QAL_ERR(LOG_TAG, "getStreamAttributes Failed");
        return status;
    }

    float voldB = 0.0f;
    struct qal_volume_data *voldata = NULL;
    voldata = (struct qal_volume_data *)calloc(1, (sizeof(uint32_t) +
                      (sizeof(struct qal_channel_vol_kv) * (0xFFFF))));
    if (!voldata) {
        status = -ENOMEM;
        goto exit;
    }

    status = s->getVolumeData(voldata);
    if (0 != status) {
        QAL_ERR(LOG_TAG,"%s: getVolumeData Failed \n", __func__);
        goto error_1;
    }

    QAL_VERBOSE(LOG_TAG,"%s: volume sent:%f \n",__func__, (voldata->volume_pair[0].vol));
    voldB = (voldata->volume_pair[0].vol);

    switch (static_cast<uint32_t>(tag)) {
    case TAG_STREAM_VOLUME:
       if (voldB == 0.0f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_15));
       }
       else if (voldB < 0.002172f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_15));
       }
       else if (voldB < 0.004660f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_14));
       }
       else if (voldB < 0.01f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_13));
       }
       else if (voldB < 0.014877f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_12));
       }
       else if (voldB < 0.023646f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_11));
       }
       else if (voldB < 0.037584f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_10));
       }
       else if (voldB < 0.055912f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_9));
       }
       else if (voldB < 0.088869f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_8));
       }
       else if (voldB < 0.141254f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_7));
       }
       else if (voldB < 0.189453f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_6));
       }
       else if (voldB < 0.266840f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_5));
       }
       else if (voldB < 0.375838f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_4));
       }
       else if (voldB < 0.504081f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_3));
       }
       else if (voldB < 0.709987f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_2));
       }
       else if (voldB < 0.9f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_1));
       }
       else if (voldB <= 1.0f) {
          ckv.push_back(std::make_pair(VOLUME,LEVEL_0));
       }
       break;
    case TAG_MODULE_CHANNELS:
        if (sAttr.type == QAL_STREAM_VOICE_UI) {
            stream_config_kv = s->getStreamModifiers();
            if (stream_config_kv.size() == 0 ||
                stream_config_kv[0].second != VUI_STREAM_CFG_SVA) {
                QAL_DBG(LOG_TAG, "Skip fluence ckv for non-SVA case");
                break;
            }

            cap_prof = rm->GetSVACaptureProfile();
            if (!cap_prof) {
                QAL_ERR(LOG_TAG, "Invalid capture profile");
                status = -EINVAL;
                break;
            }

            if (!cap_prof->GetChannels()) {
                QAL_ERR(LOG_TAG, "Invalid channels");
                status = -EINVAL;
                break;
            }
            ckv.push_back(std::make_pair(CHANNELS,
                cap_prof->GetChannels()));
        }
        break;
    case SPKR_PROT_ENABLED :
        status = s->getAssociatedDevices(associatedDevices);
        if (0 != status) {
            QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
            return status;
        }

        for (int i = 0; i < associatedDevices.size(); i++) {
            status = associatedDevices[i]->getDeviceAttributes(&dAttr);
            if (0 != status) {
                QAL_ERR(LOG_TAG,"%s: getAssociatedDevices Failed \n", __func__);
                return status;
            }
            if (dAttr.id == QAL_DEVICE_OUT_SPEAKER) {
                if (dAttr.config.ch_info.channels > 1) {
                    QAL_DBG(LOG_TAG, "Multi channel speaker");
                    ckv.push_back(std::make_pair(SPK_PRO_CH_MAP, LEFT_RIGHT));
                }
                else {
                    QAL_DBG(LOG_TAG, "Mono channel speaker");
                    ckv.push_back(std::make_pair(SPK_PRO_CH_MAP, RIGHT_MONO));
                }
                break;
            }
        }
        break;
    case SPKR_PROT_DISABLED :
        ckv.push_back(std::make_pair(SPK_PRO_CH_MAP, SP_DISABLED));
        break;
    default:
        break;
    }

    QAL_VERBOSE(LOG_TAG,"%s: exit status- %d", __func__, status);
error_1:
    free(voldata);
exit:
    return status;
}

int PayloadBuilder::populateTagKeyVector(Stream *s, std::vector <std::pair<int,int>> &tkv, int tag, uint32_t* gsltag)
{
    int status = 0;
    QAL_VERBOSE(LOG_TAG,"%s: enter, tag 0x%x", __func__, tag);
    struct qal_stream_attributes sAttr;

    status = s->getStreamAttributes(&sAttr);

    if (status != 0) {
        QAL_ERR(LOG_TAG,"stream get attributes failed");
        return status;
    }

    switch (tag) {
    case MUTE_TAG:
       tkv.push_back(std::make_pair(MUTE,ON));
       *gsltag = TAG_MUTE;
       break;
    case UNMUTE_TAG:
       tkv.push_back(std::make_pair(MUTE,OFF));
       *gsltag = TAG_MUTE;
       break;
    case VOICE_SLOW_TALK_OFF:
       tkv.push_back(std::make_pair(TAG_KEY_SLOW_TALK, TAG_VALUE_SLOW_TALK_OFF));
       *gsltag = TAG_STREAM_SLOW_TALK;
       break;
    case VOICE_SLOW_TALK_ON:
       tkv.push_back(std::make_pair(TAG_KEY_SLOW_TALK, TAG_VALUE_SLOW_TALK_ON));
       *gsltag = TAG_STREAM_SLOW_TALK;
       break;
    case PAUSE_TAG:
       tkv.push_back(std::make_pair(PAUSE,ON));
       *gsltag = TAG_PAUSE;
       break;
    case RESUME_TAG:
       tkv.push_back(std::make_pair(PAUSE,OFF));
       *gsltag = TAG_PAUSE;
       break;
    case MFC_SR_8K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_8K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_16K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_16K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_32K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_32K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_44K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_44K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_48K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_48K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_96K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_96K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_192K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_192K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case MFC_SR_384K:
       tkv.push_back(std::make_pair(SAMPLINGRATE,SAMPLINGRATE_384K));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case FLUENCE_ON_TAG:
       tkv.push_back(std::make_pair(FLUENCE,FLUENCE_ON));
       *gsltag = TAG_FLUENCE;
       break;
    case FLUENCE_OFF_TAG:
       tkv.push_back(std::make_pair(FLUENCE,FLUENCE_OFF));
       *gsltag = TAG_FLUENCE;
       break;
    case FLUENCE_EC_TAG:
       tkv.push_back(std::make_pair(FLUENCE,FLUENCE_EC));
       *gsltag = TAG_FLUENCE;
       break;
    case FLUENCE_NS_TAG:
       tkv.push_back(std::make_pair(FLUENCE,FLUENCE_NS));
       *gsltag = TAG_FLUENCE;
       break;
    case CHS_1:
       tkv.push_back(std::make_pair(CHANNELS, CHANNELS_1));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case CHS_2:
       tkv.push_back(std::make_pair(CHANNELS, CHANNELS_2));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case CHS_3:
       tkv.push_back(std::make_pair(CHANNELS, CHANNELS_3));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case CHS_4:
       tkv.push_back(std::make_pair(CHANNELS, CHANNELS_4));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case BW_16:
       tkv.push_back(std::make_pair(BITWIDTH, BITWIDTH_16));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case BW_24:
       tkv.push_back(std::make_pair(BITWIDTH, BITWIDTH_24));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case BW_32:
       tkv.push_back(std::make_pair(BITWIDTH, BITWIDTH_32));
       if (sAttr.direction == QAL_AUDIO_INPUT)
            *gsltag = TAG_STREAM_MFC_SR;
       else
            *gsltag = TAG_DEVICE_MFC_SR;
       break;
    case OP_MODE:
        tkv.push_back(std::make_pair(TAG_MODULE_OP_MODE, NORMAL));
        *gsltag = TAG_MODULE_OP_MODE;
    case INCALL_RECORD_UPLINK:
       tkv.push_back(std::make_pair(TAG_KEY_MUX_DEMUX_CONFIG, TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK));
       *gsltag = TAG_STREAM_MUX_DEMUX;
       break;
    case INCALL_RECORD_DOWNLINK:
       tkv.push_back(std::make_pair(TAG_KEY_MUX_DEMUX_CONFIG, TAG_VALUE_MUX_DEMUX_CONFIG_DOWNLINK));
       *gsltag = TAG_STREAM_MUX_DEMUX;
       break;
    case INCALL_RECORD_UPLINK_DOWNLINK_MONO:
       tkv.push_back(std::make_pair(TAG_KEY_MUX_DEMUX_CONFIG, TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK_DOWNLINK_MONO));
       *gsltag = TAG_STREAM_MUX_DEMUX;
       break;
    case INCALL_RECORD_UPLINK_DOWNLINK_STEREO:
       tkv.push_back(std::make_pair(TAG_KEY_MUX_DEMUX_CONFIG, TAG_VALUE_MUX_DEMUX_CONFIG_UPLINK_DOWNLINK_STEREO));
       *gsltag = TAG_STREAM_MUX_DEMUX;
       break;
    default:
       QAL_ERR(LOG_TAG,"%s: Tag not supported \n", __func__);
       break;
    }

    QAL_VERBOSE(LOG_TAG,"%s: exit status- %d", __func__, status);
    return status;
}

void PayloadBuilder::payloadSPConfig(uint8_t** payload, size_t* size, uint32_t miid, void *param)
{
    struct apm_module_param_data_t* header = NULL;
    param_id_sp_th_vi_r0t0_cfg_t *spConf;
    uint8_t* payloadInfo = NULL;
    size_t payloadSize = 0, padBytes = 0;
    vi_r0t0_cfg_t* r0t0 = NULL;

    param_id_sp_th_vi_r0t0_cfg_t *data = NULL;

    data = (param_id_sp_th_vi_r0t0_cfg_t *) param;

    if (!data) {
        QAL_ERR(LOG_TAG, "Invalid input parameters");
        return;
    }

    payloadSize = sizeof(struct apm_module_param_data_t) +
                  sizeof(param_id_sp_th_vi_r0t0_cfg_t) +
                  sizeof(vi_r0t0_cfg_t) * data->num_speakers;

    padBytes = QAL_PADDING_8BYTE_ALIGN(payloadSize);

    payloadInfo = (uint8_t*) calloc(1, payloadSize + padBytes);
    if (!payloadInfo) {
        QAL_ERR(LOG_TAG, "payloadInfo malloc failed %s", strerror(errno));
        return;
    }

    header = (struct apm_module_param_data_t*) payloadInfo;
    spConf = (param_id_sp_th_vi_r0t0_cfg_t *) (payloadInfo +
                    sizeof(struct apm_module_param_data_t));
    r0t0 = (vi_r0t0_cfg_t*) (payloadInfo + sizeof(struct apm_module_param_data_t)
                             + sizeof(param_id_sp_th_vi_r0t0_cfg_t));

    header->module_instance_id = miid;
    header->param_id = PARAM_ID_SP_TH_VI_R0T0_CFG;
    header->error_code = 0x0;
    header->param_size = payloadSize - sizeof(struct apm_module_param_data_t);

    QAL_DBG(LOG_TAG, "header params \n IID:%x param_id:%x error_code:%d param_size:%d",
                    header->module_instance_id, header->param_id,
                    header->error_code, header->param_size);

    spConf->num_speakers = data->num_speakers;
    for(int i = 0; i < data->num_speakers; i++) {
        r0t0[i].r0_cali_q24 = data->vi_r0t0_cfg[i].r0_cali_q24;
        r0t0[i].t0_cali_q6 = data->vi_r0t0_cfg[i].t0_cali_q6;
    }

    *size = payloadSize + padBytes;
    *payload = payloadInfo;
}
