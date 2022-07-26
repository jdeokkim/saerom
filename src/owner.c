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

#include "saerom.h"

/* | `owner` 모듈 함수... | */

/* 채널 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_message_success(
    struct discord *client, 
    struct discord_response *resp,
    const struct discord_message *msg
);

/* 채널 메시지 전송에 실패했을 때 호출되는 함수. */
static void on_message_failure(
    struct discord *client, 
    struct discord_response *resp
);

/* 채널 메시지 전송이 끝났을 때 호출되는 함수. */
static void on_message_complete(struct discord *client, void *data);

/* Discord 봇을 종료한다. */
static void sr_shutdown(
    struct discord *client, 
    struct discord_response *resp, 
    const struct discord_interaction_response *ret
);

/* `/msg` 명령어를 생성한다. */
void sr_command_msg_init(struct discord *client) {
    int integers[] = { DISCORD_CHANNEL_GUILD_TEXT, DISCORD_CHANNEL_DM };

    struct discord_application_command_option options[] = { 
        {
            .type = DISCORD_APPLICATION_OPTION_STRING,
            .name = "message",
            .description = "The message to send to a specific channel",
            .required = true
        },
        {
            .type = DISCORD_APPLICATION_OPTION_CHANNEL,
            .name = "channel",
            .description = "The channel to send the message to",
            .channel_types = &(struct integers) {
                .size = sizeof(integers) / sizeof(*integers),
                .array = integers
            },
            .required = true
        },
        {
            .type = DISCORD_APPLICATION_OPTION_BOOLEAN,
            .name = "embed",
            .description = "Whether to send this message as an embed or not"
        }
    };

    struct discord_create_global_application_command params = {
        .name = "msg",
        .description = "Send a message to a specific channel",
        .default_permission = true,
        .options = &(struct discord_application_command_options) {
            .size = sizeof(options) / sizeof(*options),
            .array = options
        }
    };

    discord_create_global_application_command(
        client,
        sr_config_get_application_id(),
        &params,
        NULL
    );
}

/* `/msg` 명령어에 할당된 메모리를 해제한다. */
void sr_command_msg_cleanup(struct discord *client) {
    /* no-op */
}

/* `/msg` 명령어를 실행한다. */
void sr_command_msg_run(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event == NULL) {
        log_error("[SAEROM] This command cannot be run in the console.");
        
        return;
    }

    const struct discord_user *user = (event->member != NULL)
        ? event->member->user
        : event->user;

    if (user->id != sr_config_get_application_owner_id()) {
        const struct discord_user *self = discord_get_self(client);

        struct discord_embed embeds[] = {
            {
                .description = "This command can only be invoked by the bot owner.",
                .timestamp = discord_timestamp(client),
                .footer = &(struct discord_embed_footer) {
                    .text = "⚡"
                },
                .author = &(struct discord_embed_author) {
                    .name = self->username,
                    .icon_url = (char *) get_avatar_url(self)
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

    char *message = "";

    u64snowflake as_embed = 0, channel_id = 0;

    for (int i = 0; i < event->data->options->size; i++) {
        char *name = event->data->options->array[i].name;
        char *value = event->data->options->array[i].value;

        if (streq(name, "message")) message = value;
        else if (streq(name, "channel")) channel_id = strtoull(value, NULL, 10);
        else if (streq(name, "embed")) as_embed = streq(value, "true");
    }

    const struct discord_user *self = discord_get_self(client);

    struct discord_embed embeds[] = {
        {
            .description = message,
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "⚡"
            },
            .author = &(struct discord_embed_author) {
                .name = self->username,
                .icon_url = (char *) get_avatar_url(self)
            }
        }
    };

    struct discord_interaction *event_clone = malloc(
        sizeof(struct discord_interaction)
    );

    event_clone->id = event->id;
    event_clone->token = malloc(strlen(event->token) + 1);

    strcpy(event_clone->token, event->token);

    discord_create_message(
        client, 
        channel_id,
        &(struct discord_create_message) {
            .content = (!as_embed) ? message : NULL,
            .embeds = (as_embed) 
                    ? &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            } : NULL
        },
        &(struct discord_ret_message) {
            .data = event_clone,
            .cleanup = on_message_complete,
            .done = on_message_success,
            .fail = on_message_failure
        }
    );
}

/* `/stop` 명령어를 생성한다. */
void sr_command_stop_init(struct discord *client) {
    struct discord_create_global_application_command params = {
        .name = "stop",
        .description = "Shut down the bot",
        .default_permission = true
    };

    discord_create_global_application_command(
        client,
        sr_config_get_application_id(),
        &params,
        NULL
    );
}

/* `/stop` 명령어에 할당된 메모리를 해제한다. */
void sr_command_stop_cleanup(struct discord *client) {
    /* no-op */
}

/* (`/stop` 명령어를 실행한다.) */
static void _sr_command_stop_run(
    struct discord *client, 
    struct discord_response *resp, 
    const struct discord_interaction_response *ret
) {
    sr_config_set_status_flags(0);
}

/* `/stop` 명령어를 실행한다. */
void sr_command_stop_run(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event != NULL) {
        const struct discord_user *user = (event->member != NULL)
            ? event->member->user
            : event->user;

        const struct discord_user *self = discord_get_self(client);

        if (user->id != sr_config_get_application_owner_id()) {
            struct discord_embed embeds[] = {
                {
                    .description = "This command can only be invoked by the bot owner.",
                    .timestamp = discord_timestamp(client),
                    .footer = &(struct discord_embed_footer) {
                        .text = "⚡"
                    },
                    .author = &(struct discord_embed_author) {
                        .name = self->username,
                        .icon_url = (char *) get_avatar_url(self)
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
                .description = "Shutting down the bot...",
                .timestamp = discord_timestamp(client),
                .footer = &(struct discord_embed_footer) {
                    .text = "⚡"
                },
                .author = &(struct discord_embed_author) {
                    .name = self->username,
                    .icon_url = (char *) get_avatar_url(self)
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
                .done = _sr_command_stop_run
            }
        );

        return;
    }

    sr_config_set_status_flags(0);
}

/* 채널 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_message_success(
    struct discord *client, 
    struct discord_response *resp,
    const struct discord_message *msg
) {
    if (resp->data == NULL) return;
    
    struct discord_interaction *event = resp->data;

    struct discord_embed embeds[] = {
        {
            .title = APPLICATION_NAME,
            .description = "Sent the message to the specified channel!",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "💌"
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
}

/* 채널 메시지 전송에 실패했을 때 호출되는 함수. */
static void on_message_failure(
    struct discord *client, 
    struct discord_response *resp
) {
    if (resp->data == NULL) return;
    
    struct discord_interaction *event = resp->data;

    struct discord_embed embeds[] = {
        {
            .title = APPLICATION_NAME,
            .description = "Failed to send the message to the specified channel.",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "💌"
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
}

/* 채널 메시지 전송이 끝났을 때 호출되는 함수. */
static void on_message_complete(struct discord *client, void *data) {
    struct discord_interaction *event = data;

    free(event->token);
    free(event);
}