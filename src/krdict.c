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

#include <yxml.h>

#include <saerom.h>

/* | `krdict` 모듈 매크로 정의... | */

#define MAX_EXAMPLE_COUNT     10
#define MAX_ORDER_COUNT       7

/* | `krdict` 모듈 자료형 정의... | */

/* `/krd` 명령어의 조건 플래그를 나타내는 열거형. */
enum krdict_flag {
    KRD_FLAG_PART_EXAM  = (1 << 0),
    KRD_FLAG_TRANSLATED = (1 << 1)
};

/* `/krd` 명령어의 개별 검색 결과를 나타내는 구조체. */
struct krdict_item {
    char word[MAX_STRING_SIZE];
    char origin[MAX_STRING_SIZE];
    char pos[MAX_STRING_SIZE];
    char link[MAX_STRING_SIZE];
    char dfn[2 * MAX_STRING_SIZE];
    char exam[2 * MAX_STRING_SIZE];
};

/* | `krdict` 모듈 상수 및 변수... | */

/* `/krd` 명령어의 검색 대상 목록. */
static struct discord_application_command_option_choice choices[] = {
    { .name = "Definitions", .value = "\"word\"" },
    { .name = "Examples",    .value = "\"exam\"" }
};

/* `/krd` 명령어의 매개 변수. */
static struct discord_application_command_option options[] = {
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "query",
        .description = "The text you're looking up",
        .required = true
    },
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "part",
        .description = "Which part of each entry to be shown (if 'Examples' is chosen, it will"
                       " NOT be translated)",
        .choices = &(struct discord_application_command_option_choices) {
            .size = sizeof(choices) / sizeof(*choices),
            .array = choices
        }
    },
    {
        .type = DISCORD_APPLICATION_OPTION_BOOLEAN,
        .name = "translated",
        .description = "Whether to translate the results to English or not"
    }
};

/* `/krd` 명령어에 대한 정보. */
static struct discord_create_global_application_command params = {
    .name = "krd",
    .description = "Search the given text in the dictionaries published by the National Institute"
                   " of Korean Language",
    .default_permission = true,
    .options = &(struct discord_application_command_options) {
        .size = sizeof(options) / sizeof(*options),
        .array = options
    }
};

/* | `krdict` 모듈 함수... | */

/* 개인 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_message_success(
    struct discord *client, 
    struct discord_response *resp,
    const struct discord_message *msg
);

/* 개인 메시지 전송에 실패했을 때 호출되는 함수. */
static void on_message_failure(
    struct discord *client, 
    struct discord_response *resp
);

/* 개인 메시지 전송이 끝났을 때 호출되는 함수. */
static void on_message_complete(struct discord *client, void *data);

/* 컴포넌트와의 상호 작용 시에 호출되는 함수. */
static void on_component_interaction(
    struct discord *client,
    const struct discord_interaction *event
);

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data);

/* `/krd` 명령어를 생성한다. */
void sr_command_krdict_init(struct discord *client) {
    discord_create_global_application_command(
        client,
        sr_config_get_application_id(),
        &params,
        NULL
    );
}

/* `/krd` 명령어에 할당된 메모리를 해제한다. */
void sr_command_krdict_cleanup(struct discord *client) {
    /* no-op */
}

/* `/krd` 명령어를 실행한다. */
void sr_command_krdict_run(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event == NULL) {
        log_error("[SAEROM] This command cannot be run in the console.");

        return;
    } else if (event->type == DISCORD_INTERACTION_MESSAGE_COMPONENT) {
        on_component_interaction(client, event);

        return;
    }

    char *query = "", *part = "word", *translated = "true";

    for (int i = 0; i < event->data->options->size; i++) {
        char *name = event->data->options->array[i].name;
        char *value = event->data->options->array[i].value;

        if (streq(name, "query")) query = value;
        else if (streq(name, "part")) part = value;
        else if (streq(name, "translated")) translated = value;
    }

    sr_command_krdict_create_request(client, event, on_response, query, part, translated);

    discord_create_interaction_response(
        client, 
        event->id, 
        event->token, 
        &(struct discord_interaction_response) {
            .type = DISCORD_INTERACTION_DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE
        },
        NULL
    );
}

