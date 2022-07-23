/*
    Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
    Copyright (c) 2022, Jaedeok Kim <jdeokkim@protonmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#ifndef CURLV_H
#define CURLV_H

#include <stddef.h>

#include <curl/curl.h>

/* | 자료형 정의... | */

/* 문자열을 나타내는 구조체. */
typedef struct CURLV_STR {
    char *str;   // 문자열의 메모리 주소.
    size_t len;  // 문자열의 길이.
} CURLV_STR;

/* 사용자 요청의 처리가 끝났을 때 호출될 함수.*/
typedef void (*curlv_read_callback)(CURLV_STR res, void *user_data);

/* 사용자의 요청을 나타내는 구조체. */
typedef struct CURLV_REQ {
    CURL *easy;                    // 요청에 사용할 핸들.
    struct curl_slist *header;     // HTTP 요청 헤더.
    curlv_read_callback callback;  // 응답을 받았을 때 호출될 함수. 
    void *user_data;               // 사용자 정의 데이터.
} CURLV_REQ;

/* `CURLV` 인터페이스를 나타내는 구조체. */
typedef struct CURLV CURLV;

#ifdef __cplusplus
extern "C" {
#endif

/* | 라이브러리 함수... | */

/* `CURLV` 인터페이스를 초기화한다. */
CURLV *curlv_init(void);

/* `CURLV` 인터페이스에 할당된 메모리를 해제한다. */
void curlv_cleanup(CURLV *cv);

/* `CURLV` 인터페이스에 새로운 요청을 추가한다. */
void curlv_create_request(CURLV *cv, const CURLV_REQ *req);

/* `CURLV` 인터페이스에서 가장 먼저 추가된 요청을 제거한다. */
void curlv_remove_request(CURLV *cv);

/* `CURLV` 인터페이스에 추가된 모든 요청들을 처리한다. */
void curlv_read_requests(CURLV *cv);

#ifdef __cplusplus
}
#endif

#endif // `CURLV_H`

#ifdef CURLV_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

/* | 매크로 정의... | */

typedef void *QUEUE[2];

#define QUEUE(type) QUEUE

#define QUEUE_NEXT(q)       (*(QUEUE **) &((*(q))[0]))
#define QUEUE_PREV(q)       (*(QUEUE **) &((*(q))[1]))
#define QUEUE_PREV_NEXT(q)  (QUEUE_NEXT(QUEUE_PREV(q)))
#define QUEUE_NEXT_PREV(q)  (QUEUE_PREV(QUEUE_NEXT(q)))

#define QUEUE_DATA(ptr, type, field)                          \
  ((type *) ((char *) (ptr) - offsetof(type, field)))

#define QUEUE_FOREACH(q, h)                                   \
  for ((q) = QUEUE_NEXT(h); (q) != (h); (q) = QUEUE_NEXT(q))

#define QUEUE_EMPTY(q)                                        \
  ((const QUEUE *) (q) == (const QUEUE *) QUEUE_NEXT(q))

#define QUEUE_HEAD(q)                                         \
  (QUEUE_NEXT(q))

#define QUEUE_INIT(q)                                         \
  do {                                                        \
    QUEUE_NEXT(q) = (q);                                      \
    QUEUE_PREV(q) = (q);                                      \
  }                                                           \
  while (0)

#define QUEUE_INSERT_TAIL(h, q)                               \
  do {                                                        \
    QUEUE_NEXT(q) = (h);                                      \
    QUEUE_PREV(q) = QUEUE_PREV(h);                            \
    QUEUE_PREV_NEXT(q) = (q);                                 \
    QUEUE_PREV(h) = (q);                                      \
  }                                                           \
  while (0)

#define QUEUE_REMOVE(q)                                       \
  do {                                                        \
    QUEUE_PREV_NEXT(q) = QUEUE_NEXT(q);                       \
    QUEUE_NEXT_PREV(q) = QUEUE_PREV(q);                       \
  }                                                           \
  while (0)

/* | 자료형 정의... | */

/* 요청 큐의 요소를 나타내는 구조체. */
typedef struct CURLV_QE {
    CURLV_REQ request;
    CURLV_STR response;
    QUEUE(_) entry;
} CURLV_QE;

/* `CURLV` 인터페이스를 나타내는 구조체. */
struct CURLV {
    CURLM *multi;
    QUEUE(CURLV_QE) requests;
    pthread_mutex_t lock;
};

/* | 라이브러리 함수... | */

/* `CURLV` 인터페이스의 응답 처리 시에 호출되는 함수. */
static size_t curlv_write_callback(char *ptr, size_t size, size_t num, void *write_data);

/* 요청 큐의 요소를 초기화한다. */
static CURLV_QE *curlv_qe_init(const CURLV_REQ *req);

