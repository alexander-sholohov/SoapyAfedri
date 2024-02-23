#include "portable_utils.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <errno.h>
#include <string.h>
#endif

// from https://handsonnetworkprogramming.com/articles/socket-error-message-text/

const char *get_error_text()
{
#if defined(_WIN32)

    static char message[256] = {0};
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, WSAGetLastError(), 0, message, 256, 0);
    char *nl = strrchr(message, '\n');
    if (nl)
        *nl = 0;
    return message;

#else

    return strerror(errno);

#endif
}
