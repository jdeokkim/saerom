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

#include <string.h>
#include <sigar.h>

#define CURLV_IMPLEMENTATION
#include <curlv.h>

#include "saerom.h"

/* | `bot` 모듈 매크로 정의... | */

#define CONFIG_PATH "res/config.json"

/* | `bot` 모듈 상수 및 변수... | */

/* Discord 봇의 명령어 목록. */
static const struct sr_command commands[] = {
    {
        .name = "info",
        .on_create = create_info_command,
        .on_release = release_info_command,
        .on_run = run_info_command
    },
    {
        .name = "krd",
        .on_create = create_krdict_command,
        .on_release = release_krdict_command,
        .on_run = run_krdict_command
    },
    {
        .name = "msg",
        .on_create = create_msg_command,
        .on_release = release_msg_command,
        .on_run = run_msg_command
    },
    {
        .name = "ppg",
        .on_create = create_papago_command,
        .on_release = release_papago_command,
        .on_run = run_papago_command
    },
    {
        .name = "stop",
        .on_create = create_stop_command,
        .on_release = release_stop_command,
        .on_run = run_stop_command
    }
};

/* `CURLV` 인터페이스. */
static CURLV *curlv;

/* `sigar` 인터페이스. */
static sigar_t *sigar;

/* Discord 봇의 클라이언트 객체. */
static struct discord *client;

/* Discord 봇의 시작 시간. */
static uint64_t timestamp;

/* | `bot` 모듈 함수... | */

/* Discord 봇이 대기 중일 때 주기적으로 호출된다. */
static void on_idle(struct discord *client);

/* Discord 봇의 클라이언트가 준비되었을 때 호출된다. */
static void on_ready(
    struct discord *client, 
    const struct discord_ready *event
);

/* Discord 봇이 명령어 처리를 요청받았을 때 호출된다. */
static void on_interaction_create(
    struct discord *client,
    const struct discord_interaction *event
);

/* Discord 봇의 추가 정보를 초기화한다. */
static void sr_core_init(void);

/* Discord 봇의 추가 정보에 할당된 메모리를 해제한다. */
static void sr_core_cleanup(void);

/* 표준 입력 스트림에서 명령어를 입력받는 스레드를 생성한다. */
static void sr_input_reader_init(void);

/* Discord 봇의 명령어들을 생성한다. */
static void create_commands(struct discord *client);

/* Discord 봇의 명령어들에 할당된 메모리를 해제한다. */
static void release_commands(struct discord *client);

/* Discord 봇을 초기화한다. */
void sr_bot_init(int argc, char *argv[]) {
    ccord_global_init();

    client = discord_config_init((argc > 1) ? argv[1] : CONFIG_PATH);

    sr_core_init();

    discord_set_on_idle(client, on_idle);
    discord_set_on_ready(client, on_ready);
    discord_set_on_interaction_create(client, on_interaction_create);

    create_commands(client);
}

/* Discord 봇에 할당된 메모리를 해제한다. */
void sr_bot_cleanup(void) {
    release_commands(client);

    sr_core_cleanup();
    
    log_info("[SAEROM] Cleaning up global resources");

    discord_cleanup(client);

    ccord_global_cleanup();
}

/* Discord 봇을 실행한다. */
void sr_bot_run(void) {
    discord_run(client);
}

/* Discord 봇의 클라이언트 객체를 반환한다. */
struct discord *get_client(void) {
    return client;
}

/* `CURLV` 인터페이스를 반환한다. */
void *get_curlv(void) {
    return (void *) curlv;
}

/* Discord 봇의 명령어 목록을 반환한다. */
const struct sr_command *get_commands(int *len) {
    if (len != NULL) *len = sizeof(commands) / sizeof(*commands);

    return commands;
}

/* Discord 봇의 CPU 사용량 (단위: 퍼센트)을 반환한다. */
double get_cpu_usage(void) {
    sigar_proc_cpu_t cpu;

    sigar_proc_cpu_get(
        sigar, 
        sigar_pid_get(sigar), 
        &cpu
    );

    return cpu.percent;
}

/* Discord 봇의 메모리 사용량 (단위: 메가바이트)을 반환한다. */
double get_ram_usage(void) {
    static const double DENOMINATOR = 1.0 / (1024.0 * 1024.0);

    sigar_proc_mem_t mem;

    sigar_proc_mem_get(
        sigar, 
        sigar_pid_get(sigar), 
        &mem
    );

    return mem.resident * DENOMINATOR;
}

/* Discord 봇의 작동 시간 (단위: 밀리초)을 반환한다. */
uint64_t get_uptime(void) {
    return discord_timestamp(client) - timestamp;
}