/* 요청 큐의 요소에 할당된 메모리를 해제한다. */
static void curlv_qe_cleanup(CURLV_QE *qe);

/* `CURLV` 인터페이스를 초기화한다. */
CURLV *curlv_init(void) {
    CURLV *result = calloc(1, sizeof(CURLV));

    QUEUE_INIT(&result->requests);

    result->multi = curl_multi_init();

    pthread_mutex_init(&result->lock, NULL);

    return result;
}

/* `CURLV` 인터페이스에 할당된 메모리를 해제한다. */
void curlv_cleanup(CURLV *cv) {
    if (cv != NULL) {
        while (!QUEUE_EMPTY(&cv->requests))
            curlv_remove_request(cv);

        curl_multi_cleanup(cv->multi);

        pthread_mutex_destroy(&cv->lock);
    }

    free(cv);
}

/* `CURLV` 인터페이스에 새로운 요청을 추가한다. */
void curlv_create_request(CURLV *cv, const CURLV_REQ *req) {
    if (cv == NULL || req == NULL) return;

    CURLV_QE *qe = curlv_qe_init(req);

    curl_easy_setopt(req->easy, CURLOPT_WRITEFUNCTION, curlv_write_callback);
    curl_easy_setopt(req->easy, CURLOPT_WRITEDATA, (void *) &qe->response);
    curl_easy_setopt(req->easy, CURLOPT_HTTPHEADER, req->header);
    curl_easy_setopt(req->easy, CURLOPT_PRIVATE, (void *) qe);

    pthread_mutex_lock(&cv->lock);

    curl_multi_add_handle(cv->multi, req->easy);

    QUEUE_INSERT_TAIL(&cv->requests, &qe->entry);

    pthread_mutex_unlock(&cv->lock);
}

/* `CURLV` 인터페이스에서 가장 먼저 추가된 요청을 제거한다. */
void curlv_remove_request(CURLV *cv) {
    if (cv == NULL) return;

    pthread_mutex_lock(&cv->lock);

    QUEUE(CURLV_QE) *head = QUEUE_HEAD(&cv->requests);

    QUEUE_REMOVE(head);

    CURLV_QE *qe = QUEUE_DATA(head, CURLV_QE, entry);

    curl_multi_remove_handle(cv->multi, qe->request.easy);

    curlv_qe_cleanup(qe);

    pthread_mutex_unlock(&cv->lock);
}

/* `CURLV` 인터페이스에 추가된 모든 요청들을 처리한다. */
void curlv_read_requests(CURLV *cv) {
    if (cv == NULL) return;

    /* https://curl.se/libcurl/c/threadsafe.html */

    pthread_mutex_lock(&cv->lock);

    int still_running;

    do {
        CURLMcode result = curl_multi_perform(cv->multi, &still_running);

        if (result != CURLM_OK) break;
    } while (still_running);

    struct CURLMsg *msg;

    for (;;) {
        int msgs_in_queue;

        msg = curl_multi_info_read(cv->multi, &msgs_in_queue);

        if (msg == NULL) break;

        if (msg->msg == CURLMSG_DONE) {
            CURLV_QE *qe;

            CURLcode result = curl_easy_getinfo(
                msg->easy_handle,
                CURLINFO_PRIVATE,
                &qe
            );

            if (qe->request.callback != NULL)
                qe->request.callback(qe->response, qe->request.user_data);

            QUEUE_REMOVE(&qe->entry);

            curl_multi_remove_handle(cv->multi, qe->request.easy);
            curlv_qe_cleanup(qe);
        }
    }

    pthread_mutex_unlock(&cv->lock);
}

/* `CURLV` 인터페이스의 응답 처리 시에 호출되는 함수. */
static size_t curlv_write_callback(char *ptr, size_t size, size_t num, void *write_data) {
    size_t new_size = size * num;

    CURLV_STR *response = (CURLV_STR *) write_data;

    char *new_ptr = realloc(response->str, response->len + new_size + 1);

    if (new_ptr == NULL) return 0;

    response->str = new_ptr;

    memcpy(&(response->str[response->len]), ptr, new_size);

    response->len += new_size;
    response->str[response->len] = 0;
    
    return new_size;
}

/* 요청 큐의 요소를 초기화한다. */
static CURLV_QE *curlv_qe_init(const CURLV_REQ *req) {
    CURLV_QE *result = calloc(1, sizeof(CURLV_QE));

    result->request = (*req);

    return result;
}

/* 요청 큐의 요소에 할당된 메모리를 해제한다. */
static void curlv_qe_cleanup(CURLV_QE *qe) {
    if (qe != NULL) {
        curl_easy_cleanup(qe->request.easy);
        curl_slist_free_all(qe->request.header);

        free(qe->response.str);
    }

    free(qe);
}

#endif // `CURLV_IMPLEMENTATION`