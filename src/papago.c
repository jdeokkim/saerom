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
#include <json.h>

#include "saerom.h"

#define PAPAGO_REQUEST_URL "https://openapi.naver.com/v1/papago/n2mt"

/* | `papago` 모듈 자료형 정의... | */

/* `/ppg` 명령어의 실행 정보를 나타내는 구조체. */
struct papago_context {
    struct discord *client;
    struct {
        u64snowflake id;
        char *token;
    } event;
    char *text;
};

/* | `papago` 모듈 상수 및 변수... | */

/* `/ppg` 명령어의 원본 언어 및 목적 언어 목록.*/
static struct discord_application_command_option_choice languages[] = {
    { .name = "Chinese (Simplified)",  .value = "\"zh-CN\"" },
    { .name = "Chinese (Traditional)", .value = "\"zh-TW\"" },
    { .name = "English",               .value = "\"en\""    },
    { .name = "French",                .value = "\"fr\""    },
    { .name = "German",                .value = "\"de\""    },
    { .name = "Indonesian",            .value = "\"id\""    },
    { .name = "Italian",               .value = "\"it\""    },
    { .name = "Japanese",              .value = "\"ja\""    },
    { .name = "Korean",                .value = "\"ko\""    },
    { .name = "Russian",               .value = "\"ru\""    },
    { .name = "Spanish",               .value = "\"es\""    },
    { .name = "Thai",                  .value = "\"th\""    },
    { .name = "Vietnamese",            .value = "\"vi\""    }
};

/* `/ppg` 명령어의 매개 변수. */
static struct discord_application_command_option options[] = {
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "text",
        .description = "The text to be translated",
        .required = true
    },
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "source",
        .description = "The language to translate the text from",
        .choices = &(struct discord_application_command_option_choices) {
            .size = sizeof(languages) / sizeof(*languages),
            .array = languages
        }
    },
    {
        .type = DISCORD_APPLICATION_OPTION_STRING,
        .name = "target",
        .description = "The language to translate the text to",
        .choices = &(struct discord_application_command_option_choices) {
            .size = sizeof(languages) / sizeof(*languages),
            .array = languages
        }
    }
};

/* `/ppg` 명령어에 대한 정보. */
static struct discord_create_global_application_command params = {
    .name = "ppg",
    .description = "Translate the given text between two languages using NAVER™ Papago NMT API",
    .default_permission = true,
    .options = &(struct discord_application_command_options) {
        .size = sizeof(options) / sizeof(*options),
        .array = options
    }
};

/* | `papago` 모듈 함수... | */

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data);

/* 응답 결과로 받은 오류 메시지를 처리한다. */
static void handle_error(struct papago_context *context, const char *code);

/* `/ppg` 명령어를 생성한다. */
void create_papago_command(struct discord *client) {
    discord_create_global_application_command(
        client,
        sr_config_get_application_id(),
        &params,
        NULL
    );
}

/* `/ppg` 명령어에 할당된 메모리를 해제한다. */
void release_papago_command(struct discord *client) {
    /* no-op */
}

