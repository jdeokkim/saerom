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

#include <time.h>

#include "saerom.h"

/* | `info` 모듈 상수 및 변수... | */

/* `/info` 명령어에 대한 정보. */
static struct discord_create_global_application_command params = {
    .name = "info",
    .description = "Show information about this bot",
    .default_permission = true
};

/* | `info` 모듈 함수... | */

/* `/info` 명령어를 생성한다. */
void create_info_command(struct discord *client) {
    discord_create_global_application_command(
        client,
        sr_config_get_application_id(),
        &params,
        NULL
    );
}

/* `/info` 명령어에 할당된 메모리를 해제한다. */
void release_info_command(struct discord *client) {
    /* no-op */
}

/* `/info` 명령어를 실행한다. */
void run_info_command(
    struct discord *client,
    const struct discord_interaction *event
) {
    char cpu_usage_str[MAX_STRING_SIZE];
    char ram_usage_str[MAX_STRING_SIZE];

    char uptime_str[MAX_STRING_SIZE];
    char ping_str[MAX_STRING_SIZE];
    char flags_str[MAX_STRING_SIZE];
    
    time_t uptime_in_seconds = get_uptime() * 0.001f;

    snprintf(cpu_usage_str, sizeof(cpu_usage_str), "%.1f%%", get_cpu_usage());
    snprintf(ram_usage_str, sizeof(ram_usage_str), "%.1fMB", get_ram_usage());

    strftime(uptime_str, sizeof(uptime_str), "%T", gmtime(&uptime_in_seconds));
    snprintf(ping_str, sizeof(ping_str), "%dms", discord_get_ping(client));
    snprintf(flags_str, sizeof(flags_str), "0x%02" PRIu64 "X", sr_config_get_module_flags());

    if (event == NULL) {
        log_info("[SAEROM] %s: %s", APPLICATION_NAME, APPLICATION_DESCRIPTION);

        log_info(
            "[SAEROM] Version: %s, Uptime: %s, Latency: %s", 
            APPLICATION_VERSION, 
            uptime_str, 
            flags_str
        );

        log_info(
            "[SAEROM] CPU: %s, RAM: %s, FLAGS: %s", 
            cpu_usage_str, 
            ram_usage_str, 
            flags_str
        );

        return;
    }

    struct discord_embed_field fields[] = {
        {
            .name = "Version",
            .value = APPLICATION_VERSION,
            .Inline = true
        },
        {
            .name = "Uptime",
            .value = uptime_str,
            .Inline = true
        },
        {
            .name = "Latency",
            .value = ping_str,
            .Inline = true
        },
        {
            .name = "CPU",
            .value = cpu_usage_str,
            .Inline = true
        },
        {
            .name = "RAM",
            .value = ram_usage_str,
            .Inline = true
        },
        {
            .name = "FLAGS",
            .value = flags_str,
            .Inline = true
        },
    };

    struct discord_embed embeds[] = {
        {
            .title = APPLICATION_NAME,
            .description = APPLICATION_DESCRIPTION,
            .url = APPLICATION_PROJECT_URL,
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "✨"
            },
            .thumbnail = &(struct discord_embed_thumbnail) {
                .url = (char *) get_avatar_url(discord_get_self(client))
            },
            .author = &(struct discord_embed_author) {
                .name = DEVELOPER_NAME,
                .icon_url = DEVELOPER_ICON_URL
            },
            .fields = &(struct discord_embed_fields) {
                .size = sizeof(fields) / sizeof(*fields),
                .array = fields
            }
        }
    };

    struct discord_interaction_response params = {
        .type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE,
        .data = &(struct discord_interaction_callback_data) { 
            .embeds = &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            }
        }
    };

    discord_create_interaction_response(
        client, 
        event->id, 
        event->token, 
        &params, 
        NULL
    );
}