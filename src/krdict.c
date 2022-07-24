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

#include <curlv.h>
#include <yxml.h>

#include "saerom.h"

#define KRDICT_REQUEST_URL      "https://krdict.korean.go.kr/api/search"

#define KRDICT_MAX_ORDER_COUNT  8

/* | `krdict` 모듈 자료형 정의... | */

/* `krdict` 모듈 명령어 실행 정보를 나타내는 구조체. */
struct krdict_context {
    struct discord *client;
    struct {
        u64snowflake id;
        char *token;
    } event;
    bool translated;
};

/* `krdict` 모듈 명령어의 개별 검색 결과를 나타내는 구조체. */
struct krdict_item {
    char word[MAX_STRING_SIZE];
    char origin[MAX_STRING_SIZE];
    char pos[MAX_STRING_SIZE];
    char link[MAX_STRING_SIZE];
    char entry[DISCORD_EMBED_DESCRIPTION_LEN];
};

/* | `krdict` 모듈 상수 및 변수... | */

/* `krdict` 모듈 명령어의 매개 변수. */
static struct discord_application_command_option options[] = {
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "query",
        .description = "The text you're looking up",
        .required = true
    },
    {
        .type = DISCORD_APPLICATION_OPTION_BOOLEAN,
        .name = "translated",
        .description = "Whether to translate the results to English or not"
    }
};

/* `krdict` 모듈 명령어에 대한 정보. */
static struct discord_create_global_application_command params = {
    .name = "krd",
    .description = "Search the given text in the Basic Korean Dictionary by the National Institute of Korean Language",
    .default_permission = true,
    .options = &(struct discord_application_command_options) {
        .size = sizeof(options) / sizeof(*options),
        .array = options
    }
};

/* | `krdict` 모듈 함수... | */

/* 개인 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_direct_message_success(
    struct discord *client, 
    struct discord_response *resp,
    const struct discord_message *msg
);

/* 개인 메시지 전송에 실패했을 때 호출되는 함수. */
static void on_direct_message_failure(
    struct discord *client, 
    struct discord_response *resp
);

/* 컴포넌트와의 상호 작용 시에 호출되는 함수. */
static void on_interaction(
    struct discord *client,
    const struct discord_interaction *event
);

/* 컴포넌트와의 상호 작용이 끝났을 때 호출되는 함수. */
static void on_interaction_complete(
    struct discord *client, 
    void *data
);

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data);

/* 응답 결과로 받은 오류 메시지를 처리한다. */
static void handle_error(struct krdict_context *context, const char *code);

/* `krdict` 모듈 명령어를 생성한다. */
void create_krdict_command(struct discord *client) {
    discord_create_global_application_command(
        client,
        APPLICATION_ID,
        &params,
        NULL
    );
}

/* `krdict` 모듈 명령어에 할당된 메모리를 해제한다. */
void release_krdict_command(struct discord *client) {
    /* TODO: ... */
}

/* `krdict` 모듈 명령어를 실행한다. */
void run_krdict_command(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event->type == DISCORD_INTERACTION_MESSAGE_COMPONENT) {
        on_interaction(client, event);

        return;
    }

    char *query = "", *translated = "true";

    for (int i = 0; i < event->data->options->size; i++) {
        char *name = event->data->options->array[i].name;
        char *value = event->data->options->array[i].value;

        if (string_equals(name, "query")) query = value;
        else if (string_equals(name, "translated")) translated = value;
    }

    CURLV_REQ request = { .callback = on_response };

    request.easy = curl_easy_init();

    char buffer[DISCORD_MAX_MESSAGE_LEN] = "";

    struct sr_config *config = get_sr_config();

    snprintf(
        buffer, 
        DISCORD_MAX_MESSAGE_LEN, 
        string_equals(translated, "true") 
            ? "key=%s&q=%s&advanced=y&translated=y&trans_lang=1"
            : "key=%s&q=%s&advanced=y",
        config->krdict.api_key,
        query
    );

    curl_easy_setopt(request.easy, CURLOPT_URL, KRDICT_REQUEST_URL);
    curl_easy_setopt(request.easy, CURLOPT_POSTFIELDSIZE, strlen(buffer));
    curl_easy_setopt(request.easy, CURLOPT_COPYPOSTFIELDS, buffer);
    curl_easy_setopt(request.easy, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(request.easy, CURLOPT_POST, 1);

    struct krdict_context *context = malloc(sizeof(struct krdict_context));

    context->client = client;
    context->event.id = event->id;
    context->event.token = malloc(strlen(event->token) + 1);
    context->translated = string_equals(translated, "true");

    strcpy(context->event.token, event->token);

    request.user_data = context;

    curlv_create_request(get_curlv(), &request);

    discord_create_interaction_response(
        context->client, 
        context->event.id, 
        context->event.token, 
        &(struct discord_interaction_response) {
            .type = DISCORD_INTERACTION_DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE
        },
        NULL
    );
}