/* Discord 봇이 대기 중일 때 주기적으로 호출된다. */
static void on_idle(struct discord *client) {
    if (!(sr_config_get_status_flags() & SR_STATUS_RUNNING)) {
        sr_config_set_status_flags(0);

        log_info("[SAEROM] Shutting down the bot");

        sr_bot_cleanup();

        exit(EXIT_SUCCESS);
    }
    
    curlv_read_requests(curlv);

    // CPU 사용량 최적화
    cog_sleep_us(1);
}

/* Discord 봇의 클라이언트가 준비되었을 때 호출된다. */
static void on_ready(
    struct discord *client, 
    const struct discord_ready *event
) {
    log_info(
        "[SAEROM] Connected to Discord as %s#%s (in %d server(s))", 
        event->user->username, 
        event->user->discriminator,
        event->guilds->size
    );

    /*
        "Bots are only able to send `name`, `type`, and optionally `url`."

        - https://discord.com/developers/docs/topics/gateway#activity-object-activity-structure
    */

    struct discord_activity activities[] = {
        {
            .name = "/info (" APPLICATION_VERSION ")",
            .type = DISCORD_ACTIVITY_GAME,
        },
    };

    timestamp = discord_timestamp(client);

    struct discord_presence_update status = {
        .activities = &(struct discord_activities) {
            .size = sizeof(activities) / sizeof(*activities),
            .array = activities,
        },
        .status = "online",
        .afk = false,
        .since = timestamp
    };

    discord_update_presence(client, &status);
}

/* Discord 봇이 명령어 처리를 요청받았을 때 호출된다. */
static void on_interaction_create(
    struct discord *client,
    const struct discord_interaction *event
) {
    /*
        "An Interaction is the message that your application receives when a user 
        uses an application command or a message component. For Slash Commands, 
        it includes the values that the user submitted."

        - https://discord.com/developers/docs/interactions/receiving-and-responding
    */

    if (event->data == NULL) return;

    const struct discord_user *user = (event->member != NULL)
        ? event->member->user
        : event->user;

    const char *context = NULL;

    switch (event->type) {
        case DISCORD_INTERACTION_APPLICATION_COMMAND:
            context = event->data->name;

            log_info(
                "[SAEROM] Handling command `/%s` executed by %s#%s",
                context,
                user->username,
                user->discriminator
            );

            for (int i = 0; i < sizeof(commands) / sizeof(*commands); i++)
                if (streq(context, commands[i].name))
                    commands[i].on_run(client, event);

            break;

        case DISCORD_INTERACTION_MESSAGE_COMPONENT:
            context = event->data->custom_id;

            log_info(
                "[SAEROM] Handling component interaction `#%s` invoked by %s#%s",
                context,
                user->username,
                user->discriminator
            );

            for (int i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
                const char *name = commands[i].name;

                if (strncmp(context, name, strlen(name)) == 0) 
                    commands[i].on_run(client, event);
            }

            break;
    }
}

/* Discord 봇의 추가 정보를 초기화한다. */
static void sr_core_init(void) {
    if (client == NULL) return;

    curlv = curlv_init();

    sigar_open(&sigar);

    sr_config_init();
    sr_input_reader_init();
}

/* Discord 봇의 추가 정보에 할당된 메모리를 해제한다. */
static void sr_core_cleanup(void) {
    if (client == NULL) return;
    
    sr_config_cleanup();

    curlv_cleanup(curlv);

    sigar_close(sigar);
}

/* 표준 입력 스트림에서 명령어를 입력받는 스레드를 생성한다. */
static void sr_input_reader_init(void) {
    pthread_attr_t attributes;

    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

    pthread_t handle;

    pthread_create(&handle, &attributes, &read_input, client);

    pthread_attr_destroy(&attributes);
}

/* Discord 봇의 명령어들을 생성한다. */
static void create_commands(struct discord *client) {
    const int commands_len = sizeof(commands) / sizeof(*commands);

    const u64bitmask module_flags = sr_config_get_module_flags();

    log_info("[SAEROM] Creating %d global application command(s)", commands_len);

    for (int i = 0; i < commands_len; i++) {
        if (streq(commands[i].name, "krd")
            && !(module_flags & SR_MODULE_KRDICT)) continue;

        if (streq(commands[i].name, "ppg")
            && !(module_flags & SR_MODULE_PAPAGO)) continue;

        if (commands[i].on_create != NULL)
            commands[i].on_create(client);
    }
}

/* Discord 봇의 명령어들에 할당된 메모리를 해제한다. */
static void release_commands(struct discord *client) {
    const int commands_len = sizeof(commands) / sizeof(*commands);

    const u64bitmask module_flags = sr_config_get_module_flags();

    log_info("[SAEROM] Releasing %d global application command(s)", commands_len);

    for (int i = 0; i < commands_len; i++) {
        if (streq(commands[i].name, "krd")
            && !(module_flags & SR_MODULE_KRDICT)) continue;

        if (streq(commands[i].name, "ppg")
            && !(module_flags & SR_MODULE_PAPAGO)) continue;

        if (commands[i].on_release != NULL)
            commands[i].on_release(client);
    }
}