/* `/krd` 명령어 명령어의 오픈 API 요청을 생성한다. */
void sr_command_krdict_create_request(
    struct discord *client,
    const struct discord_interaction *event,
    curlv_read_callback on_response,
    const char *query,
    const char *part,
    const char *translated
) {
    CURLV_REQ request = { .callback = on_response };

    request.easy = curl_easy_init();

    char buffer[DISCORD_MAX_MESSAGE_LEN] = "";

    // 우리말샘 오픈 API는 다국어 번역을 지원하지 않는다.
    if (streq(part, "exam") || streq(translated, "false")) {
        snprintf(
            buffer, 
            sizeof(buffer), 
            "key=%s&q=%s&part=%s&advanced=%s",
            sr_config_get_urms_api_key(),
            query,
            part,
            (streq(part, "word")) ? "y" : "n"
        );

        curl_easy_setopt(request.easy, CURLOPT_URL, REQUEST_URL_URMSAEM);
    } else {
        snprintf(
            buffer, 
            sizeof(buffer), 
            "key=%s&q=%s&part=%s&advanced=%s&translated=y&trans_lang=1",
            sr_config_get_krd_api_key(),
            query,
            part,
            (streq(part, "word")) ? "y" : "n"
        );

        curl_easy_setopt(request.easy, CURLOPT_URL, REQUEST_URL_KRDICT);
    }

    curl_easy_setopt(request.easy, CURLOPT_POSTFIELDSIZE, strlen(buffer));
    curl_easy_setopt(request.easy, CURLOPT_COPYPOSTFIELDS, buffer);
    curl_easy_setopt(request.easy, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(request.easy, CURLOPT_POST, 1);

    struct sr_command_context *context = malloc(sizeof(*context));

    context->event = discord_claim(client, event);

    if (streq(part, "exam")) context->flags |= KRD_FLAG_PART_EXAM;
    if (streq(translated, "true")) context->flags |= KRD_FLAG_TRANSLATED;

    request.user_data = context;

    curlv_create_request(sr_get_curlv(), &request);
}

/* `/krd` 명령어 처리 과정에서 발생한 오류를 처리한다. */
void sr_command_krdict_handle_error(
    struct sr_command_context *context, 
    const char *code
) {
     if (context == NULL || code == NULL) return; 

    log_warn(
        "[SAEROM] An error (`%s`) has occured while processing the request",
        code
    );

    struct discord *client = sr_get_client();

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .description = "An unknown error has occured while processing "
                           "your request.",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
            }
        }
    };

    discord_edit_original_interaction_response(
        client,
        sr_config_get_application_id(),
        context->event->token,
        &(struct discord_edit_original_interaction_response) {
            .embeds = &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            }
        },
        NULL
    );

    discord_unclaim(client, context->event);

    free(context);
}

/* `/krd` 명령어의 응답 데이터를 가공한다. */
int sr_command_krdict_parse_data(
    CURLV_STR xml, 
    char *buffer, 
    size_t size,
    u64bitmask flags
) {
    if (xml.len == 0 || buffer == NULL) return 0;

    yxml_t *parser = malloc(sizeof(*parser) + xml.len);

    yxml_init(parser, parser + 1, xml.len);

    struct krdict_item item = { .word = "" };

    char content[DISCORD_MAX_MESSAGE_LEN] = "";

    char *elem, *temp, *ptr;

    int len, num, total, order = 1;

    for (char *doc = xml.str; *doc; doc++) {
        yxml_ret_t result = yxml_parse(parser, *doc);

        if (result < 0) {
            strncpy(buffer, "-1", size);

            free(parser);

            return -1;
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

                if (streq(parser->elem, "error")) {
                    strncpy(buffer, content, size);

                    free(parser);

                    return -1;
                }

                if (streq(parser->elem, "channel")) {
                    if (streq(elem, "total")) {
                        total = atoi(content);

                        if (total <= 0) break;
                    } else if (streq(elem, "num")) {
                        num = atoi(content);

                        if (total > num) total = num;
                    }

                    break;
                }

                // 검색 결과로 어휘를 출력할 경우?
                if (!(flags & KRD_FLAG_PART_EXAM)) {
                    if (order > MAX_ORDER_COUNT) break;

                    if (streq(elem, "word"))
                        strncpy(item.word, content, sizeof(item.word));
                    else if (streq(elem, "pos"))
                        strncpy(item.pos, content, sizeof(item.pos));
                    else if (streq(elem, "link"))
                        strncpy(item.link, content, sizeof(item.link));
                    else if (streq(elem, "origin"))
                        strncpy(item.origin, content, sizeof(item.origin));
                    else if (streq(elem, "sense_order") || streq(elem, "sense_no"))
                        order = atoi(content);
                    else if (streq(elem, "definition") || streq(elem, "trans_word"))
                        strncpy(item.dfn, content, sizeof(item.dfn));
                    else if (streq(elem, "trans_dfn"))
                        strncpy(item.exam, content, sizeof(item.exam));
                    else if (streq(elem, "sense")) {
                        char *item_pos = strlen(item.pos) > 0 ? item.pos : "?";

                        len = strlen(buffer);

                        if (strlen(item.origin) > 0) {
                            snprintf(
                                buffer + len, 
                                size - len,
                                "[**%s (%s) 「%s」**](%s)\n\n",
                                item.word,
                                item.origin,
                                item_pos,
                                item.link
                            );
                        } else {
                            snprintf(
                                buffer + len, 
                                size - len,
                                "[**%s 「%s」**](%s)\n\n",
                                item.word,
                                item_pos,
                                item.link
                            );
                        }

                        len = strlen(buffer);

                        if (flags & KRD_FLAG_TRANSLATED) {
                            snprintf(
                                buffer + len, 
                                size - len,
                                "**%d. %s**\n"
                                "- %s\n\n",
                                order,
                                item.dfn,
                                item.exam
                            );
                        } else {
                            snprintf(
                                buffer + len, 
                                size - len,
                                "- %s\n\n",
                                item.dfn
                            );
                        }
                    }
                } else {
                    if (order > MAX_EXAMPLE_COUNT) break;

                    if (streq(elem, "word"))
                        strncpy(item.word, content, sizeof(item.word));
                    else if (streq(elem, "example"))
                        strncpy(item.exam, content, sizeof(item.exam));
                    else if (streq(elem, "link")) {
                        strncpy(item.link, content, sizeof(item.link));

                        len = strlen(buffer);

                        snprintf(
                            buffer + len, 
                            size - len,
                            "%d. %s\n\n",
                            order,
                            item.exam
                        );

                        order++;
                    }
                }

                elem = parser->elem;

                break;
        }

        if (total <= 0) break;
    }

    free(parser);

    return total;
}

