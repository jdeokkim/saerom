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
#include <string.h>

#include <pthread.h>

#include "saerom.h"

/* | `input` 모듈 매크로 정의... | */

#define MAX_COMMAND_SIZE (MAX_STRING_SIZE + DISCORD_MAX_MESSAGE_LEN)

/* | `input` 모듈 함수... | */

/* 표준 입력 스트림 (`stdin`)에서 명령어를 입력받는다. */
void *read_input(void *arg) {
    struct discord *client = arg;
    struct sr_command *commands = NULL;

    char context[MAX_COMMAND_SIZE];
    
    int commands_len = 0;

    commands = (struct sr_command *) get_commands(&commands_len);

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