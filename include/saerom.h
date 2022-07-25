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
#define APPLICATION_VERSION      "v0.2.0"
#define APPLICATION_DESCRIPTION  "A C99 Discord bot for Korean learning servers."
#define APPLICATION_PROJECT_URL  "https://github.com/jdeokkim/saerom"

#define DEVELOPER_NAME           "Jaedeok Kim"
#define DEVELOPER_ICON_URL       "https://avatars.githubusercontent.com/u/28700668"

#define MAX_FILE_SIZE             1024
#define MAX_STRING_SIZE           256

/* | 자료형 정의... | */

/* Discord 봇의 환경 설정을 나타내는 구조체. */
struct sr_config {
    unsigned char flags;
    struct {
        char api_key[MAX_STRING_SIZE];
    } krdict;
    struct {
        char client_id[MAX_STRING_SIZE];
        char client_secret[MAX_STRING_SIZE];
    } papago;
};

/* Discord 봇의 명령어를 나타내는 구조체. */
struct sr_command {
    const char *name;
    void (*on_create)(struct discord *client);
    void (*on_release)(struct discord *client);
    void (*on_run)(
        struct discord *client,
        const struct discord_interaction *event
    );
};

/* Discord 봇의 환경 설정 플래그를 나타내는 열거형. */
enum sr_flag {
    SR_FLAG_KRDICT = (1 << 0),
    SR_FLAG_PAPAGO = (1 << 1)
};

/* | `bot` 모듈 함수... | */

/* Discord 봇을 초기화한다. */
void init_bot(int argc, char *argv[]);

/* Discord 봇을 실행한다. */
void run_bot(void);

/* Discord 봇에 할당된 메모리를 해제한다. */
void deinit_bot(void);

/* `CURLV` 인터페이스를 반환한다. */
void *get_curlv(void);

/* Discord 봇의 애플리케이션 고유 번호를 반환한다. */
u64snowflake get_application_id(void);

/* Discord 봇 관리자의 고유 번호를 반환한다. */
u64snowflake get_owner_id(void);

/* Discord 봇의 환경 설정 정보를 반환한다. */
struct sr_config *get_sr_config(void);

/* Discord 봇의 명령어 목록을 반환한다. */
const struct sr_command *get_commands(int *len);

/* Discord 봇의 CPU 사용량 (단위: 퍼센트)을 반환한다. */
double get_cpu_usage(void);

/* Discord 봇의 메모리 사용량 (단위: 메가바이트)을 반환한다. */
double get_ram_usage(void);

/* Discord 봇의 작동 시간 (단위: 밀리초)을 반환한다. */
uint64_t get_uptime(void);

/* | `info` 모듈 함수... | */

/* `/info` 명령어를 생성한다. */
void create_info_command(struct discord *client);

/* `/info` 명령어에 할당된 메모리를 해제한다. */
void release_info_command(struct discord *client);

/* `/info` 명령어를 실행한다. */
void run_info_command(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `krdict` 모듈 함수... | */

/* `/krd` 명령어를 생성한다. */
void create_krdict_command(struct discord *client);

/* `/krd` 명령어에 할당된 메모리를 해제한다. */
void release_krdict_command(struct discord *client);

/* `/krd` 명령어를 실행한다. */
void run_krdict_command(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `owner` 모듈 함수... | */

/* `/msg` 명령어를 생성한다. */
void create_msg_command(struct discord *client);

/* `/msg` 명령어에 할당된 메모리를 해제한다. */
void release_msg_command(struct discord *client);

/* `/msg` 명령어를 실행한다. */
void run_msg_command(
    struct discord *client,
    const struct discord_interaction *event
);

/* `/stop` 명령어를 생성한다. */
void create_stop_command(struct discord *client);

/* `/stop` 명령어에 할당된 메모리를 해제한다. */
void release_stop_command(struct discord *client);

/* `/stop` 명령어를 실행한다. */
void run_stop_command(
    struct discord *client,
    const struct discord_interaction *event
);

/* | `papago` 모듈 함수... | */

/* `/ppg` 명령어를 생성한다. */
void create_papago_command(struct discord *client);

/* `/ppg` 모듈 명령어에 할당된 메모리를 해제한다. */
void release_papago_command(struct discord *client);

/* `/ppg` 모듈 명령어를 실행한다. */
void run_papago_command(
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