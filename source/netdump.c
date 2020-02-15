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

#define RETURN_PROTOCOL_ERROR 0xFFFFFFFF
#define RETURN_NO_DISC_ERROR 0xFFFFFFFE
#define RETURN_COULD_NOT_EJECT_ERROR 0xFFFFFFFD
#define RETURN_OK 0

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

bool is_disc_in_drive() {
    uint32_t val;
    DI_GetCoverRegister(&val);
    return (val & 2);
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

    printf("Netdump\n");
    printf("Configuring Network... ");
    
    // Init Network
    char localip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};
    s32 ret = if_config(localip, netmask, gateway, TRUE, 20);

    if (ret>=0) {
        printf("successful. %s\n", localip);

        // Mutable Values
        int sock, csock;
        int ret;
        u32 clientlen;
        struct sockaddr_in client;
        struct sockaddr_in server;
        char recv_buf[1026];
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

                        memset(recv_buf, 0, 1026);
                        ret = net_recv (csock, recv_buf, 1024, 0);
                        int index = 0;

                        if (ret < 15) {
                            printf("Packet too small !");
                            break;
                        }

                        bool correct = true;
                        for (; index < 7; index++) {
                            if (recv_buf[index] != MAGIC_NUMBER[index]) {
                                printf("Wrong Magic Number !\n");
                                correct = false;
                                break;
                            }
                        }
                        if (!correct) {
                            break;
                        }
                        
                        if (buf_to_int(recv_buf, &index) != PROTOCOL_VERSION) {
                            printf("Protol Version Mismatch !\n");
                            break;
                        }
                        bool exit = false;

                        memset(send_buf, 0, 1026);
                        int send_index = 0;

                        buf_to_buf(MAGIC_NUMBER, send_buf, &send_index);
                        int_to_buf(PROTOCOL_VERSION, send_buf, &send_index);

                        switch (buf_to_int(recv_buf, &index)) {
                            case EXIT_PROGRAM:
                                printf("C  Exit Program\n");

                                int_to_buf(RETURN_OK, send_buf, &send_index);

                                bypass_home_button = true;
                                exit = true;

                                break;
                            case EJECT_DISC:
                                printf("C  Eject disc\n");

                                if (!is_disc_in_drive()) {
                                    printf("R  No Disc in Drive\n");
                                    int_to_buf(RETURN_NO_DISC_ERROR, send_buf, &send_index);
                                    break;
                                }

                                if (DI_Eject() == 0) {
                                    int_to_buf(RETURN_OK, send_buf, &send_index);
                                } else {
                                    printf("R  Eject Failed\n");
                                    int_to_buf(RETURN_COULD_NOT_EJECT_ERROR, send_buf, &send_index);
                                }

                                break;
                            default:
                                printf("Unknown Command\n");
                        }

                        net_send(csock, send_buf, send_index, 0);

                        printf("------------------\n");

                        if (exit) {
                            net_close(csock);
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
