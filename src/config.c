/*
    Copyright (c) 2022 jdeokkim

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "saerom.h"

/* | `config` 모듈 자료형 정의... | */

/* Discord 봇의 환경 설정을 나타내는 구조체. */
struct sr_config {
    u64snowflake application_id;
    u64snowflake application_owner_id;
    struct {
        u64bitmask module;
        u64bitmask status;
    } flags;
    struct {
        char krd_api_key[MAX_STRING_SIZE];
        char urms_api_key[MAX_STRING_SIZE];
    } krdict;
    struct {
        char client_id[MAX_STRING_SIZE];
        char client_secret[MAX_STRING_SIZE];
    } papago;
    pthread_mutex_t lock;
};

/* | `config` 모듈 상수 및 변수... | */

/* Discord 봇의 환경 설정. */
static struct sr_config config;

/* | `config` 모듈 함수... | */

/* (Discord 봇의 환경 설정을 초기화한다.) */
static void _sr_config_init(
    struct discord *client, 
    struct discord_response *resp, 
    const struct discord_application *ret
) {
    pthread_mutex_lock(&config.lock);

    config.application_owner_id = ret->owner->id;

    pthread_mutex_unlock(&config.lock);
}

/* Discord 봇의 환경 설정을 초기화한다. */
void sr_config_init(void) {
    if (get_client() == NULL) return;

    struct discord *client = get_client();

    pthread_mutex_init(&config.lock, NULL);

    struct ccord_szbuf_readonly field = discord_config_get_field(
        client, (char *[2]) { "discord", "application_id" }, 2
    );

    char buffer[MAX_STRING_SIZE] = "";

    strncpy(buffer, field.start, field.size);

    {
        pthread_mutex_lock(&config.lock);

        config.application_id = strtoull(buffer, NULL, 10);

        pthread_mutex_unlock(&config.lock);
    }

    field = discord_config_get_field(
        client, (char *[3]) { "saerom", "krdict", "enable" }, 3
    );

    if (strncmp("true", field.start, field.size) == 0) {
        pthread_mutex_lock(&config.lock);

        config.flags.module |= SR_MODULE_KRDICT;

        field = discord_config_get_field(
            client, (char *[3]) { "saerom", "krdict", "krd_api_key" }, 3
        );

        strncpy(config.krdict.krd_api_key, field.start, field.size);

        field = discord_config_get_field(
            client, (char *[3]) { "saerom", "krdict", "urms_api_key" }, 3
        );

        strncpy(config.krdict.urms_api_key, field.start, field.size);

        pthread_mutex_unlock(&config.lock);
    }

    field = discord_config_get_field(
        client, (char *[3]) { "saerom", "papago", "enable" }, 3
    );

    if (strncmp("true", field.start, field.size) == 0) {
        pthread_mutex_lock(&config.lock);

        config.flags.module |= SR_MODULE_PAPAGO;

        field = discord_config_get_field(
            client, (char *[3]) { "saerom", "papago", "client_id" }, 3
        );

        strncpy(config.papago.client_id, field.start, field.size);

        field = discord_config_get_field(
            client, (char *[3]) { "saerom", "papago", "client_secret" }, 3
        );

        strncpy(config.papago.client_secret, field.start, field.size);

        pthread_mutex_unlock(&config.lock);
    }

    {
        pthread_mutex_lock(&config.lock);

        config.flags.status = SR_STATUS_RUNNING;

        pthread_mutex_unlock(&config.lock);
    }

    discord_get_current_bot_application_information(
        client, 
        &(struct discord_ret_application) {
            .done = _sr_config_init
        }
    );
}

/* Discord 봇의 환경 설정에 할당된 메모리를 해제한다. */
void sr_config_cleanup(void) {
    pthread_mutex_destroy(&config.lock);
}

/* Discord 봇의 애플리케이션 ID를 반환한다. */
u64snowflake sr_config_get_application_id(void) {
    return config.application_id;
}

/* Discord 봇의 애플리케이션 소유자 ID를 반환한다. */
u64snowflake sr_config_get_application_owner_id(void) {
    return config.application_owner_id;
}

/* Discord 봇의 모듈 플래그 데이터를 반환한다. */
u64bitmask sr_config_get_module_flags(void) {
    return config.flags.module;
}

/* Discord 봇의 상태 플래그 데이터를 반환한다. */
u64bitmask sr_config_get_status_flags(void) {
    return config.flags.status;
}

/* Discord 봇의 국립국어원 한국어기초사전 API 키를 반환한다. */
const char *sr_config_get_krd_api_key(void) {
    return config.krdict.krd_api_key;
}

/* Discord 봇의 국립국어원 우리말샘 API 키를 반환한다. */
const char *sr_config_get_urms_api_key(void) {
    return config.krdict.urms_api_key;
}

/* Discord 봇의 NAVER™ 파파고 NMT API 클라이언트 ID를 반환한다. */
const char *sr_config_get_papago_client_id(void) {
    return config.papago.client_id;
}

/* Discord 봇의 NAVER™ 파파고 NMT API 클라이언트 시크릿을 반환한다. */
const char *sr_config_get_papago_client_secret(void) {
    return config.papago.client_secret;
}

/* Discord 봇의 모듈 플래그 데이터를 `flags`로 설정한다. */
void sr_config_set_module_flags(u64bitmask flags) {
    pthread_mutex_lock(&config.lock);

    config.flags.module = flags;

    pthread_mutex_unlock(&config.lock);
}

/* Discord 봇의 상태 플래그 데이터를 `flags`로 설정한다. */
void sr_config_set_status_flags(u64bitmask flags) {
    pthread_mutex_lock(&config.lock);

    config.flags.status = flags;

    pthread_mutex_unlock(&config.lock);
}
