/*
 * Copyright (c) 2015 Yubico AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <assert.h>

#include <security/pam_appl.h>
#include <security/pam_modutil.h>

static struct data {
  int id;
  const char user[255];
  const char otp[255];
} _data[] = {
  {1, "foo", "vvincredibletrerdegkkrkkneieultcjdghrejjbckh"},
  {2, "foo", "vvincredibletrerdegkkrkkneieultcjdghrejjbckh"},
};

static const char *err = "error";

static const struct data *test_get_data(void *id) {
  int i;
  for(i = 0; i < sizeof(_data) / sizeof(struct data); i++) {
    if(id == _data[i].id) {
      return &_data[i];
    }
  }
  return NULL;
}

const char * pam_strerror(pam_handle_t *pamh, int errnum) {
  fprintf(stderr, "in pam_strerror()\n");
  return "error";
}

int pam_set_data(pam_handle_t *pamh, const char *module_data_name, void *data,
    void (*cleanup)(pam_handle_t *pamh, void *data, int error_status)) {
  fprintf(stderr, "in pam_set_data() %s\n", module_data_name);
  return PAM_SUCCESS;
}

int pam_get_user(const pam_handle_t *pamh, const char **user, const char *prompt) {
  fprintf(stderr, "in pam_get_user()\n");
  *user = test_get_data((void*)pamh)->user;
  return PAM_SUCCESS;
}

static int conv_func(int num_msg, const struct pam_message **msg,
    struct pam_response **resp, void *appdata_ptr) {
  fprintf(stderr, "in conv_func()\n");
  if(num_msg != 1) {
    return PAM_CONV_ERR;
  }

  struct pam_response *reply = malloc(sizeof(struct pam_response));
  reply->resp = test_get_data(appdata_ptr)->otp;
  *resp = reply;
  return PAM_SUCCESS;
}

static struct pam_conv pam_conversation = {
  conv_func,
  NULL,
};

int pam_get_item(const pam_handle_t *pamh, int item_type, const void **item) {
  fprintf(stderr, "in pam_get_item() %d\n", item_type);
  if(item_type == 5) {
    pam_conversation.appdata_ptr = pamh;
    *item = &pam_conversation;
  }
  return PAM_SUCCESS;
}

int pam_modutil_drop_priv(pam_handle_t *pamh, struct pam_modutil_privs *p,
    const struct passwd *pw) {
  fprintf(stderr, "in pam_modutil_drop_priv()\n");
  return PAM_SUCCESS;
}

int pam_modutil_regain_priv(pam_handle_t *pamh, struct pam_modutil_privs *p) {
  fprintf(stderr, "in pam_modutil_regain_priv()\n");
  return PAM_SUCCESS;
}

int pam_set_item(pam_handle_t *pamh, int item_type, const void *item) {
  fprintf(stderr, "in pam_set_item()\n");
  return PAM_SUCCESS;
}

static int test_authenticate1(void) {
  char *cfg[] = {
    "id=1",
    "url=http://localhost:8888/wsapi/2/verify?id=%d&otp=%s",
    "authfile=aux/authfile",
    "debug",
  };
  return pam_sm_authenticate(1, 0, sizeof(cfg) / sizeof(char*), cfg);
}

static int test_authenticate2(void) {
  char *cfg[] = {
    "id=1",
    "urllist=http://localhost:8888/wsapi/2/verify",
    "authfile=aux/authfile",
    "debug",
  };
  return pam_sm_authenticate(2, 0, sizeof(cfg) / sizeof(char*), cfg);
}

static pid_t run_mock(const char *port) {
  pid_t pid = fork();
  if(pid == 0) {
    execlp("aux/ykval.pl", port, NULL);
  }
  /* Give the "server" time to settle */
  sleep(1);
  return pid;
}

int main () {
  int ret = 0;
  pid_t child = run_mock("8888");

  if(test_authenticate1() != 0) {
    ret = 1;
    goto out;
  }
  if(test_authenticate2() != 0) {
    ret = 2;
    goto out;
  }

out:
  kill(child, 9);
  printf("killed %d\n", child);
  return ret;
}