/* 개인 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_direct_message_success(
    struct discord *client, 
    struct discord_response *resp,
    const struct discord_message *msg
) {
    if (resp->data == NULL) return;

    struct discord_interaction *event = resp->data;

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .description = "Sent you a direct message!",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
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

/* 개인 메시지 전송에 실패했을 때 호출되는 함수. */
static void on_direct_message_failure(
    struct discord *client, 
    struct discord_response *resp
) {
    if (resp->data == NULL) return;
    
    struct discord_interaction *event = resp->data;

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .description = "Failed to send you a direct message. "
                           "Please check your privacy settings!",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
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

/* 컴포넌트와의 상호 작용 시에 호출되는 함수. */
static void on_interaction(
    struct discord *client,
    const struct discord_interaction *event
) {
    struct discord_channel ret_channel = { .id = 0 };

    const struct discord_user *user = (event->member != NULL)
        ? event->member->user
        : event->user;

    CCORDcode result = discord_create_dm(
        client,
        &(struct discord_create_dm) {
            .recipient_id = user->id
        },
        &(struct discord_ret_channel) {
            .sync = &ret_channel
        }
    );

    if (result != CCORD_OK) return;

    discord_channel_cleanup(&ret_channel);

    log_info(
        "[SAEROM] Attempting to send a direct message to %s#%s",
        user->username,
        user->discriminator
    );

    struct discord_interaction *event_clone = malloc(
        sizeof(struct discord_interaction)
    );

    event_clone->id = event->id;
    event_clone->token = malloc(strlen(event->token) + 1);

    strcpy(event_clone->token, event->token);
    
    discord_create_message(
        client, 
        ret_channel.id,
        &(struct discord_create_message) {
            .embeds = event->message->embeds
        },
        &(struct discord_ret_message) {
            .data = event_clone,
            .cleanup = on_interaction_complete,
            .done = on_direct_message_success,
            .fail = on_direct_message_failure
        }
    );
}

/* 컴포넌트와의 상호 작용이 끝났을 때 호출되는 함수. */
static void on_interaction_complete(
    struct discord *client, 
    void *data
) {
    struct discord_interaction *event = data;

    free(event->token);
    free(event);
}

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data) {
    if (res.str == NULL || user_data == NULL) return;

    log_info("[SAEROM] Received %ld bytes from \"%s\"", res.len, KRDICT_REQUEST_URL);

    yxml_t *parser = malloc(sizeof(yxml_t) + res.len);

    yxml_init(parser, parser + 1, res.len);

    struct krdict_context *context = (struct krdict_context *) user_data;
    struct krdict_item item;

    char buffer[DISCORD_EMBED_DESCRIPTION_LEN] = "";
    char content[DISCORD_MAX_MESSAGE_LEN] = "";

    char *elem, *temp, *ptr;

    int len, num, order, total = 1;

    for (char *doc = res.str; *doc; doc++) {
        yxml_ret_t result = yxml_parse(parser, *doc);

        if (result < 0) {
            handle_error(context, "-1");

            free(parser);

            return;
        }

        switch (result) {
            case YXML_ELEMSTART:
                elem = parser->elem;
                ptr = content;

                break;

            case YXML_CONTENT:
                temp = parser->data;

                while (*temp != 0) {
                    if (*temp == '\n' || *temp == '\t') {
                        temp++;

                        continue;
                    }

                    *(ptr++) = *(temp++);
                }

                break;

            case YXML_ELEMEND:
                *ptr = 0;

                if (string_equals(parser->elem, "error")) {
                    handle_error(context, content);

                    free(parser);

                    return;
                }

                if (string_equals(parser->elem, "channel")) {
                    if (string_equals(elem, "total")) {
                        total = atoi(content);

                        if (total <= 0) break;
                    } else if (string_equals(elem, "num")) {
                        num = atoi(content);

                        if (total > num) total = num;
                    } else {
                        continue;
                    }
                }

                if (string_equals(elem, "word")) {
                    strncpy(item.word, content, sizeof(item.word));
                } else if (string_equals(elem, "origin")) {
                    strncpy(item.origin, content, sizeof(item.origin));
                } else if (string_equals(elem, "pos")) {
                    strncpy(item.pos, content, sizeof(item.pos));
                } else if (string_equals(elem, "link")) {
                    strncpy(item.link, content, sizeof(item.link));

                    if (strlen(item.origin) > 0) {
                        snprintf(
                            item.entry, 
                            sizeof(item.entry), 
                            "[**%s (%s) 「%s」**](%s)\n\n",
                            item.word,
                            item.origin,
                            item.pos,
                            item.link
                        );
                    } else {
                        snprintf(
                            item.entry, 
                            sizeof(item.entry), 
                            "[**%s 「%s」**](%s)\n\n",
                            item.word,
                            item.pos,
                            item.link
                        );
                    }

                    strcat(buffer, item.entry);
                } else if (string_equals(elem, "sense_order")) {
                    order = atoi(content);
                } else if (string_equals(elem, "definition")) {
                    if (context->translated) break;

                    len = strlen(buffer);

                    snprintf(
                        buffer + len, 
                        sizeof(buffer) - len,
                        "**%d. %s**\n- ",
                        order,
                        item.word
                    );

                    len = strlen(buffer);

                    snprintf(
                        buffer + len, 
                        sizeof(buffer) - len,
                        "%s\n\n",
                        content
                    );
                } else if (string_equals(elem, "trans_word")) {
                    if (order >= KRDICT_MAX_ORDER_COUNT) break;

                    len = strlen(buffer);

                    snprintf(
                        buffer + len, 
                        sizeof(buffer) - len,
                        "**%d. %s**\n- ",
                        order,
                        content
                    );
                } else if (string_equals(elem, "trans_dfn")) {
                    if (order >= KRDICT_MAX_ORDER_COUNT) break;

                    len = strlen(buffer);

                    snprintf(
                        buffer + len, 
                        sizeof(buffer) - len,
                        "%s\n\n",
                        content
                    );
                }

                elem = parser->elem;

                break;
        }

        if (total <= 0) break;
    }

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .description = "No results found.",
            .timestamp = discord_timestamp(context->client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
            }
        }
    };

    if (total > 0) embeds[0].description = buffer;

    struct discord_component buttons[] = {
        {
            .type = DISCORD_COMPONENT_BUTTON,
            .style = DISCORD_BUTTON_SECONDARY,
            .label = "🔖 Bookmark",
            .custom_id = "krd_btn_uwu"
        }
    };

    struct discord_component action_rows[] = {
        {
            .type = DISCORD_COMPONENT_ACTION_ROW,
            .components = &(struct discord_components){
                .size = sizeof(buttons) / sizeof(*buttons),
                .array = buttons
            }
        },
    };

    discord_edit_original_interaction_response(
        context->client,
        APPLICATION_ID,
        context->event.token,
        &(struct discord_edit_original_interaction_response) {
            .components = &(struct discord_components){
                .size = (total > 0) 
                    ? sizeof(action_rows) / sizeof(*action_rows)
                    : 0,
                .array = action_rows
            },
            .embeds = &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            }
        },
        NULL
    );

    free(context->event.token);
    free(context);

    free(parser);
}

/* 응답 결과로 받은 오류 메시지를 처리한다. */
static void handle_error(struct krdict_context *context, const char *code) {
    if (context == NULL || code == NULL) return; 

    log_warn(
        "[SAEROM] An error (%s) has occured while processing the request",
        code
    );

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .description = "An unknown error has occured while processing "
                           "your request.",
            .timestamp = discord_timestamp(context->client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
            }
        }
    };

    discord_edit_original_interaction_response(
        context->client,
        APPLICATION_ID,
        context->event.token,
        &(struct discord_edit_original_interaction_response) {
            .embeds = &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            }
        },
        NULL
    );

    free(context->event.token);
    free(context);
}