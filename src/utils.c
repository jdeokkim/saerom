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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <json.h>

#include "saerom.h"

/* | `utils` 모듈 매크로 정의... | */

#define DISCORD_MAGIC_NUMBER  5

/* | `utils` 모듈 함수... | */

/* 주어진 사용자의 프로필 사진 URL을 반환한다. */
const char *get_avatar_url(const struct discord_user *user) {
    static char buffer[MAX_STRING_SIZE];

    if (user->avatar != NULL) {
        snprintf(
            buffer, 
            MAX_STRING_SIZE,
            "https://cdn.discordapp.com/avatars/%"SCNu64"/%s.webp",
            user->id,
            user->avatar
        );
    } else {
        snprintf(
            buffer, 
            MAX_STRING_SIZE,
            "https://cdn.discordapp.com/embed/avatars/%"SCNu64".png",
            strtoul(user->discriminator, NULL, 10) % DISCORD_MAGIC_NUMBER
        );
    }

    return buffer;
}

/* 표준 입력 스트림 (`stdin`)에서 명령어를 입력받는다. */
void *read_input(void *arg) {
    struct discord *client = arg;
    struct sr_command *commands = NULL;

    char context[DISCORD_MAX_MESSAGE_LEN];
    
    int commands_len = 0;

    commands = (struct sr_command *) sr_get_commands(&commands_len);

    for (;;) {
        memset(context, 0, sizeof(context));

        if (fgets(context, sizeof(context), stdin)) ;

        context[strcspn(context, "\r\n")] = 0;

        if (*context == '\0') continue;

        bool run_success = false;

        for (int i = 0; i < commands_len; i++) {
            if (streq(context, commands[i].name)) {
                commands[i].on_run(client, NULL);

                run_success = true;
            }
        }

        if (!run_success) 
            log_error("[SAEROM] Command not found: `/%s`", context);
    }

    pthread_exit(NULL);
}

/* 두 문자열의 내용이 서로 같은지 확인한다. */
bool streq(const char *s1, const char *s2) {
    return strncmp(s1, s2, MAX_STRING_SIZE) == 0;
}

/* UTF-8로 인코딩된 문자열의 길이를 반환한다. */
size_t utf8len(const char *str) {
    size_t result = 0;

    // 첫 번째 바이트만 확인한다.
    for (const char *c = str; *c; c++) {
        if ((*c & 0xf8) == 0xf0) c += 3;
        else if ((*c & 0xf0) == 0xe0) c += 2;
        else if ((*c & 0xe0) == 0xc0) c += 1;
        else ;
        
        result++;
    }

    return result;
}