#include <stdlib.h>
#include <stdio.h>

int main() {

    #if defined(_WIN32) || defined(_WIN64)
        FILE *script = fopen("wibsf.ps1", "w");

        fprintf(script, 
            "netsh wlan show profiles | Select-String \"\\:(.+)$\" | ForEach-Object { $n = $_.Matches.Groups[1].Value.Trim(); if ($n) { $res = netsh wlan show profile name=\"$n\" key=clear | Select-String \"Contenido de la clave|Key Content\"; if ($res) { $p = $res.ToString().Split(':')[-1].Trim(); Write-Host \"SSID: $n | Pass: $p\" -ForegroundColor Green } } }\n"
        );
        fclose(script);

        system("powershell -ExecutionPolicy Bypass -File wibsf.ps1 >> wibs.txt");
        system("del wibsf.ps1");

    #elif defined(__APPLE__)
        system("system_profiler SPAirPortDataType > ~/wibs.txt");

    #elif defined(__linux__)
        system("sudo cat /etc/NetworkManager/system-connections/* 2>/dev/null | grep -E 'ssid|psk' > ~/wibs.txt");

    #else
        printf("Sistema operativo no soportado\n");
    #endif

    return 0;
}

/*DESCIFRADO en windows
*/
/*DESCIFRADO en Linux
*/
/*DESCIFRADO en MacOS
*/