#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "pam.h"

static char guessbuffer[4096];

static int password_validator(int msgcount,
  const struct pam_message **messages, struct pam_response **resp, void *_x) {

    /* Unused: */ (void) _x;

    char *guess = NULL;
    struct pam_response *resp_msg;
    int status = PAM_CONV_ERR;

    for (int i = 0; i < msgcount; i++) {
        if (messages[i]->msg_style != PAM_PROMPT_ECHO_OFF ||
          strncmp(messages[i]->msg, "Password: ", 10)) {
            continue;
        }

        if (!(resp_msg = malloc(sizeof(struct pam_response))) ||
            !(guess = strdup(guessbuffer))) {

            perror("memory allocation failure");
            free(guess);
            free(resp_msg);
        } else {
            resp_msg->resp_retcode = 0;
            resp_msg->resp = guess;
            resp[i] = resp_msg;
            status = PAM_SUCCESS;
        }

        break;
    }

    return status;
}

int pam_password_ok(const char *guess, char *errorbuf, size_t errorbufsize)
{
    pam_handle_t *handle;
    volatile char *k;
    int status;
    struct passwd* pw;

    static struct pam_conv conversation = {
        password_validator,
        NULL,
    };

    char *eom = errorbuf + errorbufsize;

    errno = 0;

    if (!(pw = getpwuid(getuid()))) {
        if (errorbuf && errorbufsize > 0) {
            strncpy(errorbuf, "getpwuid: ", errorbufsize);
            *eom = '\0';
            errorbufsize -= strlen(errorbuf);
        }

        if (errorbuf && errorbufsize > 0) {
            strncat(
                errorbuf,
                errno ? strerror(errno) : "missing password database entry",
                errorbufsize
            );
            *eom = '\0';
        }

        return 0;
    }

    strncpy(guessbuffer, guess, sizeof(guessbuffer));
    guessbuffer[sizeof(guessbuffer) - 1] = '\0';

    status = pam_start("login", pw->pw_name, &conversation, &handle);
    status = (status != PAM_SUCCESS) ? status : pam_authenticate(handle, 0);
    status = (status != PAM_SUCCESS) ? status : pam_acct_mgmt(handle, 0);

    if (status != PAM_SUCCESS && errorbuf && errorbufsize > 0) {
        strncpy(errorbuf, pam_strerror(handle, status), errorbufsize);
        *eom = '\0';
    }

    pam_end(handle, status);

    for (k = guessbuffer; k < (guessbuffer + sizeof(guessbuffer)); k++) {
        *k = '\0';
    }

    return status == PAM_SUCCESS;
}
