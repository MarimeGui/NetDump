#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <di/di.h>
#include <ogc/system.h>
#include "cleanrip/main_exports.h"
#include "cleanrip/gc_dvd_exports.h"

#define PORT 9875
#define PROTOCOL_VERSION 1

#define MAGIC_NUMBER "NETDUMP"

#define DISCONNECT 0xFFFFFFFF
#define EXIT_PROGRAM 0xFFFFFFFE
#define SHUTDOWN 0xFFFFFFFD
#define EJECT_DISC 1
#define DISC_INFO 2
#define DUMP_BCA 3
#define DUMP_GAME 4

#define RETURN_PROTOCOL_ERROR 0xFFFFFFFF
#define RETURN_NO_DISC_ERROR 0xFFFFFFFE
#define RETURN_COULD_NOT_EJECT_ERROR 0xFFFFFFFD
#define RETURN_OK 0
#define RETURN_DISC_INFO 1
#define RETURN_BCA 2
#define RETURN_GAME 3

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

// Writes an int to a buffer
void int_to_buf(int value, char *buf, int *index) {
    buf[(*index)++] = (value & 0xFF000000) >> 24;
    buf[(*index)++] = (value & 0x00FF0000) >> 16;
    buf[(*index)++] = (value & 0x0000FF00) >> 8;
    buf[(*index)++] = value & 0x000000FF;
}

// Reads int from buffer
int buf_to_int(char *buf, int *index) {
    return (buf[(*index)++] << 24) | (buf[(*index)++] << 16) | (buf[(*index)++] << 8) | buf[(*index)++];
}

void buf_to_buf(char in_buf[], char *out_buf, int *index) {
    int other_index = 0;
    while (other_index < strlen(in_buf)) {
        out_buf[*index] = in_buf[other_index];
        (*index)++;
        other_index++;
    }
}

bool check_buf(char ref_buf[], char *to_check, int *index) {
    for (; *index < strlen(ref_buf); (*index)++) {
        if (to_check[*index] != ref_buf[*index]) {
            return false;
        }
    }
    return true;
}

bool is_disc_in_drive() {
    uint32_t val;
    DI_GetCoverRegister(&val);
    return (val & 2);
}

void wait_for_button_exit() {
    printf("Press HOME to exit\n");
    while(1) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        if ( pressed & WPAD_BUTTON_HOME ) exit(0);
        VIDEO_WaitVSync();
    }
}

// Init various things
void *initialize() {
    void *xfb;
    DI_Init();
    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    return xfb;
}

