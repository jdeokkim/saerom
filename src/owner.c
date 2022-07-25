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

#include "saerom.h"

/* | `owner` 모듈 함수... | */

/* Discord 봇을 종료한다. */
static void _shutdown(
    struct discord *client, 
    struct discord_response *resp, 
    const struct discord_interaction_response *ret
);

/* `/stop` 명령어를 생성한다. */
void create_stop_command(struct discord *client) {
    static struct discord_create_global_application_command params = {
        .name = "stop",
        .description = "Shut down the bot",
        .default_permission = true
    };

    discord_create_global_application_command(
        client,
        APPLICATION_ID,
        &params,
        NULL
    );
}

/* `/stop` 명령어에 할당된 메모리를 해제한다. */
void release_stop_command(struct discord *client) {
    /* no-op */
}

/* `/stop` 명령어를 실행한다. */
void run_stop_command(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event != NULL) {
        const struct discord_user *user = (event->member != NULL)
            ? event->member->user
            : event->user;

        if (user->id != get_owner_id()) {
            const struct discord_user *self = discord_get_self(client);

            struct discord_embed embeds[] = {
                {
                    .title = APPLICATION_NAME,
                    .description = "This command can only be invoked by the bot owner.",
                    .timestamp = discord_timestamp(client),
                    .footer = &(struct discord_embed_footer) {
                        .text = "⚡"
                    }
                }
            };

            struct discord_interaction_response params = {
                .type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE,
                .data = &(struct discord_interaction_callback_data) { 
                    .flags = DISCORD_MESSAGE_EPHEMERAL,
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

            return;
        }

        struct discord_embed embeds[] = {
            {
                .title = APPLICATION_NAME,
                .description = "Shutting down the bot...",
                .timestamp = discord_timestamp(client),
                .footer = &(struct discord_embed_footer) {
                    .text = "⚡"
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
            &(struct discord_ret_interaction_response) {
                .done = _shutdown
            }
        );

        return;
    }

    _shutdown(NULL, NULL, NULL);
}

/* Discord 봇을 종료한다. */
static void _shutdown(
    struct discord *client, 
    struct discord_response *resp, 
    const struct discord_interaction_response *ret
) {
    log_info("[SAEROM] Shutting down the bot");

    deinit_bot();

    exit(EXIT_SUCCESS);
}