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

#ifndef SAEROM_H
#define SAEROM_H

#include <stdbool.h>

#include <concord/cog-utils.h>
#include <concord/discord.h>
#include <concord/log.h>

/* | 매크로 정의... | */

#define APPLICATION_NAME         "jdeokkim/saerom"
#define APPLICATION_VERSION      "v0.3.0-dev"
#define APPLICATION_DESCRIPTION  "A C99 Discord bot for Korean learning servers."
#define APPLICATION_PROJECT_URL  "https://github.com/jdeokkim/saerom"

#define DEVELOPER_NAME           "Jaedeok Kim"
#define DEVELOPER_ICON_URL       "https://avatars.githubusercontent.com/u/28700668"

#define MAX_FILE_SIZE             1024
#define MAX_STRING_SIZE           256

#define MAX_ORDER_COUNT           8

/* | 자료형 정의... | */

/* Discord 봇의 명령어를 나타내는 구조체. */
struct sr_command {
    const char *name;
    void (*on_init)(struct discord *client);
    void (*on_cleanup)(struct discord *client);
    void (*on_run)(
        struct discord *client,
        const struct discord_interaction *event
    );
};

/* Discord 봇의 모듈 플래그를 나타내는 열거형. */
enum sr_module_flag {
    SR_MODULE_KRDICT = (1 << 0),
    SR_MODULE_PAPAGO = (1 << 1)
};

/* Discord 봇의 상태 플래그를 나타내는 열거형. */
enum sr_status_flag {
    SR_STATUS_RUNNING = (1 << 0)
};

/* | `bot` 모듈 함수... | */

/* Discord 봇을 초기화한다. */
void sr_bot_init(int argc, char *argv[]);

/* Discord 봇에 할당된 메모리를 해제한다. */
void sr_bot_cleanup(void);

/* Discord 봇을 실행한다. */
void sr_bot_run(void);

/* Discord 봇의 클라이언트 객체를 반환한다. */
struct discord *get_client(void);

/* `CURLV` 인터페이스를 반환한다. */
void *get_curlv(void);

/* Discord 봇의 명령어 목록을 반환한다. */
const struct sr_command *get_commands(int *len);

/* Discord 봇의 CPU 사용량 (단위: 퍼센트)을 반환한다. */
double get_cpu_usage(void);

/* Discord 봇의 메모리 사용량 (단위: 메가바이트)을 반환한다. */
double get_ram_usage(void);

/* Discord 봇의 작동 시간 (단위: 밀리초)을 반환한다. */
uint64_t get_uptime(void);

/* | `config` 모듈 함수... | */

/* Discord 봇의 환경 설정을 초기화한다. */
void sr_config_init(void);

/* Discord 봇의 환경 설정에 할당된 메모리를 해제한다. */
void sr_config_cleanup(void);

/* Discord 봇의 애플리케이션 ID를 반환한다. */
u64snowflake sr_config_get_application_id(void);

/* Discord 봇의 애플리케이션 소유자 ID를 반환한다. */
u64snowflake sr_config_get_application_owner_id(void);

/* Discord 봇의 모듈 플래그 데이터를 반환한다. */
u64bitmask sr_config_get_module_flags(void);

/* Discord 봇의 상태 플래그 데이터를 반환한다. */
u64bitmask sr_config_get_status_flags(void);

/* Discord 봇의 국립국어원 한국어기초사전 API 키를 반환한다. */
const char *sr_config_get_krd_api_key(void);

/* Discord 봇의 국립국어원 우리말샘 API 키를 반환한다. */
const char *sr_config_get_urms_api_key(void);

/* Discord 봇의 NAVER™ 파파고 NMT API 클라이언트 ID를 반환한다. */
const char *sr_config_get_papago_client_id(void);

/* Discord 봇의 NAVER™ 파파고 NMT API 클라이언트 시크릿을 반환한다. */
const char *sr_config_get_papago_client_secret(void);

/* Discord 봇의 모듈 플래그 데이터를 `flags`로 설정한다. */
void sr_config_set_module_flags(u64bitmask flags);

/* Discord 봇의 상태 플래그 데이터를 `flags`로 설정한다. */
void sr_config_set_status_flags(u64bitmask flags);

/* | `info` 모듈 함수... | */

/* `/info` 명령어를 생성한다. */
void sr_command_info_init(struct discord *client);

/* `/info` 명령어에 할당된 메모리를 해제한다. */
void sr_command_info_cleanup(struct discord *client);

/* `/info` 명령어를 실행한다. */
void sr_command_info_run(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `krdict` 모듈 함수... | */

/* `/krd` 명령어를 생성한다. */
void sr_command_krdict_init(struct discord *client);

/* `/krd` 명령어에 할당된 메모리를 해제한다. */
void sr_command_krdict_cleanup(struct discord *client);

/* `/krd` 명령어를 실행한다. */
void sr_command_krdict_run(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `owner` 모듈 함수... | */

/* `/msg` 명령어를 생성한다. */
void sr_command_msg_init(struct discord *client);

/* `/msg` 명령어에 할당된 메모리를 해제한다. */
void sr_command_msg_cleanup(struct discord *client);

/* `/msg` 명령어를 실행한다. */
void sr_command_msg_run(
    struct discord *client,
    const struct discord_interaction *event
);

/* `/stop` 명령어를 생성한다. */
void sr_command_stop_init(struct discord *client);

/* `/stop` 명령어에 할당된 메모리를 해제한다. */
void sr_command_stop_cleanup(struct discord *client);

/* `/stop` 명령어를 실행한다. */
void sr_command_stop_run(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `papago` 모듈 함수... | */

/* `/ppg` 명령어를 생성한다. */
void sr_command_papago_init(struct discord *client);

/* `/ppg` 모듈 명령어에 할당된 메모리를 해제한다. */
void sr_command_papago_cleanup(struct discord *client);

/* `/ppg` 모듈 명령어를 실행한다. */
void sr_command_papago_run(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `utils` 모듈 함수... | */

/* 주어진 사용자의 프로필 사진 URL을 반환한다. */
const char *get_avatar_url(const struct discord_user *user);

/* 주어진 경로에 위치한 파일의 내용을 반환한다. */
char *get_file_contents(const char *path);

/* 표준 입력 스트림 (`stdin`)에서 명령어를 입력받는다. */
void *read_input(void *arg);

/* 두 문자열의 내용이 서로 같은지 확인한다. */
bool streq(const char *s1, const char *s2);

#endif