int main(int argc, char **argv) {
    xfb = initialize();

    bool bypass_home_button = false;
    bool shutdown = false;

    printf("Netdump\n");
    printf("Performing Checks...");

    if (!have_ahbprot()) {
        printf(" failed.\n");
        printf("AHBPROT check failed.\n");
        wait_for_button_exit();
        return 0;
    }

    s32 ios_version = IOS_GetVersion();
    int ios_58_exists = find_ios(58);
    if (ios_58_exists && (ios_version != 58)) {
        printf(" failed.\n");
        printf("IOS 58 exists but isn't in use.\n");
        wait_for_button_exit();
        return 0;
    }
    if (!ios_58_exists) {
        printf(" failed.\n");
        printf("IOS 58 does not exist.\n");
        wait_for_button_exit();
        return 0;
    }

    printf(" OK.\n");

    printf("Configuring Network... ");
    
    char localip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};
    s32 ret = if_config(localip, netmask, gateway, TRUE, 20);

    if (if_config(localip, netmask, gateway, TRUE, 20) < 0) {
        printf(" fail.\n");
        wait_for_button_exit();
        return 0;
    }
    
    printf("successful. Wii IP: %s\n", localip);

    // Mutable Values
    int sock, csock;
    u32 clientlen;
    struct sockaddr_in client;
    struct sockaddr_in server;
    char recv_buf[1026];
    char send_buf[1026];

    clientlen = sizeof(client);

    sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    if (sock == INVALID_SOCKET) {
        printf("Failed to open socket.\n");
        wait_for_button_exit();
        return 0;
    }

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (net_bind(sock, (struct sockaddr *) &server, sizeof(server))) {
        printf("Error while binding socket.\n");
        wait_for_button_exit();
        return 0;
    }

    if (net_listen(sock, 5)) {
        printf("Error while listening.\n");
        wait_for_button_exit();
        return 0;
    }

    bool program_exit = false;

    // New clients loop
    while (1) {
        printf("Waiting for client.\n");

        csock = net_accept(sock, (struct sockaddr *) &client, &clientlen);

        if (csock < 0) {
            printf("Error connecting to client.\n");
            continue;
        }

        printf("-> %s\n", inet_ntoa(client.sin_addr));

        bool disconnect = false;

        // New command loop
        while (1) {
            memset(recv_buf, 0, 1026);
            ret = net_recv(csock, recv_buf, 1024, 0);
            int recv_index = 0;

            memset(send_buf, 0, 1026);
            int send_index = 0;

            buf_to_buf(MAGIC_NUMBER, send_buf, &send_index);
            int_to_buf(PROTOCOL_VERSION, send_buf, &send_index);

            if (ret < 15) {
                printf("Packet too small (%d) !\n", ret);
                break;
            }

            if (!check_buf(MAGIC_NUMBER, recv_buf, &recv_index)) {
                printf("Wrong Magic Number !\n");
                break;
            }

            if (buf_to_int(recv_buf, &recv_index) != PROTOCOL_VERSION) {
                printf("Protol Version Mismatch !\n");
                int_to_buf(RETURN_PROTOCOL_ERROR, send_buf, &send_index);
                net_send(csock, send_buf, send_index, 0);
                break;
            }

            // Check command
            switch (buf_to_int(recv_buf, &recv_index)) {
                case DISCONNECT:
                    printf("C  Disconnect\n");

                    int_to_buf(RETURN_OK, send_buf, &send_index);

                    disconnect = true;

                    break;
                case EXIT_PROGRAM:
                    printf("C  Exit Program\n");

                    int_to_buf(RETURN_OK, send_buf, &send_index);

                    bypass_home_button = true;
                    disconnect = true;
                    program_exit = true;

                    break;
                case SHUTDOWN:
                    printf("C Shtudown\n");

                    int_to_buf(RETURN_OK, send_buf, &send_index);

                    bypass_home_button = true;
                    disconnect = true;
                    program_exit = true;
                    shutdown = true;

                    break;
                case EJECT_DISC:
                    printf("C  Eject disc\n");

                    if (!is_disc_in_drive()) {
                        printf("R  No Disc in Drive\n");
                        int_to_buf(RETURN_NO_DISC_ERROR, send_buf, &send_index);
                        break;
                    }

                    if (DI_Eject() == 0) {
                        printf("R  OK\n");
                        int_to_buf(RETURN_OK, send_buf, &send_index);
                    } else {
                        printf("R  Eject Failed\n");
                        int_to_buf(RETURN_COULD_NOT_EJECT_ERROR, send_buf, &send_index);
                    }

                    break;
                case DUMP_BCA:
                    printf("C  Dump BCA\n");

                    if (init_dvd() == NO_DISC) {
                        printf("R  No Disc in Drive\n");
                        int_to_buf(RETURN_NO_DISC_ERROR, send_buf, &send_index);
                        break;
                    }

                    break;
                default:
                    printf("Unknown Command\n");
            }

            net_send(csock, send_buf, send_index, 0);

            if (disconnect) {
                break;
            }
        }

        printf("------------------\n");

        net_close(csock);

        if (program_exit) {
            net_close(sock);
            break;
        }

    }

    if (!bypass_home_button) {
        wait_for_button_exit();
    }

    if (shutdown) {
        SYS_ResetSystem(SYS_POWEROFF, 0, 0);
    }

    return 0;
}