/* `/ppg` 명령어를 실행한다. */
void run_papago_command(
    struct discord *client,
    const struct discord_interaction *event
) {
    if (event == NULL) {
        log_error("[SAEROM] This command cannot be run in the console.");

        return;
    }
    
    char *text = "", *source = "ko", *target = "en";

    for (int i = 0; i < event->data->options->size; i++) {
        char *name = event->data->options->array[i].name;
        char *value = event->data->options->array[i].value;

        if (streq(name, "text")) text = value;
        else if (streq(name, "source")) source = value;
        else if (streq(name, "target")) target = value;
    }

    CURLV_REQ request = { .callback = on_response };

    request.easy = curl_easy_init();

    char buffer[DISCORD_MAX_MESSAGE_LEN] = "";

    snprintf(
        buffer, 
        sizeof(buffer), 
        "source=%s&target=%s&text=%s",
        source, 
        target, 
        text
    );

    curl_easy_setopt(request.easy, CURLOPT_URL, PAPAGO_REQUEST_URL);
    curl_easy_setopt(request.easy, CURLOPT_POSTFIELDSIZE, strlen(buffer));
    curl_easy_setopt(request.easy, CURLOPT_COPYPOSTFIELDS, buffer);
    curl_easy_setopt(request.easy, CURLOPT_POST, 1);

    request.header = curl_slist_append(
        request.header, 
        "Content-Type: application/x-www-form-urlencoded; charset=UTF-8"
    );

    snprintf(
        buffer, 
        sizeof(buffer), 
        "X-Naver-Client-Id: %s",
        sr_config_get_papago_client_id()
    );

    request.header = curl_slist_append(request.header, buffer);

    snprintf(
        buffer, 
        sizeof(buffer), 
        "X-Naver-Client-Secret: %s",
        sr_config_get_papago_client_secret()
    );

    request.header = curl_slist_append(request.header, buffer);

    struct papago_context *context = malloc(sizeof(struct papago_context));

    context->client = client;
    context->event.id = event->id;
    context->event.token = malloc(strlen(event->token) + 1);
    context->text = malloc(strlen(text) + 1);

    strcpy(context->event.token, event->token);
    strcpy(context->text, text);

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

/* 요청 URL에서 응답을 받았을 때 호출되는 함수. */
static void on_response(CURLV_STR res, void *user_data) {
    if (res.str == NULL || user_data == NULL) return;

    log_info("[SAEROM] Received %ld bytes from \"%s\"", res.len, PAPAGO_REQUEST_URL);

    JsonNode *root = json_decode(res.str);

    JsonNode *node = json_find_member(root, "errorCode");

    struct papago_context *context = (struct papago_context *) user_data;
    
    if (node != NULL) {
        handle_error(context, node->string_);

        json_delete(root);

        return;
    }

    node = json_find_member(root, "message");
    node = json_find_member(node, "result");

    struct discord_embed_field fields[2] = {
        [0] = { .value = context->text }
    };

    char source_field_name[MAX_STRING_SIZE] = "";
    char target_field_name[MAX_STRING_SIZE] = "";

    JsonNode *i = NULL;

    json_foreach(i, node) {
        if (streq(i->key, "srcLangType")) {
            snprintf(source_field_name, MAX_STRING_SIZE, "Source (%s)", i->string_);

            fields[0].name = source_field_name;
        } else if (streq(i->key, "tarLangType")) {
            snprintf(target_field_name, MAX_STRING_SIZE, "Target (%s)", i->string_);

            fields[1].name = target_field_name;
        } else if (streq(i->key, "translatedText")) {
            fields[1].value = i->string_;
        }
    }

    struct discord_embed embeds[] = {
        {
            .title = "Translation",
            .timestamp = discord_timestamp(context->client),
            .footer = &(struct discord_embed_footer) {
                .text = "🌏"
            },
            .fields = &(struct discord_embed_fields) {
                .size = sizeof(fields) / sizeof(*fields),
                .array = fields
            }
        }
    };

    discord_edit_original_interaction_response(
        context->client,
        sr_config_get_application_id(),
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
    free(context->text);
    free(context);

    json_delete(root);
}

/* 응답 결과로 받은 오류 메시지를 처리한다. */
static void handle_error(struct papago_context *context, const char *code) {
    if (context == NULL || code == NULL) return;

    log_warn(
        "[SAEROM] An error (`%s`) has occured while processing the request",
        code
    );

    struct discord_embed embeds[] = {
        {
            .title = "Translation",
            .timestamp = discord_timestamp(context->client),
            .footer = &(struct discord_embed_footer) {
                .text = "🌏"
            }
        }
    };
    
    if (streq(code, "024"))
        embeds[0].description = "Invalid client id or client secret given, "
                                "please check your configuration file.";
    else if (streq(code, "N2MT05"))
        embeds[0].description = "Target language must not be the same as the "
                                "source language.";
    else if (streq(code, "N2MT08"))
        embeds[0].description = "The length of the `text` parameter is too long.";
    else
        embeds[0].description = "An unknown error has occured while processing "
                                "your request.";

    discord_edit_original_interaction_response(
        context->client,
        sr_config_get_application_id(),
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
    free(context->text);

    free(context);
}