#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <di/di.h>

#define PORT 9875
#define MAGIC_NUMBER "NETDUMP"
#define PROTOCOL_VERSION 1

#define EXIT_PROGRAM 0xFFFFFFFE
#define EJECT_DISC 1

#define RETURN_OK 0

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

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

    printf("Netdump\n");
    printf("Configuring Network... ");
    
    char localip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};
    s32 ret = if_config(localip, netmask, gateway, TRUE, 20);
    if (ret>=0) {
        printf("successful. %s\n", localip);

        int sock, csock;
        int ret;
        u32 clientlen;
        struct sockaddr_in client;
        struct sockaddr_in server;
        char temp[1026];
        char send_buf[1026];

        clientlen = sizeof(client);

        sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        if (sock != INVALID_SOCKET) {
            memset(&server, 0, sizeof(server));
            memset(&client, 0, sizeof(client));

            server.sin_family = AF_INET;
            server.sin_port = htons(PORT);
            server.sin_addr.s_addr = INADDR_ANY;
            ret = net_bind(sock, (struct sockaddr *) &server, sizeof(server));

            if (!ret) {
                if (!(ret = net_listen(sock, 5))) {
                    while (1) {
                        csock = net_accept(sock, (struct sockaddr *) &client, &clientlen);

                        if (csock < 0) {
                            printf("Error connecting socket %d !\n", csock);
                            break;
                        }
                        printf("-> %s\n", inet_ntoa(client.sin_addr));
                        memset(temp, 0, 1026);
                        ret = net_recv (csock, temp, 1024, 0);
                        int index = 0;
                        printf("Received %d bytes\n", ret);
                        if (ret < 15) {
                            printf("Packet too small !");
                            break;
                        }
                        bool correct = true;
                        for (; index < 7; index++) {
                            if (temp[index] != MAGIC_NUMBER[index]) {
                                printf("Wrong Magic Number !\n");
                                correct = false;
                                break;
                            }
                        }
                        if (!correct) {
                            break;
                        }
                        u32 protocol_v = (temp[index++] << 24) | (temp[index++] << 16) | (temp[index++] << 8) | temp[index++];
                        if (protocol_v != PROTOCOL_VERSION) {
                            printf("Protol Version Mismatch !\n");
                            break;
                        }
                        u32 command = (temp[index++] << 24) | (temp[index++] << 16) | (temp[index++] << 8) | temp[index++];
                        bool exit = false;
                        switch (command) {
                            case EXIT_PROGRAM:
                                printf("Exiting Program\n");
                                bypass_home_button = true;
                                exit = true;
                                memset(send_buf, 2, 1026);
                                int send_index = 0;
                                for (; send_index < strlen(MAGIC_NUMBER); send_index++) {
                                    send_buf[send_index] = MAGIC_NUMBER[send_index];
                                }
                                send_buf[send_index++] = (PROTOCOL_VERSION & 0xFF000000) >> 24;
                                send_buf[send_index++] = (PROTOCOL_VERSION & 0x00FF0000) >> 16;
                                send_buf[send_index++] = (PROTOCOL_VERSION & 0x0000FF00) >> 8;
                                send_buf[send_index++] = PROTOCOL_VERSION & 0x000000FF;
                                send_buf[send_index++] = (RETURN_OK & 0xFF000000) >> 24;
                                send_buf[send_index++] = (RETURN_OK & 0x00FF0000) >> 16;
                                send_buf[send_index++] = (RETURN_OK & 0x0000FF00) >> 8;
                                send_buf[send_index++] = RETURN_OK & 0x000000FF;
                                net_send(csock, send_buf, send_index, 0);
                                net_close(csock);
                                break;
                            case EJECT_DISC:
                                printf("Ejecting disc\n");
                                if (DI_Eject() == 0) {
                                    printf("Ejected Properly\n");
                                } else {
                                    printf("Couldn't eject disc !\n");
                                }
                                break;
                            default:
                                printf("Unknown Command\n");
                        }
                        printf("------------------\n");
                        if (exit) {
                            break;
                        }
                    }
                } else {
                    printf("Error %d while listening !", ret);
                }
                
            } else {
                printf("Error %d while binding socket !\n", ret);
            }
        } else {
            printf("Failed to open socket.\n");
        }
    } else {
        printf("failed.\n");
    }

    if (!bypass_home_button) {
        printf("Press HOME to exit\n");
        while(1) {
            WPAD_ScanPads();
            u32 pressed = WPAD_ButtonsDown(0);
            if ( pressed & WPAD_BUTTON_HOME ) exit(0);
            VIDEO_WaitVSync();
        }
    }

    return 0;
}