/* 개인 메시지 전송에 성공했을 때 호출되는 함수. */
static void on_message_success(
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
static void on_message_failure(
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

/* 개인 메시지 전송이 끝났을 때 호출되는 함수. */
static void on_message_complete(struct discord *client, void *data) {
    discord_unclaim(client, (struct discord_interaction *) data);
}

/* 컴포넌트와의 상호 작용 시에 호출되는 함수. */
static void on_component_interaction(
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
    
    discord_create_message(
        client, 
        ret_channel.id,
        &(struct discord_create_message) {
            .embeds = event->message->embeds
        },
        &(struct discord_ret_message) {
            .data = (void *) discord_claim(client, event),
            .cleanup = on_message_complete,
            .done = on_message_success,
            .fail = on_message_failure
        }
    );
}

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data) {
    if (res.str == NULL || user_data == NULL) return;

    struct sr_command_context *context = (struct sr_command_context *) user_data;

    bool request_url_check = (context->flags & KRD_FLAG_PART_EXAM)
        || !(context->flags & KRD_FLAG_TRANSLATED);

    log_info(
        "[SAEROM] Received %ld bytes from \"%s\"", 
        res.len,
        request_url_check 
            ? REQUEST_URL_URMSAEM 
            : REQUEST_URL_KRDICT
    );

    char buffer[DISCORD_EMBED_DESCRIPTION_LEN] = "";

    int total = sr_command_krdict_parse_data(
        res, 
        buffer, 
        sizeof(buffer), 
        context->flags
    );

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

    struct discord *client = sr_get_client();

    struct discord_embed embeds[] = {
        {
            .title = "Results",
            .timestamp = discord_timestamp(client),
            .footer = &(struct discord_embed_footer) {
                .text = "🗒️"
            }
        }
    };

    if (total < 0) {
        sr_command_krdict_handle_error(context, buffer);

        return;
    } else if (total == 0) embeds[0].description = "No results found.";
    else embeds[0].description = buffer;

    discord_edit_original_interaction_response(
        client,
        sr_config_get_application_id(),
        context->event->token,
        &(struct discord_edit_original_interaction_response) {
            .components = &(struct discord_components){
                .size = (total > 0) ? sizeof(action_rows) / sizeof(*action_rows) : 0,
                .array = action_rows
            },
            .embeds = &(struct discord_embeds) {
                .size = sizeof(embeds) / sizeof(*embeds),
                .array = embeds
            }
        },
        NULL
    );

    discord_unclaim(client, context->event);

    free(context);
}