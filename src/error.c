

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "error.h"


static char *err_to_str(unsigned int code) {
  char *b[32]; /* a little overkill never hurt */
  memset((char *) b, 0x00, 32);
  sprintf((char *) b, "0x%08x", code);
  return g_strdup((char *)b);
}


#define CASE(val, str) \
case val: \
  m = str; \
  break;


char* mwError(unsigned int code) {
  const char *m;

  switch(code) {

    /* 8.3.1.1 General error/success codes */
    CASE(ERR_SUCCESS, "Success");
    CASE(ERR_FAILURE, "General failure");
    CASE(ERR_REQUEST_DELAY, "Request delayed");
    CASE(ERR_REQUEST_INVALID, "Request is invalid");
    CASE(ERR_NO_USER, "User is not online");

    /* 8.3.1.3 Client error codes */
    CASE(ERR_CLIENT_USER_GONE, "User not present");
    CASE(ERR_CLIENT_USER_DND, "User is in Do Not Disturb mode");
    CASE(ERR_CLIENT_USER_ELSEWHERE, "Already logged in elsewhere");

    /* 8.3.1.2 Connection/disconnection errors */
    CASE(CONNECTION_BROKEN, "Connection broken");
    CASE(CONNECTION_ABORTED, "Connection aborted");
    CASE(CONNECTION_REFUSED, "Connection refused");
    CASE(CONNECTION_RESET, "Connection reset");
    CASE(CONNECTION_TIMED, "Connection timed out");
    CASE(CONNECTION_CLOSED, "Connection closed");
    CASE(INCORRECT_LOGIN, "Incorrect Username/Password");
    CASE(VERIFICATION_DOWN, "Login verification down or unavailable");

    /* 8.3.1.4 IM error codes */
    CASE(ERR_IM_COULDNT_REGISTER, "ERR_IM_COULDNT_REGISTER");
    CASE(ERR_IM_ALREADY_REGISTERED, "ERR_IM_ALREADY_REGISTERED");
    CASE(ERR_IM_NOT_REGISTERED, "ERR_IM_NOT_REGISTERED");

  default:
    /* return now to avoid re-duplicating */
    return err_to_str(code);
  }

  return g_strdup(m);
}


#undef CASE
