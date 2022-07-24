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

#include <json.h>

#include "saerom.h"

/* | `utils` 모듈 매크로 정의... | */

#define DISCORD_MAGIC_NUMBER 5

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

/* 주어진 경로에 위치한 파일의 내용을 반환한다. */
char *get_file_contents(const char *path) {
    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        log_warn("[SAEROM] Unable to open file \"%s\"", path);

        return NULL;
    }

    char *buffer = malloc(MAX_FILE_SIZE);

    if (buffer == NULL) {
        log_warn("[SAEROM] Unable to allocate memory for file \"%s\"", path);

        return NULL;
    }

    size_t file_size = fread(buffer, sizeof(*buffer), MAX_FILE_SIZE, fp);

    char *new_buffer = realloc(buffer, file_size);

    if (new_buffer != NULL) {
        new_buffer[file_size] = 0;

        buffer = new_buffer;
    }

    fclose(fp);

    return buffer;
}

/* 두 문자열의 내용이 서로 같은지 확인한다. */
bool streq(const char *s1, const char *s2) {
    return strncmp(s1, s2, MAX_STRING_SIZE) == 0;
}