#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <shellapi.h>
#endif

int main() {
    const char* url = "https://youtu.be/dQw4w9WgXcQ?si=pEn7ZKNDJMtyLy5L";

    while (1) {
        #if defined(_WIN32) || defined(_WIN64)
            ShellExecuteA(0, "open", url, 0, 0, SW_SHOWNORMAL);

        #elif defined(__APPLE__)
            char command[256];
            snprintf(command, sizeof(command), "open %s", url);
            system(command);

        #elif defined(__linux__)
            char command[256];
            snprintf(command, sizeof(command), "xdg-open %s", url);
            system(command);
        #endif
    }
    return 0;

}
