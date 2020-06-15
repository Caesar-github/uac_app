/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "uac_control.h"
#include <rockit/rt_header.h>
#include <rockit/rt_metadata.h>
#include <rockit/RTUACGraph.h>
#include <rockit/RTMediaBuffer.h>

/*
 * pc datas -> rv1109
 * usb record->xxxx process->speaker playback
 */
#define UAC_USB_RECORD_SPK_PLAY_CONFIG_FILE "/oem/usr/share/uac_app/usb_recode_speaker_playback.json"

/*
 * rv1109 datas -> pc
 * mic record->>xxxx process->usb playback
 */
#define UAC_MIC_RECORD_USB_PLAY_CONFIG_FILE "/oem/usr/share/uac_app/mic_recode_usb_playback.json"

/*
 * test: rv1109 datas -> pc
 * file read->>xxxx process->usb playback
 */
#define UAC_FILE_READ_USB_PLAY_CONFIG_FILE "/oem/usr/share/uac_app/file_read_usb_playback.json"


typedef struct _UACStream {
    pthread_mutex_t mutex;
    RTUACGraph     *uac;
} UACStream;

typedef struct _UACControl {
    UACStream     stream[UAC_STREAM_MAX];
} UACControl;

UACControl *gUAControl = NULL;

int uac_control_create() {
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    gUAControl = (UACControl*)calloc(1, sizeof(UACControl));
    if (!gUAControl) {
        printf("%s:%d fail to malloc memory!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    for (int i = 0; i < UAC_STREAM_MAX; i++) {
        pthread_mutex_init(&gUAControl->stream[i].mutex, NULL);
        gUAControl->stream[i].uac = NULL;
    }

    return 0;
}

void uac_control_destory() {
    if (gUAControl == NULL)
        return;
    printf("%s:%d\n", __FUNCTION__, __LINE__);
    for (int i = 0; i < UAC_STREAM_MAX; i++) {
        if (gUAControl->stream[i].uac != NULL) {
            delete(gUAControl->stream[i].uac);
            gUAControl->stream[i].uac = NULL;
        }
        pthread_mutex_destroy(&gUAControl->stream[i].mutex);
    }

    gUAControl = NULL;
}

int uac_start(int type) {
    if (gUAControl == NULL)
        return -1;

    if (type >= UAC_STREAM_MAX) {
        printf("%s:%d error, type = %d\n", __FUNCTION__, __LINE__, type);
        return -1;
    }

    uac_stop(type);
    char* config = (char*)UAC_MIC_RECORD_USB_PLAY_CONFIG_FILE;
    char* name = (char*)"uac_playback";
    if (type == UAC_STREAM_RECORD) {
        name = (char*)"uac_record";
        config = (char*)UAC_USB_RECORD_SPK_PLAY_CONFIG_FILE;
    }
    printf("%s:%d, config = %s\n", __FUNCTION__, __LINE__, config);
    RTUACGraph* uac = new RTUACGraph(name);
    if (uac == NULL) {
        printf("%s:%d error, malloc fail\n", __FUNCTION__, __LINE__);
        return -1;
    }
    uac->autoBuild(config);
    uac->prepare();
    uac->start();

    pthread_mutex_lock(&gUAControl->stream[type].mutex);
    gUAControl->stream[type].uac = uac;
    pthread_mutex_unlock(&gUAControl->stream[type].mutex);

    return 0;
}

void uac_stop(int type) {
    if (gUAControl == NULL)
        return;

    RTUACGraph* uac = NULL;
    if (type >= UAC_STREAM_MAX) {
        printf("%s:%d error, type = %d\n", __FUNCTION__, __LINE__, type);
        return;
    }
    printf("%s:%d, type = %d\n", __FUNCTION__, __LINE__, type);
    pthread_mutex_lock(&gUAControl->stream[type].mutex);
    uac = gUAControl->stream[type].uac;
    gUAControl->stream[type].uac = NULL;
    pthread_mutex_unlock(&gUAControl->stream[type].mutex);

    if (uac != NULL) {
        uac->stop();
        uac->waitUntilDone();
        delete uac;
    }
}
