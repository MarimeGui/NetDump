#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>

#define PORT 25565

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void *initialize() {
    void *xfb;
    VIDEO_Init();
    WPAD_Init();
    rmode = VIDEO_GetPreferredMode(MULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitForVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    return xfb;
}


int main(int argc, char **argv) {

    xfb = initialize();

    printf("Netdump\n");
    printf("Configuring Network... ");
    
    char localip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};
    ret = if_config(localip, netmask, gateway, TRUE, 20);
    if (ret>=0) {
        printf("successful. %s\n", localip);

        int sock, csock;
        int ret;
        u32 clientlen;
        struct sockaddr_in client;
        struct sockaddr_in server;
        char temp[1026];
        static int hits=0;

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
                while (1) {
                    csock = net_accept(sock, (struct sockaddr *) &client, &clientlen);

                    if (csock < 0) {
                        printf("Error connecting socket %d !\n", csock);
                        while(1);
                    }

                    
                }
            } else {
                printf("Error %d while listening !\n", ret);
            }
        } else {
            printf("Failed to open socket.\n");
        }
    } else {
        printf("failed.\n");
    }

    // Waits for button input before exiting
    while(1) {
        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);
        if ( pressed & WPAD_BUTTON_HOME ) exit(0);
        VIDEO_WaitVSync();
    }

    return 0;
}
